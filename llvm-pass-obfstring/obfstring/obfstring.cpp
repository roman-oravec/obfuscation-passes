// Based on https://github.com/tsarpaul/llvm-string-obfuscator

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/IRMover.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <typeinfo>

using namespace std;
using namespace llvm;


namespace {
class GlobalString {
public:
  GlobalVariable *Glob;
  unsigned int index;
  int type;
  int string_length;
  static const int SIMPLE_STRING_TYPE = 1;
  static const int STRUCT_STRING_TYPE = 2;

  GlobalString(GlobalVariable *Glob, int length)
      : Glob(Glob), index(-1), string_length(length), type(SIMPLE_STRING_TYPE) {
  }
  GlobalString(GlobalVariable *Glob, unsigned int index, int length)
      : Glob(Glob), index(index), string_length(length),
        type(STRUCT_STRING_TYPE) {}
};

Function *createDecodeStubFunc(Module &M, vector<GlobalString *> &GlobalStrings,
                               Function *DecodeFunc) {
  auto &Ctx = M.getContext();
  // Add DecodeStub function
  FunctionCallee DecodeStubCallee =
      M.getOrInsertFunction("decode_stub",
                            /*ret*/ Type::getVoidTy(Ctx));
  Function *DecodeStubFunc = cast<Function>(DecodeStubCallee.getCallee());

  DecodeStubFunc->removeFnAttr(llvm::Attribute::OptimizeNone);
  DecodeStubFunc->removeFnAttr(llvm::Attribute::NoInline);
  DecodeStubFunc->addFnAttr(llvm::Attribute::AlwaysInline);
  // Set internal linkage to avoid naming conflicts
  DecodeStubFunc->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
  DecodeStubFunc->setCallingConv(CallingConv::C);

  // Create entry block
  BasicBlock *BB = BasicBlock::Create(Ctx, "entry", DecodeStubFunc);
  IRBuilder<> Builder(BB);

  // Add calls to decode every encoded global
  for (GlobalString *GlobString : GlobalStrings) {
    if (GlobString->type == GlobString->SIMPLE_STRING_TYPE) {
      auto *StrPtr =
          Builder.CreatePointerCast(GlobString->Glob, Type::getInt8PtrTy(Ctx));
      Builder.CreateCall(DecodeFunc, {StrPtr}); // CHANGE: removed le
    } else if (GlobString->type == GlobString->STRUCT_STRING_TYPE) {
      auto *String =
          Builder.CreateStructGEP(GlobString->Glob, GlobString->index);
      auto *StrPtr = Builder.CreatePointerCast(String, Type::getInt8PtrTy(Ctx));
      Builder.CreateCall(DecodeFunc, {StrPtr}); // CHANGE: removed le
    }
  }
  Builder.CreateRetVoid();

  return DecodeStubFunc;
}

Function *createDecodeFunc(Module &M) {
  auto &Ctx = M.getContext();
  llvm::SMDiagnostic mod_err;

  // TODO: use flag instead of hardcoded path
  unique_ptr<Module> DecModule = parseIRFile("codec.bc", mod_err, Ctx);
  if (!DecModule)
    mod_err.print("Unable to parse BC file", errs());
  // Check if we have an decoding function
  assert(DecModule->getFunction("decode") && "Decoding function not found");

  std::vector<llvm::GlobalValue *> imports({DecModule->getFunction("decode")});

  auto err = IRMover(M).move(
      move(DecModule), imports,
      [](llvm::GlobalValue &GV, llvm::IRMover::ValueAdder Add) {},
      /*IsPerformingImport=*/false);
  handleAllErrors(std::move(err), [&](llvm::ErrorInfoBase &eib) {
    errs() << "Could not import decoder function: " << eib.message();
  });

  // Set attributes to the new decoder
  auto DecodeFunc = M.getFunction("decode");

  DecodeFunc->removeFnAttr(llvm::Attribute::OptimizeNone);
  DecodeFunc->removeFnAttr(llvm::Attribute::NoInline);
  DecodeFunc->addFnAttr(llvm::Attribute::AlwaysInline);
  // Set internal linkage to avoid naming conflicts
  DecodeFunc->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
  DecodeFunc->setCallingConv(CallingConv::C);

  return DecodeFunc;
}

void createDecodeStubBlock(Function *F, Function *DecodeStubFunc) {
  auto &Ctx = F->getContext();
  BasicBlock &EntryBlock = F->getEntryBlock();

  // Create new block
  BasicBlock *NewBB = BasicBlock::Create(Ctx, "DecodeStub",
                                         EntryBlock.getParent(), &EntryBlock);
  IRBuilder<> Builder(NewBB);

  // Call stub func instruction
  Builder.CreateCall(DecodeStubFunc);
  // Jump to original entry block
  Builder.CreateBr(&EntryBlock);
}

Constant *EncodeString(std::unique_ptr<ExecutionEngine> &engine, ConstantDataArray *CDA, LLVMContext &Ctx){
  std::string encStr = CDA->getAsCString().str();
  std::vector<llvm::GenericValue> args(1);
  args[0].PointerVal = (void *)encStr.c_str();
  auto encFun = engine->FindFunctionNamed("encode");
  engine->runFunction(encFun, args);
  return ConstantDataArray::getString(Ctx, encStr);
}

vector<GlobalString *> encodeGlobalStrings(Module &M) {
  vector<GlobalString *> GlobalStrings;
  std::unique_ptr<llvm::ExecutionEngine> engine;
  auto &Ctx = M.getContext();
  
  // Get codec
  llvm::SMDiagnostic mod_err;
  // TODO: use flag instead of hardcoded path
  unique_ptr<Module> codec = parseIRFile("codec.bc", mod_err, Ctx);
  if (!codec)
    mod_err.print("Unable to parse BC file", errs());
  // Check if we have an decoding function
  assert(codec->getFunction("encode") && "Encoding function not found");

  // Initialize exec engine
  std::string eng_err;
  engine.reset(llvm::EngineBuilder(llvm::CloneModule(*codec))
                   .setEngineKind(llvm::EngineKind::Interpreter)
                   .setErrorStr(&eng_err)
                   .create());
  assert(engine && "Failed to initialize execution engine.");

  // Encode all global strings
  for (GlobalVariable &Glob : M.globals()) {
    // Ignore external globals & uninitialized globals.
    if (!Glob.hasInitializer() || Glob.hasExternalLinkage())
      continue;

    // Unwrap the global variable to receive its value
    Constant *Initializer = Glob.getInitializer();

    // Check if its a string
    if (isa<ConstantDataArray>(Initializer)) {
      auto CDA = cast<ConstantDataArray>(Initializer);
      if (!CDA->isString())
        continue;

      auto NewConst = EncodeString(engine, CDA, Ctx);

      // Overwrite the global value
      Glob.setInitializer(NewConst);

      GlobalStrings.push_back(new GlobalString(&Glob, CDA->getAsCString().size()));
      Glob.setConstant(false);
    } else if (isa<ConstantStruct>(Initializer)) {
      // Handle structs
      auto CS = cast<ConstantStruct>(Initializer);
      if (Initializer->getNumOperands() > 1)
        continue; // TODO: Fix bug when removing this: "Constant not found in
                  // constant table!"
      for (unsigned int i = 0; i < Initializer->getNumOperands(); i++) {
        auto CDA = dyn_cast<ConstantDataArray>(CS->getOperand(i));
        if (!CDA || !CDA->isString())
          continue;

        // Create encoded string variable
        Constant *NewConst = EncodeString(engine, CDA, Ctx);

        // Overwrite the struct member
        CS->setOperand(i, NewConst);

        GlobalStrings.push_back(new GlobalString(&Glob, i, CDA->getAsString().size()));
        Glob.setConstant(false);
      }
    }
  }

  return GlobalStrings;
}

struct ObfStringPass : public PassInfoMixin<ObfStringPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // TODO: detect entry function if main not present
    Function *MainFunc = M.getFunction("main");
    assert(MainFunc);

    // Transform the strings
    auto GlobalStrings = encodeGlobalStrings(M);

    // Inject functions
    Function *DecodeFunc = createDecodeFunc(M);
    Function *DecodeStub = createDecodeStubFunc(M, GlobalStrings, DecodeFunc);

    // Inject a call to DecodeStub from main
    createDecodeStubBlock(MainFunc, DecodeStub);

    return PreservedAnalyses::all();
  };
};
} // end anonymous namespace

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ObfStringPass", "v0.1",
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "obfstring") {
                    MPM.addPass(ObfStringPass());
                    return true;
                  }
                  return false;
                });
          }};
}

/* char ObfStringPass::ID = 0;

// Register the pass 
static RegisterPass<ObfStringPass> X("obfstring", "Obfuscate strings"); */
