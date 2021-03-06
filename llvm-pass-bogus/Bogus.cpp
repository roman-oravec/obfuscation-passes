// Pass based on Obfuscator-LLVM
#define DEBUG_TYPE "bogus"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace {
struct BogusFlowPass : public FunctionPass {

  std::vector<Value *> IntegerVect;
  bool changed = false;
  static char ID;
  BogusFlowPass() : FunctionPass(ID) {}

  void Bogus(Function &F);
  void AddBogus(BasicBlock *basicBlock, Function &F);
  BasicBlock *createAltered(BasicBlock *basicBlock, const Twine &Name,
                            Function *F);
  bool doF(Module &M);
  Value *getSymOP(Module &M, Instruction *inst, Value *arg);
  Value *getSimpleOP(Module &M, Instruction *inst,
                     std::vector<Value *> &argVec);

  virtual bool runOnFunction(Function &F) {
    Bogus(F);
    doF(*F.getParent());
    return changed;
  } // runOnFunction
};  // Pass

} // namespace

void BogusFlowPass::Bogus(Function &F) {
  // Put all the function's block in a list
  std::list<BasicBlock *> basicBlocks;
  for (auto BBi = F.begin(); BBi != F.end(); ++BBi) {
    basicBlocks.push_back(&*BBi);
  }

  while (!basicBlocks.empty()) {
    BasicBlock *basicBlock = basicBlocks.front();
    AddBogus(basicBlock, F);
    basicBlocks.pop_front();
  }
}

void BogusFlowPass::AddBogus(BasicBlock *basicBlock, Function &F) {
  auto i1 = basicBlock->begin();
  if (basicBlock->getFirstNonPHIOrDbgOrLifetime()) {
    i1 = (BasicBlock::iterator)basicBlock->getFirstNonPHIOrDbgOrLifetime();
  }

  // Obfuscating Landing Pad block would result in errors
  if (isa<LandingPadInst>(i1)) {
    return;
  }
  BasicBlock *original = basicBlock->splitBasicBlock(i1, "original");
  BasicBlock *altered = createAltered(original, "copy", &F);

  // Now that all the blocks are created,
  // we modify the terminators to adjust the control flow.

  altered->getTerminator()->eraseFromParent();
  basicBlock->getTerminator()->eraseFromParent();

  // Preparing a condition..
  // For now, the condition is an always true comparaison between 2 float
  // This will be complicated after the pass (in doFinalization())
  Value *LHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
  Value *RHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
  FCmpInst *condition =
      new FCmpInst(*basicBlock, FCmpInst::FCMP_TRUE, LHS, RHS, "condition");

  // Jump to the original basic block if the condition is true or
  // to the altered block if false.
  BranchInst::Create(original, altered, (Value *)condition, basicBlock);

  // The altered block loop back on the original one.
  BranchInst::Create(original, altered);

  // The end of the original is modified to give the impression that sometimes
  // it continues in the loop, and sometimes it returns the desired value
  // (of course it's always true, so it always uses the original terminator.
  // But this will be obfuscated too ;) )

  // iterate on instruction just before the terminator of the original
  BasicBlock::iterator BBi = original->end();

  // Split at this point (we only want the terminator in the second part)
  BasicBlock *originalpart2 = original->splitBasicBlock(--BBi, "originalpart2");

  // The first part goes either to the return statement or to the begining
  // of the altered block. So we erase the terminator created when splitting.
  original->getTerminator()->eraseFromParent();

  FCmpInst *condition2 =
      new FCmpInst(*original, CmpInst::FCMP_TRUE, LHS, RHS, "condition2");
  BranchInst::Create(originalpart2, altered, (Value *)condition2, original);
}

BasicBlock *BogusFlowPass::createAltered(BasicBlock *basicBlock,
                                         const Twine &Name = "altered",
                                         Function *F = 0) {
  ValueToValueMapTy VMap;
  BasicBlock *altered = llvm::CloneBasicBlock(basicBlock, VMap, Name, F);
  auto ji = basicBlock->begin();
  // Remap operands
  for (auto BBi = altered->begin(), e = altered->end(); BBi != e; ++BBi) {
    // Loop over the operands of the instruction
    for (User::op_iterator opi = BBi->op_begin(), ope = BBi->op_end();
         opi != ope; ++opi) {
      // get the value for the operand
      Value *v = MapValue(*opi, VMap, RF_None, 0);
      if (v != 0) {
        *opi = v;
      }
    }

    // Remap phi nodes' incoming blocks.
    if (PHINode *pn = dyn_cast<PHINode>(BBi)) {
      for (unsigned j = 0, e = pn->getNumIncomingValues(); j != e; ++j) {
        Value *v = MapValue(pn->getIncomingBlock(j), VMap, RF_None, 0);
        if (v != 0) {
          pn->setIncomingBlock(j, cast<BasicBlock>(v));
        }
      }
    }
    // Remap attached metadata.
    SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
    BBi->getAllMetadata(MDs);
    // important for compiling with DWARF, using option -g.
    BBi->setDebugLoc(ji->getDebugLoc());
    ji++;
  }

  // TODO improved junk code insertion here

  return altered;
}

/* doFinalization
 *
 * Overwrite FunctionPass method to apply the transformations to the whole
 * module. This part obfuscate all the always true predicates of the module.
 * More precisely, the condition which predicate is FCMP_TRUE.
 * It also remove all the functions' basic blocks' and instructions' names.
 */
bool BogusFlowPass::doF(Module &M) {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(0, 1);
  std::vector<Instruction *> toEdit, toDelete;
  std::vector<Value *> argVec;
  Value *argValue, *symVal, *pred;
  Type *argType;

  for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi) {
    for (Function::iterator fi = mi->begin(), fe = mi->end(); fi != fe; ++fi) {
      Instruction *ti = fi->getTerminator();
      if (ti->getOpcode() == Instruction::Br) {
        BranchInst *br = (BranchInst *)(ti);
        if (br->isConditional()) {
          FCmpInst *cond = (FCmpInst *)br->getCondition();
          unsigned opcode = cond->getOpcode();
          if (opcode == Instruction::FCmp) {
            if (cond->getPredicate() == FCmpInst::FCMP_TRUE) {
              toDelete.push_back(cond);
              toEdit.push_back(ti);
            }
          }
        }
      }
    } // function loop
  }   // module loop

  // Replace all the branches found
  for (auto BBi = toEdit.begin(); BBi != toEdit.end(); ++BBi) {
    // Check if function has an integer arguments
    // and store them in argVec
    Function *F = (*BBi)->getParent()->getParent();
    for (Function::arg_iterator argIt = F->arg_begin(); argIt != F->arg_end();
         ++argIt) {
      argValue = &*argIt;
      argType = argValue->getType();
      if (argType->isIntegerTy()) {
        if (argType->getIntegerBitWidth() == 32) {
          argVec.push_back(argValue);
        }
        /* if (isa<ConstantInt>(argValue))
        symVal = argValue; */
        if (argVec.size() >= 2)
          break; // Don't need more than 2 int args
      }
    }

    // Try to construct symbolic OP using arrays
    // Use Simple OP if it fails
    // TODO use deterministic RNG
    if (!argVec.empty() && dist(rng)) {
      pred = getSymOP(M, *BBi, argVec[0]);
    } else {
      pred = getSimpleOP(M, *BBi, argVec);
    }

    // Create BranchInst with successors of original BranchInst (BBi),
    // use the opaque pred as condition
    // and insert at the end of the BB
    BranchInst::Create(((BranchInst *)*BBi)->getSuccessor(0),
                       ((BranchInst *)*BBi)->getSuccessor(1), (Value *)pred,
                       ((BranchInst *)*BBi)->getParent());
    (*BBi)->eraseFromParent(); // erase the branch
  }

  for (auto BBi = toDelete.begin(); BBi != toDelete.end(); ++BBi) {
    (*BBi)->eraseFromParent();
  }
  return true;
} // doF

Value *BogusFlowPass::getSimpleOP(Module &M, Instruction *inst,
                                  std::vector<Value *> &argVec) {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(1, INT8_MAX);
  std::uniform_int_distribution<std::mt19937::result_type> randMBA(
      1, 4); // for OP selection
  Type *i32_type = Type::getInt32Ty(M.getContext());
  IRBuilder<> Builder(inst->getParent());

  Value *x1 = ConstantInt::get(i32_type, 0);
  Value *y1 = ConstantInt::get(i32_type, 0);

  GlobalVariable *x = new GlobalVariable(
      M, i32_type, false, GlobalValue::LinkOnceAnyLinkage, (Constant *)x1, "x");
  GlobalVariable *y = new GlobalVariable(
      M, i32_type, false, GlobalValue::LinkOnceAnyLinkage, (Constant *)y1, "y");

  x->setInitializer(ConstantInt::get(i32_type, dist(rng)));
  y->setInitializer(ConstantInt::get(i32_type, dist(rng)));

  // Try to use function arguments instead of globals
  Value *opX, *opY;
  if (argVec.size() > 1) {
    // Use REM (modulus) to prevent type overflow
    opX = Builder.CreateSRem(argVec[0], ConstantInt::get(i32_type, INT8_MAX),
                             "X_REM");
    opY = Builder.CreateSRem(argVec[1], ConstantInt::get(i32_type, INT8_MAX),
                             "Y_REM");
  } else if (argVec.size() == 1) {
    opX = Builder.CreateSRem(argVec[0], ConstantInt::get(i32_type, INT8_MAX),
                             "X_REM");
    opY = Builder.CreateLoad((Value *)y, "y");
  } else {
    opX = Builder.CreateLoad((Value *)x, "x");
    opY = Builder.CreateLoad((Value *)y, "y");
  }

  short rand = randMBA(rng);
  Value *res = nullptr;
  switch (rand) {
  case 1: // 7y^2 - 1 != x
    res = Builder.CreateICmpNE(
        Builder.CreateSub(Builder.CreateMul(ConstantInt::get(i32_type, 7),
                                            Builder.CreateMul(opY, opY)),
                          ConstantInt::get(i32_type, 1)),
        opX);
    break;
  case 2: // (x^2 + 1) % 7 != 0
    res = Builder.CreateICmpNE(
        Builder.CreateSRem(Builder.CreateAdd(Builder.CreateMul(opX, opX),
                                             ConstantInt::get(i32_type, 1)),
                           ConstantInt::get(i32_type, 7)),
        ConstantInt::get(i32_type, 0));
    break;
  case 3: // (x^2 + x + 7) % 81 != 0
    res = Builder.CreateICmpNE(
        Builder.CreateSRem(
            Builder.CreateAdd(
                Builder.CreateAdd(Builder.CreateMul(opX, opX), opX),
                ConstantInt::get(i32_type, 7)),
            ConstantInt::get(i32_type, 81)),
        ConstantInt::get(i32_type, 0));
    break;
  case 4: // (4x^2 + 4) % 19 != 0
    res = res = Builder.CreateICmpNE(
        Builder.CreateSRem(
            Builder.CreateAdd(Builder.CreateMul(Builder.CreateMul(opX, opX),
                                                ConstantInt::get(i32_type, 4)),
                              ConstantInt::get(i32_type, 4)),
            ConstantInt::get(i32_type, 19)),
        ConstantInt::get(i32_type, 0));
    break;
  }
  return res;
}

// Based on https://github.com/hxuhack/symobfuscator-deprecated-
Value *BogusFlowPass::getSymOP(Module &M, Instruction *inst, Value *arg) {
  IRBuilder<> Builder(inst);
  Type *argType = arg->getType();

  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(5, 10);
  unsigned size = 8; // dist(rng);

  const DataLayout &DL = M.getDataLayout();
  ConstantInt *i0_32 = (ConstantInt *)ConstantInt::getSigned(
      Type::getInt32Ty(M.getContext()), 0);
  ConstantInt *size_i64 = dyn_cast<ConstantInt>(
      ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), size));

  // Define arrays
  ArrayType *arr_type1 = ArrayType::get(Type::getInt64Ty(M.getContext()), size);
  ArrayType *arr_type2 = ArrayType::get(Type::getInt64Ty(M.getContext()), size);
  // Allocate arrays
  AllocaInst *arr_alloc1 =
      new AllocaInst(arr_type1, DL.getAllocaAddrSpace(), "arr1", inst);
  AllocaInst *arr_alloc2 =
      new AllocaInst(arr_type2, DL.getAllocaAddrSpace(), "arr2", inst);

  // Initialize arrays
  std::vector<Instruction *> gepVec1;
  std::vector<StoreInst *> storeVec1;
  std::vector<Instruction *> gepVec2;
  std::vector<StoreInst *> storeVec2;

  for (int i = 0; i < size; ++i) {
    std::vector<Value *> vec;
    vec.push_back(i0_32);
    ConstantInt *ci = dyn_cast<ConstantInt>(
        ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), i));
    vec.push_back(ci);
    ArrayRef<Value *> arr(vec);
    gepVec1.push_back(GetElementPtrInst::CreateInBounds(
        (Value *)arr_alloc1, arr, "l1_arrayidx", inst));
  }

  for (int i = 0; i < gepVec1.size(); i++) {
    ConstantInt *ci = dyn_cast<ConstantInt>(
        ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), i));
    storeVec1.push_back(new StoreInst(ci, gepVec1[i], inst));
  }

  for (int i = 0; i < size; ++i) {
    std::vector<Value *> vec;
    vec.push_back(i0_32);
    ConstantInt *ci = dyn_cast<ConstantInt>(
        ConstantInt::getSigned(Type::getInt32Ty(M.getContext()), i));
    vec.push_back(ci);
    ArrayRef<Value *> arr(vec);
    gepVec2.push_back(GetElementPtrInst::CreateInBounds(
        (Value *)arr_alloc2, arr, "l2_arrayidx", inst));
  }

  for (int i = 0; i < gepVec2.size(); i++) {
    ConstantInt *ci = dyn_cast<ConstantInt>(
        ConstantInt::getSigned(Type::getInt64Ty(M.getContext()), i));
    storeVec2.push_back(new StoreInst(ci, gepVec2[i], inst));
  }

  Value *allocaInst = Builder.CreateAlloca(argType, DL.getAllocaAddrSpace(),
                                           nullptr, "allocaInst");
  Value *storeInst = Builder.CreateStore(arg, allocaInst);
  Value *loadInst = Builder.CreateLoad(allocaInst, "loadInst");

  // Remainder instruction
  Value *remInst;
  if (((IntegerType *)argType)->getBitWidth() != 64) {
    // Cast loadInst to i64
    Value *load64 = Builder.CreateSExtOrBitCast(
        loadInst, Type::getInt64Ty(M.getContext()), "load64");
    remInst = Builder.CreateSRem(load64, size_i64);
  } else {
    remInst = Builder.CreateSRem(loadInst, size_i64);
  }

  // Load GEPs
  std::vector<Value *> vec1;
  vec1.push_back(i0_32);
  vec1.push_back(remInst);
  ArrayRef<Value *> arrRef1(vec1);
  Value *gep1 =
      Builder.CreateInBoundsGEP((Value *)arr_alloc1, arrRef1, "idx_1");
  LoadInst *load1 = new LoadInst(gep1, "", false, inst);

  std::vector<Value *> vec2;
  vec2.push_back(i0_32);
  vec2.push_back(load1);
  ArrayRef<Value *> arrRef2(vec2);
  Value *gep2 =
      Builder.CreateInBoundsGEP((Value *)arr_alloc2, arrRef2, "idx_2");
  LoadInst *load2 = new LoadInst(gep2, "", false, inst);

  // Compare arr1[i] == arr2[j]
  Value *res = Builder.CreateICmpNE(load2, size_i64, "ArrOpq");
  return res;
}

char BogusFlowPass::ID = 0;

// Register the pass
static RegisterPass<BogusFlowPass> X("bogus", "Add Bogus Control Flow");