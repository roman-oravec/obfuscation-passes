#define DEBUG_TYPE "mba"

#include <random>

// TODO build LLVM with stats enabled and try this
#include "llvm/ADT/Statistic.h"
STATISTIC(MBACount, "The number of substituted instructions with MBA");

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

static cl::opt<int> ObfProb(
    "sub_prob",
    cl::desc("Choose the probability of obfuscating a binary operation"),
    cl::value_desc("probability"), cl::init(100), cl::Optional);

namespace {
struct MbaPass : public BasicBlockPass {

  static char ID;

  MbaPass() : BasicBlockPass(ID) {}

  Value *SubAdd(BinaryOperator *BinOp);
  Value *SubAdd2(BinaryOperator *BinOp);
  Value *SubAdd3(BinaryOperator *BinOp);

  Value *SubSub(BinaryOperator *BinOp);
  Value *SubSub2(BinaryOperator *BinOp);
  Value *SubSub3(BinaryOperator *BinOp);

  Value *SubXor(BinaryOperator *BinOp);
  Value *SubXor2(BinaryOperator *BinOp);
  Value *SubXor3(BinaryOperator *BinOp);

  Value *SubAnd(BinaryOperator *BinOp);
  Value *SubAnd2(BinaryOperator *BinOp);
  Value *SubAnd3(BinaryOperator *BinOp);

  Value *SubOr(BinaryOperator *BinOp);
  Value *SubOr2(BinaryOperator *BinOp);
  Value *SubOr3(BinaryOperator *BinOp);

  virtual bool runOnBasicBlock(BasicBlock &BB) {
    // auto rng = BB.getParent()->getParent()->createRNG(this);
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, 2);

    bool changed = false;
    for (auto current = BB.begin(), last = BB.end(); current != last;
         current++) {
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

      int randNum = dist(rng);
      switch (Opcode) {
      case Instruction::Add:
        switch (randNum) {
        case 0:
          ReplaceInstWithValue(BB.getInstList(), current, SubAdd(BinOp));
          break;
        case 1:
          ReplaceInstWithValue(BB.getInstList(), current, SubAdd2(BinOp));
          break;
        case 2:
          ReplaceInstWithValue(BB.getInstList(), current, SubAdd3(BinOp));
          break;
        }
        ++MBACount;
        changed = true;
        break;
      case Instruction::Sub:
        switch (randNum) {
        case 0:
          errs() << "Using SubSub" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubSub(BinOp));
          break;
        case 1:
          errs() << "Using SubSub2" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubSub2(BinOp));
          break;
        case 2:
          errs() << "Using SubSub3" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubSub3(BinOp));
          break;
        }
        ++MBACount;
        changed = true;
        break;
      case Instruction::Xor:
        switch (randNum) {
        case 0:
          errs() << "Using SubXor" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubXor(BinOp));
          break;
        case 1:
          errs() << "Using SubXor2" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubXor2(BinOp));
          break;
        case 2:
          errs() << "Using SubXor3" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubXor3(BinOp));
          break;
        }
        ++MBACount;
        changed = true;
        break;
      case Instruction::And:
        switch (randNum) {
        case 0:
          errs() << "Using SubAnd" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubAnd(BinOp));
          break;
        case 1:
          errs() << "Using SubAnd2" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubAnd2(BinOp));
          break;
        case 2:
          errs() << "Using SubAnd3" << '\n';
          ReplaceInstWithValue(BB.getInstList(), current, SubAnd3(BinOp));
          break;
        }
        ++MBACount;
        changed = true;
        break;
      case Instruction::Or:
        switch (randNum) {
        case 0:
          ReplaceInstWithValue(BB.getInstList(), current, SubOr(BinOp));
          break;
        case 1:
          ReplaceInstWithValue(BB.getInstList(), current, SubOr2(BinOp));
          break;
        case 2:
          ReplaceInstWithValue(BB.getInstList(), current, SubOr3(BinOp));
          break;
        }
        ++MBACount;
        changed = true;
        break;
      default:
        continue;
      }

    } // for loop
    return changed;
  } // runOnBasicBlock
};  // MbaPass

} // namespace

// Substitute x + y --> 2*(x | y) - (x ^ y)
Value *MbaPass::SubAdd(BinaryOperator *BinOp) {
  errs() << "Using SubAdd: x + y --> 2*(x | y) - (x ^ y)" << '\n';
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateMul(
          ConstantInt::get(BinOp->getType(), 2),
          Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1))),
      Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x + y --> (x^(~y)) + 2*(x|y) + 1
Value *MbaPass::SubAdd2(BinaryOperator *BinOp) {
  errs() << "Using SubAdd2: x + y --> (x^(~y)) + 2*(x|y) + 1" << '\n';
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
      Builder.CreateXor(BinOp->getOperand(0),
                        Builder.CreateNot(BinOp->getOperand(1))),
      Builder.CreateMul(
          ConstantInt::get(BinOp->getType(), 2),
          Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1))));
  NewValue = Builder.CreateAdd(NewValue, ConstantInt::get(BinOp->getType(), 1));
  return NewValue;
}

// Substitute x + y --> (x^y) + 2y − 2*(~x & y)
Value *MbaPass::SubAdd3(BinaryOperator *BinOp) {
  errs() << "Using SubAdd3: x + y --> (x^y) + 2y − 2*(~x & y)\n";
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateAdd(
          Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1)),
          Builder.CreateMul(ConstantInt::get(BinOp->getType(), 2),
                            BinOp->getOperand(1))),
      Builder.CreateMul(
          ConstantInt::get(BinOp->getType(), 2),
          Builder.CreateAnd(Builder.CreateNot(BinOp->getOperand(0)),
                            BinOp->getOperand(1))));
  return NewValue;
}

// Substitute x - y --> 2*(x & ~y) - (x ^ y)
Value *MbaPass::SubSub(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateMul(
          ConstantInt::get(BinOp->getType(), 2),
          Builder.CreateAnd(BinOp->getOperand(0),
                            Builder.CreateNot(BinOp->getOperand(1)))),
      Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x - y --> ~(~x + y) & ~(~x + y) (from VMProtect)
Value *MbaPass::SubSub2(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAnd(
      Builder.CreateNeg(Builder.CreateAdd(
          Builder.CreateNeg(BinOp->getOperand(0)), BinOp->getOperand(1))),
      Builder.CreateNeg(Builder.CreateAdd(
          Builder.CreateNeg(BinOp->getOperand(0)), BinOp->getOperand(1))));
  return NewValue;
}

// Substitite x - y --> x & ~y - ~x & y (Tigress)
Value *MbaPass::SubSub3(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateAnd(BinOp->getOperand(0),
                        Builder.CreateNot(BinOp->getOperand(1))),
      Builder.CreateAnd(Builder.CreateNot(BinOp->getOperand(0)),
                        BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x ^ y --> (x | y) - (x & y)
Value *MbaPass::SubXor(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1)),
      Builder.CreateAnd(BinOp->getOperand(0), BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x ^ y --> (x | y) − y + (~x & y)
Value *MbaPass::SubXor2(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
      Builder.CreateSub(
          Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1)),
          BinOp->getOperand(1)),
      Builder.CreateAnd(Builder.CreateNot(BinOp->getOperand(0)),
                        BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x ^ y --> (x | y) − (~x | y) + (~x)
Value *MbaPass::SubXor3(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
      Builder.CreateSub(
          Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1)),
          Builder.CreateOr(Builder.CreateNot(BinOp->getOperand(0)),
                           BinOp->getOperand(1))),
      Builder.CreateNot(BinOp->getOperand(0)));
  return NewValue;
}

// Substitute x & y --> (~x | y) - ~x
Value *MbaPass::SubAnd(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateOr(Builder.CreateNot(BinOp->getOperand(0)),
                       BinOp->getOperand(1)),
      Builder.CreateNot(BinOp->getOperand(0)));
  return NewValue;
}

// Substitute x & y --> (x | y) − (~x & y) − (x & ~y)
Value *MbaPass::SubAnd2(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateSub(
          Builder.CreateOr(BinOp->getOperand(0), BinOp->getOperand(1)),
          Builder.CreateAnd(Builder.CreateNot(BinOp->getOperand(0)),
                            BinOp->getOperand(1))),
      Builder.CreateAnd(BinOp->getOperand(0),
                        Builder.CreateNot(BinOp->getOperand(1))));
  return NewValue;
}

// Substitute x & y --> -(x ^ y) + y + (x & ~y)
Value *MbaPass::SubAnd3(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
      Builder.CreateAdd(Builder.CreateNeg(Builder.CreateXor(
                            BinOp->getOperand(0), BinOp->getOperand(1))),
                        BinOp->getOperand(1)),
      Builder.CreateAnd(BinOp->getOperand(0),
                        Builder.CreateNot(BinOp->getOperand(1))));
  return NewValue;
}

// Substitute x | y --> (x & ~y) + y
Value *MbaPass::SubOr(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateAdd(
      Builder.CreateAnd(BinOp->getOperand(0),
                        Builder.CreateNot(BinOp->getOperand(1))),
      BinOp->getOperand(1));
  return NewValue;
}

// Substitute x | y --> (x^y) + y − (~x & y)
Value *MbaPass::SubOr2(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateAdd(
          Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1)),
          BinOp->getOperand(1)),
      Builder.CreateAnd(Builder.CreateNot(BinOp->getOperand(0)),
                        BinOp->getOperand(1)));
  return NewValue;
}

// Substitute x | y --> (x ^ y) + (~x | y) − (~x)
Value *MbaPass::SubOr3(BinaryOperator *BinOp) {
  Value *NewValue;
  IRBuilder<> Builder(BinOp);
  NewValue = Builder.CreateSub(
      Builder.CreateAdd(
          Builder.CreateXor(BinOp->getOperand(0), BinOp->getOperand(1)),
          Builder.CreateOr(Builder.CreateNot(BinOp->getOperand(0)),
                           BinOp->getOperand(1))),
      Builder.CreateNot(BinOp->getOperand(0)));
  return NewValue;
}

char MbaPass::ID = 0;

// Register the pass
static RegisterPass<MbaPass> X("mba", "Substitute binary operations with MBA");
