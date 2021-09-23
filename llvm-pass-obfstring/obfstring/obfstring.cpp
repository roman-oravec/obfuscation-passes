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
#include <typeinfo>

#define STRING_ENCRYPTION_KEY "S5zeMZ68*K7ddKwiOWTt"

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

// TODO: use llvm::parseIRFile()
Function *createDecodeFunc(Module &M) {
  auto &Ctx = M.getContext();
  llvm::SMDiagnostic mod_err;

  // TODO: use flag instead of hardcoded path
  unique_ptr<Module> DecModule = parseIRFile("decode.bc", mod_err, Ctx);
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

// TODO: import the function instead
char *EncodeString(const char *Data, unsigned int Length) {
  // Encode string
  unsigned char *NewData =
      (unsigned char *)calloc(Length, sizeof(unsigned char));
  int i = 0;
  for (; Data[i] != 0; ++i) {
    unsigned key_idx = i % (sizeof(STRING_ENCRYPTION_KEY) - 1);
    if (Data[i] == 0xff) {
      NewData[i] = '.';
    }
    NewData[i] = Data[i] ^ STRING_ENCRYPTION_KEY[key_idx];
    if (NewData[i] == 0) {
      NewData[i] = STRING_ENCRYPTION_KEY[key_idx] ^ 0xff;
    }
  }
  return reinterpret_cast<char *>(NewData);
}

vector<GlobalString *> encodeGlobalStrings(Module &M) {
  vector<GlobalString *> GlobalStrings;
  auto &Ctx = M.getContext();

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

      // Extract raw string
      StringRef StrVal = CDA->getAsString();
      const char *Data = StrVal.begin();
      const int Size = StrVal.size();

      // Create encoded string variable. Constants are immutable so we must
      // override with a new Constant.
      char *NewData = EncodeString(Data, Size);
      Constant *NewConst =
          ConstantDataArray::getString(Ctx, StringRef(NewData, Size), false);

      // Overwrite the global value
      Glob.setInitializer(NewConst);

      GlobalStrings.push_back(new GlobalString(&Glob, Size));
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

        // Extract raw string
        StringRef StrVal = CDA->getAsString();
        const char *Data = StrVal.begin();
        const int Size = StrVal.size();

        // Create encoded string variable
        char *NewData = EncodeString(Data, Size);
        Constant *NewConst =
            ConstantDataArray::getString(Ctx, StringRef(NewData, Size), false);

        // Overwrite the struct member
        CS->setOperand(i, NewConst);

        GlobalStrings.push_back(new GlobalString(&Glob, i, Size));
        Glob.setConstant(false);
      }
    }
  }

  return GlobalStrings;
}

struct ObfStringPass : public PassInfoMixin<ObfStringPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    // Transform the strings
    auto GlobalStrings = encodeGlobalStrings(M);

    // Inject functions
    Function *DecodeFunc = createDecodeFunc(M);
    Function *DecodeStub = createDecodeStubFunc(M, GlobalStrings, DecodeFunc);

    // Inject a call to DecodeStub from main
    Function *MainFunc = M.getFunction("main");
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
