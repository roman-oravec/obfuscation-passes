// Based on https://blog.quarkslab.com/turning-regular-code-into-atrocities-with-llvm.html

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"

#include <boost/integer/mod_inverse.hpp>

#include <sstream>
#include <random>
using namespace llvm;

namespace {

  using prime_type = uint32_t;
  
  class ObfConstPass : public BasicBlockPass {
    std::vector<Value *> IntegerVect;
    std::default_random_engine Generator;

  public:

    static char ID;

    // Constructor
    ObfConstPass() : BasicBlockPass(ID) {}

    virtual bool runOnBasicBlock(BasicBlock &BB) {
      IntegerVect.clear();
      bool modified = false;

      // Iterator over all non-PHI instructions (not from beggining)
      for (typename BasicBlock::iterator I = BB.getFirstInsertionPt(), end = BB.end(); I != end; ++I) {
        Instruction &Inst = *I;
        if (isValidCandidateInstruction(Inst)){
        // Iterate over operands
          for (size_t i = 0; i < Inst.getNumOperands(); ++i) {
            if (Constant *C = isValidCandidateOperand(Inst.getOperand(i))) {
              if (Value *New_val = replaceConst(Inst, C)){
                Inst.setOperand(i, New_val);
                modified = true;
              } else {
                  errs() << "ObfConstPass: could not rand pick a variable for replacement\n";
              }
            }
          }
        }
        registerInteger(Inst);
       }
      return modified;
    }
  
  private:

    Value* replaceConst(Instruction &Inst, Constant *C){
      std::random_device dev;
      std::mt19937 rng(dev());
      std::uniform_int_distribution<std::mt19937::result_type> dist(1,INT32_MAX);
      auto &ctx = Inst.getParent()->getContext();
      Type *i32_type = llvm::IntegerType::getInt64Ty(ctx);
      uint32_t mod = static_cast<long>(INT32_MAX)+1;

      IRBuilder<NoFolder> Builder(&Inst);
      
      uint32_t a_inv = 0;
      uint32_t a;
      while (a_inv == 0){
        a = dist(rng);
      if (a % 2 == 0){
        a = a / 2;
      }
        a_inv = boost::integer::mod_inverse(static_cast<long>(a), static_cast<long>(mod));
      }

      uint32_t b = dist(rng);
      uint32_t x = dist(rng);
      uint32_t y = dist(rng);

      Constant *constA = ConstantInt::get(i32_type, a);
      Constant *constB = ConstantInt::get(i32_type, b);
      Constant *constX = ConstantInt::get(i32_type, x);
      Constant *constY = ConstantInt::get(i32_type, y);

      // fx = ax + b (mod uint32_max)
      uint32_t fC_ax = a*C->getUniqueInteger().getLimitedValue();
      errs() << "\n" << "fC_ax= " << fC_ax << "\n";
      
      Value *fC = Builder.CreateAdd(
        ConstantInt::get(i32_type, fC_ax),
        constB);
      fC->setName("fC");
      
      // E= x + y − (x | y) − (~x | y) + (~x)
      Value *E_1 = Builder.CreateAdd(constX, constY);
      Value *E_2 = Builder.CreateOr(constX, constY);
      Value *E_3 = Builder.CreateOr(Builder.CreateNot(constX), constY);
      Value *E_11 = Builder.CreateSub(E_1, E_2);
      Value *E_12 = Builder.CreateSub(E_11, E_3);
      Value *E = Builder.CreateAdd(E_12, Builder.CreateNot(constX));
      E->setName("E");

      // g(E + f(C)) = C
      // g(x) = a_inv*(E + f(C)) + (-a_inv * b)
      uint64_t g_b = (-a_inv*b) % mod;
      errs() << "\n" << "a_inv: " << a_inv << "\ng_b= " << g_b << "\n";
      
      Value *E_fC = Builder.CreateAdd(E, fC);
      E_fC->setName("E_fC");

      Value *NewVal = Builder.CreateAdd(
        Builder.CreateMul(
          ConstantInt::get(i32_type, a_inv), E_fC
        ),
        ConstantInt::get(i32_type, g_b)
      );

      errs() << ConstantInt::get(i32_type, g_b)->getUniqueInteger() << '\n';
      NewVal->setName("NewVal");

      Value *res1 = Builder.CreateURem(NewVal, ConstantInt::get(i32_type, mod));
      Value *res = Builder.CreateZExtOrTrunc(res1, llvm::IntegerType::getInt32Ty(ctx), "Result");
      errs() << "a= " << a << "\n" << "b= " << b  << '\n';
      return res;
    }

    bool isValidCandidateInstruction(Instruction &Inst){
      if (isa<GetElementPtrInst>(&Inst)){ // Ignore GEP
        return false;
      } else if (isa<SwitchInst>(&Inst)) { // Ignore switch
        return false;
      } else if (isa<CallInst>(&Inst)) { // Ignore calls
        return false;
      } else {
        //errs() << "Valid instruction: " << Inst << "\n";
        return true;
      }
    }

    Constant *isValidCandidateOperand(Value *V) {
      Constant *C;
      //errs() << "Checking operand: " << V << "\n";
      // Is it a constant (== literal)?
      if (!(C = dyn_cast<Constant>(V))) return nullptr;
      // Ignore 0
      //if (C->isNullValue()) return nullptr;
      // We found a constant, check if it's an integer
      if(!C->getType()->isIntegerTy()) return nullptr;
      // Ignore 1 
      //if (C->getUniqueInteger().getLimitedValue() == 1) return nullptr;
      if (C->getUniqueInteger().isNegative()) return nullptr;
      if (C->getType()->getScalarSizeInBits() != 32) return nullptr;
      errs() << "Value: " << C->getUniqueInteger().getLimitedValue() << "\n ";
      return C;
    }

    // Possibly use this instead of random numbers
    void registerInteger(Value &V) {
      if (V.getType()->isIntegerTy()){
        IntegerVect.push_back(&V);
        //errs() << "Registering integer " << V << "\n";
      }
    }

  }; // class ObfConstPass
} // namespace

char ObfConstPass::ID = 0;

// Register the pass so `opt -skeleton` runs it.
static RegisterPass<ObfConstPass> X("obfconst", "ObfConstPass pass");
