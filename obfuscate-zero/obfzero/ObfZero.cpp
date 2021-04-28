//#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

#include <random>
using namespace llvm;

namespace {

  using prime_type = uint32_t;

  static const prime_type Prime_array[] = {
     2 ,    3 ,    5 ,    7,     11,     13,     17,     19,     23,     29,
     31,    37,    41,    43,    47,     53,     59,     61,     67,     71,
     73,    79,    83,    89,    97,    101,    103,    107,    109,    113,
    127,   131,   137,   139,   149,    151,    157,    163,    167,    173,
    179,   181,   191,   193,   197,    199,    211,    223,    227,    229,
    233,   239,   241,   251,   257,    263,    269,    271,    277,    281,
    283,   293,   307,   311,   313,    317,    331,    337,    347,    349,
    353,   359,   367,   373,   379,    383,    389,    397,    401,    409,
    419,   421,   431,   433,   439,    443,    449,    457,    461,    463,
    467,   479,   487,   491,   499,    503,    509,    521,    523,    541,
    547,   557,   563,   569,   571,    577,    587,    593,    599,    601,
    607,   613,   617,   619,   631,    641,    643,    647,    653,    659,
    661,   673,   677,   683,   691,    701,    709,    719,    727,    733,
    739,   743,   751,   757,   761,    769,    773,    787,    797,    809,
    811,   821,   823,   827,   829,    839,    853,    857,    859,    863,
    877,   881,   883,   887,   907,    911,    919,    929,    937,    941,
    947,   953,   967,   971,   977,    983,    991,    997};
  
  class ZeroPass : public BasicBlockPass {
    std::vector<Value *> IntegerVect;
    std::default_random_engine Generator;

  public:

    static char ID;

    // Constructor
    ZeroPass() : BasicBlockPass(ID) {}

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
              if (Value *New_val = replaceZero(Inst, C)){
                Inst.setOperand(i, New_val);
                modified = true;
              } else {
                errs() << "ZeroPass: could not rand pick a variable for replacement\n";
              }
            }
          }
        }
        registerInteger(Inst);
       }
      return modified;
    }
  
  private:

    bool isValidCandidateInstruction(Instruction &Inst){
      if (isa<GetElementPtrInst>(&Inst)){ // Ignore GEP
        return false;
      } else if (isa<SwitchInst>(&Inst)) { // Ignore switch
        return false;
      } else if (isa<CallInst>(&Inst)) { // Ignore calls
        return false;
      } else {
        return true;
      }
    }

    Constant *isValidCandidateOperand(Value *V) {
      Constant *C;
      // Is it a constant (== literal)?
      if (!(C = dyn_cast<Constant>(V))) return nullptr;
      // Is it a NULL value?
      if (!C->isNullValue()) return nullptr;
      // We found a NULL constant, lets validate it
      if(!C->getType()->isIntegerTy()) {
        // dbgs() << "Ignoring non integer value\n";
        return nullptr;
      }
      return C;
    }


    void registerInteger(Value &V) {
      if (V.getType()->isIntegerTy()){
        IntegerVect.push_back(&V);
        //errs() << "Registering integer " << V << "\n";
      }
    }

    // Return a random prime number not equal to DifferentFrom
  // If an error occurs returns 0
  prime_type getPrime(prime_type DifferentFrom = 0) {
      static std::uniform_int_distribution<prime_type> Rand(0, std::extent<decltype(Prime_array)>::value);
      size_t MaxLoop = 10;
      prime_type Prime;

      do {
            Prime = Prime_array[Rand(Generator)];
      } while(Prime == DifferentFrom && --MaxLoop);

      if(!MaxLoop) {
          return 0;
      }

      return Prime;
    }


    Value* replaceZero(Instruction &Inst, Value* VReplace){
      // Replacing 0 by:
      // prime1 * ((x | any1)^2) != prime2 * ((y | any2)^2)
      // with prime1 != prime2 and any1 != 0 and any2 != 0

      prime_type  p1 = getPrime(),
                  p2 = getPrime(p1);

      if (p1 == 0 || p2 == 0){
        return nullptr;
      }

      Type  *ReplacedType = VReplace->getType(), // type of operand
            *IntermediaryType = IntegerType::get(Inst.getContext(), sizeof(prime_type) * 8);
       
      // Abort obfuscation if we haven't found any integers yet
      if (IntegerVect.empty()){
        return nullptr;
      }
      // Random distribution to pick variables from IntegerVect
      std::uniform_int_distribution<size_t> Rand(0, IntegerVect.size() - 1);
      // Random dist to pick Any1 and Any2 from range 1..10
      std::uniform_int_distribution<size_t> RandAny(1, 10);

      // Choose indexes for x and y
      size_t Index1 = Rand(Generator), Index2 = Rand(Generator);

      // Create objects representing literals (x,y,any1,any2...)
      Constant *any1 =  ConstantInt::get(IntermediaryType, 1 + RandAny(Generator)),
               *any2 =  ConstantInt::get(IntermediaryType, 1 + RandAny(Generator)),
               *prime1 = ConstantInt::get(IntermediaryType, p1),
               *prime2 = ConstantInt::get(IntermediaryType, p2),
               // Bitmask to prevent type overflow in x and y
               *OverflowMask = ConstantInt::get(IntermediaryType, 0x00000007);

      // Build and insert new instruction before the inst we are working on (&Inst parameter)
      IRBuilder<> Builder(&Inst);
      
      // left hand side

      // Casting x to intermediary type
      // CreateZextOrTrunc zero-extends or truncates value
      // from the first argument to type in the second arg
      Value *LhsCast = Builder.CreateZExtOrTrunc(IntegerVect.at(Index1), IntermediaryType);
      // Register new integer for future obfuscation
      registerInteger(*LhsCast);
      // Avoid overflow and trunc x (x && mask)
      Value *LhsAnd = Builder.CreateAnd(LhsCast, OverflowMask);
      registerInteger(*LhsAnd);
      // (x | any1)
      Value *LhsOr = Builder.CreateOr(LhsAnd, any1);
      registerInteger(*LhsOr);
      // (x | any)^2
      Value *LhsSquare = Builder.CreateMul(LhsOr, LhsOr);
      registerInteger(*LhsSquare);
      // prime1 * ((x | any1)^2)
      Value *LhsTot = Builder.CreateMul(LhsSquare, prime1);
      registerInteger(*LhsTot);

      // right hand side

      Value *RhsCast = Builder.CreateZExtOrTrunc(IntegerVect.at(Index2), IntermediaryType);
      registerInteger(*RhsCast);
      Value *RhsAnd = Builder.CreateAnd(RhsCast, OverflowMask);
      registerInteger(*RhsAnd);
      Value *RhsOr = Builder.CreateOr(RhsAnd, any2);
      registerInteger(*RhsOr);
      Value *RhsSquare = Builder.CreateMul(RhsOr, RhsOr);
      registerInteger(*RhsSquare);
      Value *RhsTot = Builder.CreateMul(RhsSquare, prime2);
      registerInteger(*RhsTot);

      // Compare Lhs == Rhs, which is always false
      errs() << "Creating ICMP instruction\n";

      Value *comp = Builder.CreateICmp(CmpInst::Predicate::ICMP_EQ, LhsTot, RhsTot);
      registerInteger(*comp);

      // Cast the boolean false back to the type of original operand
      Value *castComp = Builder.CreateZExt(comp, ReplacedType);
      registerInteger(*castComp);

      errs() << "Returning castComp\n";
      return castComp;
    }


  }; // class ZeroPass
} // namespace

char ZeroPass::ID = 0;

// Register the pass so `opt -skeleton` runs it.
static RegisterPass<ZeroPass> X("obfzero", "ObfuscateZero pass");
