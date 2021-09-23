#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/IRMover.h"
#include <typeinfo>

#define STRING_ENCRYPTION_KEY "S5zeMZ68*K7ddKwiOWTt"

using namespace std;
using namespace llvm;

namespace
{
	class GlobalString
	{
	public:
		GlobalVariable *Glob;
		unsigned int index;
		int type;
		int string_length;
		static const int SIMPLE_STRING_TYPE = 1;
		static const int STRUCT_STRING_TYPE = 2;

		GlobalString(GlobalVariable *Glob, int length) : Glob(Glob), index(-1), string_length(length), type(SIMPLE_STRING_TYPE) {}
		GlobalString(GlobalVariable *Glob, unsigned int index, int length) : Glob(Glob), index(index), string_length(length), type(STRUCT_STRING_TYPE) {}
	};

	Function *createDecodeStubFunc(Module &M, vector<GlobalString *> &GlobalStrings, Function *DecodeFunc)
	{
		auto &Ctx = M.getContext();
		// Add DecodeStub function
		FunctionCallee DecodeStubCallee = M.getOrInsertFunction("decode_stub",
																/*ret*/ Type::getVoidTy(Ctx));
		Function *DecodeStubFunc = cast<Function>(DecodeStubCallee.getCallee());
		DecodeStubFunc->setCallingConv(CallingConv::C);

		// Create entry block
		BasicBlock *BB = BasicBlock::Create(Ctx, "entry", DecodeStubFunc);
		IRBuilder<> Builder(BB);

		// Add calls to decode every encoded global
		for (GlobalString *GlobString : GlobalStrings)
		{
			if (GlobString->type == GlobString->SIMPLE_STRING_TYPE)
			{
				auto *StrPtr = Builder.CreatePointerCast(GlobString->Glob, Type::getInt8PtrTy(Ctx/* , 8 */));
				/* llvm::Value *le = llvm::ConstantInt::get(
					llvm::IntegerType::getInt32Ty(Ctx), GlobString->string_length); */
				Builder.CreateCall(DecodeFunc, {StrPtr}); //CHANGE: removed le
			}
			else if (GlobString->type == GlobString->STRUCT_STRING_TYPE)
			{
				auto *String = Builder.CreateStructGEP(GlobString->Glob, GlobString->index);
				auto *StrPtr = Builder.CreatePointerCast(String, Type::getInt8PtrTy(Ctx/* , 8 */));
				/* llvm::Value *le = llvm::ConstantInt::get(
					llvm::IntegerType::getInt32Ty(Ctx), GlobString->string_length); */
				Builder.CreateCall(DecodeFunc, {StrPtr}); //CHANGE: removed le
			}
		}
		Builder.CreateRetVoid();

		return DecodeStubFunc;
	}

	//TODO: use llvm::parseIRFile()
	Function *createDecodeFunc(Module &M)
	{
		auto &Ctx = M.getContext();
		llvm::SMDiagnostic mod_err;
		// TODO: use flag instead of hardcoded path
		unique_ptr<Module> DecModule = parseIRFile("decode.bc", mod_err, Ctx);
		if (!DecModule)
			//assert(DecModule && "Unable to parse bitcode file ");
			mod_err.print("Unable to parse BC file", errs());
		// Check if we have an decoding function
		assert(DecModule->getFunction("decode") && "Decoding function not found");

		std::vector<llvm::GlobalValue *> imports({DecModule->getFunction("decode")});

		auto err = IRMover(M).move(
			move(DecModule), imports,
			[](llvm::GlobalValue &GV, llvm::IRMover::ValueAdder Add) {},
			/*IsPerformingImport=*/false);
		handleAllErrors(std::move(err), [&](llvm::ErrorInfoBase &eib)
						{ errs() << "Could not import decoder function: " << eib.message(); });
		// Set attributes to the new decoder
		auto DecodeFunc = M.getFunction("decode");

		/* unsigned Idx = 0;
		for (auto &Arg : DecodeFunc->args()) */
			

		DecodeFunc->removeFnAttr(llvm::Attribute::OptimizeNone);
		DecodeFunc->removeFnAttr(llvm::Attribute::NoInline);
		DecodeFunc->addFnAttr(llvm::Attribute::AlwaysInline);
		// Set internal linkage to avoid naming conflicts
		DecodeFunc->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);

		//FunctionCallee DecodeFuncCallee = M.getOrInsertFunction("decode",
		///*ret*/			Type::getVoidTy(Ctx),
		///*args*/		Type::getInt8PtrTy(Ctx, 8),
		//			Type::getInt32Ty(Ctx));

		//Function *DecodeFunc = cast<Function>(DecodeFuncCallee.getCallee());
		DecodeFunc->setCallingConv(CallingConv::C);

		/* // Name DecodeFunc arguments
	Function::arg_iterator Args = DecodeFunc->arg_begin();
	Value *var0 = Args++;
	Value *var1 = Args++;
	
	BasicBlock *bb2 = BasicBlock::Create(Ctx, "", DecodeFunc);
	
	IRBuilder<> *Builder2 = new IRBuilder<>(bb2);
	auto *var3 = Builder2->CreateICmpNE(var0, Constant::getNullValue(Type::getInt8PtrTy(Ctx, 8)),  "var3");
	auto *var4 = Builder2->CreateICmpSGT(var1, ConstantInt::get(IntegerType::get(Ctx, 32), 0));
	auto *var5 = Builder2->CreateAnd(var4, var3, "var5");
	BasicBlock *bb6 = BasicBlock::Create(Ctx, "bb6", DecodeFunc);
	BasicBlock *bb14 = BasicBlock::Create(Ctx, "bb14", DecodeFunc);
	Builder2->CreateCondBr(var5, bb6, bb14);

	IRBuilder<> *Builder6 = new IRBuilder<>(bb6);
	PHINode *phi_var7 = Builder6->CreatePHI(Type::getInt8PtrTy(Ctx, 8), 2, "var7");
	PHINode *phi_var8 = Builder6->CreatePHI(Type::getInt32Ty(Ctx), 2, "var8");
	auto *var9 = Builder6->CreateNSWAdd(phi_var8, ConstantInt::getSigned(IntegerType::get(Ctx, 32), -1), "var9");
	auto *var10 = Builder6->CreateGEP(phi_var7, ConstantInt::get(IntegerType::get(Ctx, 64), 1), "var10");

	auto *var11 = Builder6->CreateLoad(phi_var7, "var2");
		
	auto *var12 = Builder6->CreateAdd(var11, ConstantInt::getSigned(IntegerType::get(Ctx, 8), -1), "var12");
		
	Builder6->CreateStore(var12, phi_var7);
	auto *var13 = Builder6->CreateICmpSGT(phi_var8, ConstantInt::get(IntegerType::get(Ctx, 32), 1), "var13");

	Builder6->CreateCondBr(var13, bb6, bb14);
	
	IRBuilder<> *Builder14 = new IRBuilder<>(bb14);
	Builder14->CreateRetVoid();
 //%7 = phi i8* [ %10, %6 ], [ %0, %2 ]
	phi_var7->addIncoming(var0, bb2);
	phi_var7->addIncoming(var10, bb6);
//   %8 = phi i32 [ %9, %6 ], [ %1, %2 ]
	phi_var8->addIncoming(var1, bb2);
	phi_var8->addIncoming(var9, bb6); */

		return DecodeFunc;
	}

	void createDecodeStubBlock(Function *F, Function *DecodeStubFunc)
	{
		auto &Ctx = F->getContext();
		BasicBlock &EntryBlock = F->getEntryBlock();

		// Create new block
		BasicBlock *NewBB = BasicBlock::Create(Ctx, "DecodeStub", EntryBlock.getParent(), &EntryBlock);
		IRBuilder<> Builder(NewBB);

		// Call stub func instruction
		Builder.CreateCall(DecodeStubFunc);
		// Jump to original entry block
		Builder.CreateBr(&EntryBlock);
	}

	// used for debug, remove later
	void PrintDecoded(const unsigned char *str, unsigned int len)
	{
		int i = 0;
		unsigned char *decStr = (unsigned char *)calloc(len, sizeof(unsigned char));
		for (; str[i] != 0; ++i)
		{
			unsigned key_idx = i % (sizeof(STRING_ENCRYPTION_KEY) - 1);
			decStr[i] = str[i] ^ STRING_ENCRYPTION_KEY[key_idx];
			if (decStr[i] == 0xff)
			{
				decStr[i] = STRING_ENCRYPTION_KEY[key_idx];
			}
		}
		errs() << format(" \nDecoded string:%s\n", reinterpret_cast<char *>(decStr));
		return;
	}

	char *EncodeString(const char *Data, unsigned int Length)
	{
		// Encode string
		unsigned char *NewData = (unsigned char *)calloc(Length, sizeof(unsigned char));
		int i = 0;
		for (; Data[i] != 0; ++i)
		{
			//errs() << format("%c\t%X\t%X\n", Data[i],Data[i], NewData[i]);
			unsigned key_idx = i % (sizeof(STRING_ENCRYPTION_KEY) - 1);
			if (Data[i] == 0xff)
			{
				// value 0xff is banned in the string because it is used as
				// a placeholder for 0 in encrypted result
				NewData[i] = '.';
			}
			NewData[i] = Data[i] ^ STRING_ENCRYPTION_KEY[key_idx];
			if (NewData[i] == 0)
			{
				NewData[i] = STRING_ENCRYPTION_KEY[key_idx] ^ 0xff;
			}
		}
		//errs() << format("%c\t%X\t%X\n", Data[i++],Data[i++], NewData[i++]);
		//PrintDecoded(NewData, Length);
		//errs() << "---------------------------------\n";
		return reinterpret_cast<char *>(NewData);
	}


	vector<GlobalString *> encodeGlobalStrings(Module &M)
	{
		vector<GlobalString *> GlobalStrings;
		auto &Ctx = M.getContext();

		// Encode all global strings
		for (GlobalVariable &Glob : M.globals())
		{
			// Ignore external globals & uninitialized globals.
			if (!Glob.hasInitializer() || Glob.hasExternalLinkage())
				continue;

			// Unwrap the global variable to receive its value
			Constant *Initializer = Glob.getInitializer();

			// Check if its a string
			if (isa<ConstantDataArray>(Initializer))
			{
				auto CDA = cast<ConstantDataArray>(Initializer);
				if (!CDA->isString())
					continue;

				// Extract raw string
				StringRef StrVal = CDA->getAsString();
				const char *Data = StrVal.begin();
				const int Size = StrVal.size();

				// Create encoded string variable. Constants are immutable so we must override with a new Constant.
				char *NewData = EncodeString(Data, Size);
				Constant *NewConst = ConstantDataArray::getString(Ctx, StringRef(NewData, Size), false);

				// Overwrite the global value
				Glob.setInitializer(NewConst);

				GlobalStrings.push_back(new GlobalString(&Glob, Size));
				Glob.setConstant(false);
			}
			else if (isa<ConstantStruct>(Initializer))
			{
				// Handle structs
				auto CS = cast<ConstantStruct>(Initializer);
				if (Initializer->getNumOperands() > 1)
					continue; // TODO: Fix bug when removing this: "Constant not found in constant table!"
				for (unsigned int i = 0; i < Initializer->getNumOperands(); i++)
				{
					auto CDA = dyn_cast<ConstantDataArray>(CS->getOperand(i));
					if (!CDA || !CDA->isString())
						continue;

					// Extract raw string
					StringRef StrVal = CDA->getAsString();
					const char *Data = StrVal.begin();
					const int Size = StrVal.size();

					// Create encoded string variable
					char *NewData = EncodeString(Data, Size);
					Constant *NewConst = ConstantDataArray::getString(Ctx, StringRef(NewData, Size), false);

					// Overwrite the struct member
					CS->setOperand(i, NewConst);

					GlobalStrings.push_back(new GlobalString(&Glob, i, Size));
					Glob.setConstant(false);
				}
			}
		}

		return GlobalStrings;
	}

	struct ObfStringPass : public PassInfoMixin<ObfStringPass>
	{
		PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM)
		{
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
llvmGetPassPluginInfo()
{
	return {
		LLVM_PLUGIN_API_VERSION, "ObfStringPass", "v0.1", [](PassBuilder &PB)
		{
			PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM,
												  ArrayRef<PassBuilder::PipelineElement>)
											   {
												   if (Name == "obfstring")
												   {
													   MPM.addPass(ObfStringPass());
													   return true;
												   }
												   return false;
											   });
		}};
}
