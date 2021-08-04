#define DEBUG_TYPE "mba"

//TODO build LLVM with stats enabled and try this
#include "llvm/ADT/Statistic.h"
STATISTIC(MBACount, "The number of substituted instructions with MBA");

#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/RandomNumberGenerator.h"

using namespace llvm;

static cl::opt<int>
ObfProb("sub_prob",
         cl::desc("Choose the probability of obfuscating a binary operation"),
         cl::value_desc("probability"), cl::init(100),  cl::Optional);

namespace {
  struct MbaPass : public BasicBlockPass {
    
    static char ID;

    MbaPass() : BasicBlockPass(ID) {}

    Value *SubAdd(BinaryOperator *BinOp);
    Value *SubAdd2(BinaryOperator *BinOp);
    Value *SubSub(BinaryOperator *BinOp);
    Value *SubXor(BinaryOperator *BinOp);
    Value *SubAnd(BinaryOperator *BinOp);
    Value *SubOr(BinaryOperator *BinOp);

    virtual bool runOnBasicBlock(BasicBlock &BB) {
      auto rng = BB.getParent()->getParent()->createRNG(this);
      
      //errs() << "Generated random number: " << randNum << '\n';
      
      bool changed = false;
      for (auto current = BB.begin(), last = BB.end(); current != last; current++){
        Instruction &Inst = *current;
        // Check if the instruction is a BinaryOperator type
        auto *BinOp = dyn_cast<BinaryOperator>(&Inst);
        // If not, skip it
        if (!BinOp)
          continue;
    
        // Maybe insert probability of replacement (condition) here

        unsigned Opcode = BinOp->getOpcode();
        // Substitute only integer operations
        if (!BinOp->getType()->isIntegerTy())
          continue;
        
        
        int randNum = (*rng)();
        switch(Opcode){
          case Instruction::Add:
            switch (randNum % 2){
            case 0:
              ReplaceInstWithValue(BB.getInstList(), current, SubAdd(BinOp));
              break;
            case 1:
              ReplaceInstWithValue(BB.getInstList(), current, SubAdd2(BinOp));
              break;
            }
            ++MBACount;
            changed = true;
            break;
          case Instruction::Sub: 
            ReplaceInstWithValue(BB.getInstList(), current, SubSub(BinOp));
            ++MBACount;
            changed = true;
            break;
          case Instruction::Xor:
            ReplaceInstWithValue(BB.getInstList(), current, SubXor(BinOp));
            ++MBACount;
            changed = true;
            break;
          case Instruction::And:
            ReplaceInstWithValue(BB.getInstList(), current, SubAnd(BinOp));
            ++MBACount;
            changed = true;
            break;
          case Instruction::Or:
            ReplaceInstWithValue(BB.getInstList(), current, SubOr(BinOp));
            ++MBACount;
            changed = true;
            break;
          default: continue;
        }
        
      } // for loop
      return changed;
    } // runOnBasicBlock
  }; // MbaPass

} // namespace

// Substitute x + y --> 2*(x | y) - (x ^ y)
Value* MbaPass::SubAdd(BinaryOperator *BinOp){
  errs() << "Using SubAdd: x + y --> 2*(x | y) - (x ^ y)" << '\n';
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
    Builder.CreateMul(
      ConstantInt::get(BinOp->getType(), 2),
      Builder.CreateOr(
        BinOp->getOperand(0), 
        BinOp->getOperand(1))
    ),
    Builder.CreateXor(
      BinOp->getOperand(0), 
      BinOp->getOperand(1))      
  );
  return NewValue;
}

// Substitute x + y --> (x^(~y)) + 2*(x|y) + 1
Value *MbaPass::SubAdd2(BinaryOperator *BinOp){
  errs() << "Using SubAdd2: x + y --> (x^(~y)) + 2*(x|y) + 1" << '\n';
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = 
  Builder.CreateAdd(
    Builder.CreateXor(
      BinOp->getOperand(0),
      Builder.CreateNot(BinOp->getOperand(1))
    ),
    Builder.CreateMul(
      ConstantInt::get(BinOp->getType(), 2),
      Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1))
    )
  );
  NewValue = Builder.CreateAdd(NewValue, ConstantInt::get(BinOp->getType(),1));
  return NewValue;
}

// Substitute x - y --> 2*(x & ~y) - (x ^ y)
Value *MbaPass::SubSub(BinaryOperator *BinOp){
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
    Builder.CreateMul(
      ConstantInt::get(BinOp->getType(), 2),
      Builder.CreateAnd(
        BinOp->getOperand(0),
        Builder.CreateNot(BinOp->getOperand(1)))
    ),
    Builder.CreateXor(
      BinOp->getOperand(0), 
      BinOp->getOperand(1))  
  );
  return NewValue;
}

// Substitute x ^ y --> (x | y) - (x & y)
Value *MbaPass::SubXor(BinaryOperator *BinOp){
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
    Builder.CreateOr(
      BinOp->getOperand(0), 
      BinOp->getOperand(1)),
    Builder.CreateAnd(
      BinOp->getOperand(0), 
      BinOp->getOperand(1))      
  );
  return NewValue;
}

// Substitute x & y --> (~x | y) - ~x
Value *MbaPass::SubAnd(BinaryOperator *BinOp){
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
    Builder.CreateOr(
      Builder.CreateNot(
        BinOp->getOperand(0)),
        BinOp->getOperand(1)
    ),
    Builder.CreateNot(BinOp->getOperand(0))
  );
  return NewValue;
}

// Substitute x | y --> (x & ~y) + y
Value *MbaPass::SubOr(BinaryOperator *BinOp){
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
    Builder.CreateAnd(
      BinOp->getOperand(0),
      Builder.CreateNot(BinOp->getOperand(1))
    ),
    BinOp->getOperand(1)
  );
  return NewValue;
}

char MbaPass::ID = 0;

// Register the pass 
static RegisterPass<MbaPass> X("mba", "Substitute binary operations with MBA");
