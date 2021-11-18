#define DEBUG_TYPE "bogus"

//TODO build LLVM with stats enabled and try this
#include "llvm/ADT/Statistic.h"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

namespace {
  struct BogusFlowPass : public FunctionPass {
    
    bool changed = false;
    static char ID;
    BogusFlowPass() : FunctionPass(ID) {}

    void Bogus(Function &F);
    void AddBogus(BasicBlock *basicBlock, Function &F);
    BasicBlock *createAlteredBB(BasicBlock *basicBlock, 
    const Twine &Name, Function *F);
    bool doF(Module &M);

    virtual bool runOnFunction(Function &F) {
      Bogus(F);
      doF(*F.getParent());
      return changed;
    } // runOnFunction
  }; // Pass

} // namespace

void BogusFlowPass::Bogus(Function &F){
  // Put all the function's block in a list
  std::list<BasicBlock *> basicBlocks;
  for (auto i=F.begin(); i!=F.end(); ++i) {
    basicBlocks.push_back(&*i);
  }

  while (!basicBlocks.empty()){
    BasicBlock *basicBlock = basicBlocks.front();
    AddBogus(basicBlock, F);
    basicBlocks.pop_front();
  }
}

void BogusFlowPass::AddBogus(BasicBlock *basicBlock, Function &F){
  auto i1 = basicBlock->begin();
  if (basicBlock->getFirstNonPHIOrDbgOrLifetime()){
        i1 = (BasicBlock::iterator)basicBlock->getFirstNonPHIOrDbgOrLifetime();
  }
  BasicBlock *originalBB = basicBlock->splitBasicBlock(i1, "originalBB");
  BasicBlock *alteredBB = createAlteredBB(originalBB, "alteredBB", &F);

  // Now that all the blocks are created,
  // we modify the terminators to adjust the control flow.

  alteredBB->getTerminator()->eraseFromParent();
  basicBlock->getTerminator()->eraseFromParent();

  // Preparing a condition..
  // For now, the condition is an always true comparaison between 2 float
  // This will be complicated after the pass (in doFinalization())
  Value * LHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
  Value * RHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
  FCmpInst * condition = new FCmpInst(*basicBlock, FCmpInst::FCMP_TRUE , LHS, RHS, "condition");

  // Jump to the original basic block if the condition is true or
  // to the altered block if false.
  BranchInst::Create(originalBB, alteredBB, (Value *)condition, basicBlock);
  
  // The altered block loop back on the original one.
  BranchInst::Create(originalBB, alteredBB);

  // The end of the originalBB is modified to give the impression that sometimes
  // it continues in the loop, and sometimes it returns the desired value
  // (of course it's always true, so it always uses the original terminator.
  // But this will be obfuscated too ;) )

  // iterate on instruction just before the terminator of the originalBB
  BasicBlock::iterator i = originalBB->end();

  // Split at this point (we only want the terminator in the second part)
  BasicBlock * originalBBpart2 = originalBB->splitBasicBlock(--i , "originalBBpart2");

  // The first part goes either to the return statement or to the begining
  // of the altered block. So we erase the terminator created when splitting.
  originalBB->getTerminator()->eraseFromParent();

  FCmpInst * condition2 = new FCmpInst(*originalBB, CmpInst::FCMP_TRUE , LHS, RHS, "condition2");
  BranchInst::Create(originalBBpart2, alteredBB, (Value *)condition2, originalBB);
}

BasicBlock *BogusFlowPass::createAlteredBB(BasicBlock *basicBlock, 
    const Twine &Name = "alteredBB", Function *F = 0){
  ValueToValueMapTy VMap;
  BasicBlock * alteredBB = llvm::CloneBasicBlock (basicBlock, VMap, Name, F);
  auto ji = basicBlock->begin();
  // Remap operands
  for (auto i = alteredBB->begin(), e = alteredBB->end(); i != e; ++i){
    // Loop over the operands of the instruction
    for(User::op_iterator opi = i->op_begin (), ope = i->op_end(); opi != ope; ++opi){
      // get the value for the operand
      Value *v = MapValue(*opi, VMap,  RF_None, 0);
      if (v != 0){
        *opi = v;
      }
    }

    // Remap phi nodes' incoming blocks.
    if (PHINode *pn = dyn_cast<PHINode>(i)) {
      for (unsigned j = 0, e = pn->getNumIncomingValues(); j != e; ++j) {
        Value *v = MapValue(pn->getIncomingBlock(j), VMap, RF_None, 0);
        if (v != 0){
          pn->setIncomingBlock(j, cast<BasicBlock>(v));
        }
      }
    }
    // Remap attached metadata.
    SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
    i->getAllMetadata(MDs);
    // important for compiling with DWARF, using option -g.
    i->setDebugLoc(ji->getDebugLoc());
    ji++;
  }

  // TODO improved junk code insertion here

  return alteredBB;
}

/* doFinalization
*
* Overwrite FunctionPass method to apply the transformations to the whole module.
* This part obfuscate all the always true predicates of the module.
* More precisely, the condition which predicate is FCMP_TRUE.
* It also remove all the functions' basic blocks' and instructions' names.
*/
bool BogusFlowPass::doF(Module &M){
  // TODO: improve opaque predicates, use MBA, IRBuilder

  Value * x1 =ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false);
  Value * y1 =ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false);

  GlobalVariable 	* x = new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
      GlobalValue::CommonLinkage, (Constant * )x1, "x");
  GlobalVariable 	* y = new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
      GlobalValue::CommonLinkage, (Constant * )y1, "y");

  std::vector<Instruction*> toEdit, toDelete;
  BinaryOperator *op,*op1 = NULL;
  LoadInst * opX , * opY;
  ICmpInst * condition, * condition2;

  for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi){
    for (Function::iterator fi = mi->begin(), fe = mi->end(); fi != fe; ++fi){
      Instruction *tbb = fi->getTerminator();
      if (tbb->getOpcode() == Instruction::Br){
        BranchInst *br = (BranchInst *)(tbb);
        if (br->isConditional()){
          FCmpInst *cond = (FCmpInst *) br->getCondition();
          unsigned opcode = cond->getOpcode();
          if (opcode == Instruction::FCmp){
            if (cond->getPredicate() == FCmpInst::FCMP_TRUE){
              toDelete.push_back(cond);
              toEdit.push_back(tbb);
            }
          }
        }
      }
    } // function loop
  } // module loop

  // Replace all the branches found 
  for (auto i = toEdit.begin(); i != toEdit.end(); ++i){
    //if y < 10 || x*(x+1) % 2 == 0
    opX = new LoadInst ((Value *)x, "", (*i));
    opY = new LoadInst ((Value *)y, "", (*i));

    op = BinaryOperator::Create(Instruction::Sub, (Value *)opX,
        ConstantInt::get(Type::getInt32Ty(M.getContext()), 1,
          false), "", (*i));
    op1 = BinaryOperator::Create(Instruction::Mul, (Value *)opX, op, "", (*i));
    op = BinaryOperator::Create(Instruction::URem, op1,
        ConstantInt::get(Type::getInt32Ty(M.getContext()), 2,
          false), "", (*i));
    condition = new ICmpInst((*i), ICmpInst::ICMP_EQ, op,
        ConstantInt::get(Type::getInt32Ty(M.getContext()), 0,
          false));
    condition2 = new ICmpInst((*i), ICmpInst::ICMP_SLT, opY,
        ConstantInt::get(Type::getInt32Ty(M.getContext()), 10,
          false));
    op1 = BinaryOperator::Create(Instruction::Or, (Value *)condition,
        (Value *)condition2, "", (*i));

    BranchInst::Create(((BranchInst*)*i)->getSuccessor(0),
        ((BranchInst*)*i)->getSuccessor(1),(Value *) op1,
        ((BranchInst*)*i)->getParent());
    (*i)->eraseFromParent(); // erase the branch
  }

  for (auto i = toDelete.begin(); i != toDelete.end(); ++i){
    (*i)->eraseFromParent();
  }
  return true;
} // doF

char BogusFlowPass::ID = 0;

// Register the pass 
static RegisterPass<BogusFlowPass> X("bogus", "Add Bogus Control Flow");
