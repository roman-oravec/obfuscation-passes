#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {
  struct MbaPass : public BasicBlockPass {
    
    static char ID;

    MbaPass() : BasicBlockPass(ID) {}

    virtual bool runOnBasicBlock(BasicBlock &BB) {
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
        // Substitute only integer ops
        if (!BinOp->getType()->isIntegerTy())
          continue;
        
        IRBuilder<> Builder(BinOp);
        Value *NewValue;

        switch(Opcode){
          case Instruction::Sub: 
            // Substitute x - y --> 2*(x & ~y) - (x ^ y)
            NewValue = Builder.CreateSub(
              Builder.CreateMul(
                ConstantInt::get(BinOp->getType(), 2),
                Builder.CreateAnd(BinOp->getOperand(0),
                  Builder.CreateNot(BinOp->getOperand(1)))
              ),
              Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1))      
            );
            break;

          case Instruction::Add:
            // Substitute x + y --> 2*(x | y) - (x ^ y)
            NewValue = Builder.CreateSub(
              Builder.CreateMul(
                ConstantInt::get(BinOp->getType(), 2),
                Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1))
              ),
              Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1))      
            );
            break;

          case Instruction::Xor:
            // Substitute x ^ y --> (x | y) - (x & y)
            NewValue = Builder.CreateSub(
              Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1)),
              Builder.CreateAnd(BinOp->getOperand(0), BinOp->getOperand(1))      
            );
            break;

          case Instruction::And:
            // Substitute x & y --> (~x | y) - ~x
            NewValue = Builder.CreateSub(
              Builder.CreateOr(
                Builder.CreateNot(BinOp->getOperand(0)),
                BinOp->getOperand(1)
              ),
              Builder.CreateNot(BinOp->getOperand(0))
            );
          default: continue;
        }

        // Replace the old instruction
        ReplaceInstWithValue(BB.getInstList(), current, NewValue);

        changed = true;
      }

      return changed;
    }
  };
}

char MbaPass::ID = 0;

// Register the pass 
static RegisterPass<MbaPass> X("mba", "Substitute instructions with MBA");
