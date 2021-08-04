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

    virtual bool runOnFunction(Function &F) {
      Bogus(F);
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

    //TODO
    // Remap phi nodes' incoming blocks.
    if (PHINode *pn = dyn_cast<PHINode>(i)) {
      for (unsigned j = 0, e = pn->getNumIncomingValues(); j != e; ++j) {
        Value *v = MapValue(pn->getIncomingBlock(j), VMap, RF_None, 0);
        if (v != 0){
          pn->setIncomingBlock(j, cast<BasicBlock>(v));
        }
      }
    }
  }
  return alteredBB;
}

char BogusFlowPass::ID = 0;

// Register the pass 
static RegisterPass<BogusFlowPass> X("bogus", "Add Bogus Control Flow");
