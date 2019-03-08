#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/DerivedTypes.h"
#include <map>
#include <vector>
#include <cstring>

using namespace llvm;

namespace {
	struct BranchBias : public FunctionPass {
		static char ID;
		BranchBias() : FunctionPass(ID) {}

		virtual bool runOnFunction(Function& func) override;
	};
}

bool BranchBias::runOnFunction(Function& func) {
	LLVMContext& ctx = func.getContext();
	Module* pm = func.getParent();

	for (BasicBlock& bBlock : func) {
		for (Instruction& inst : bBlock) {
			if (BranchInst* pBranchInst = dyn_cast<BranchInst>(&inst)) {
				if (pBranchInst->isConditional()) {
					//  prepare function call arguments
					IRBuilder<> updateFuncBuilder(bBlock.getTerminator());

					Function* updateFunc = cast<Function>(pm->getOrInsertFunction(
						"updateBranchInfo",
						Type::getVoidTy(ctx),
						Type::getInt1Ty(ctx)
					));

					std::vector<Value*> updateFuncArgs{ pBranchInst->getCondition() };

					//  call function
					updateFuncBuilder.CreateCall(updateFunc, updateFuncArgs);
				}
			}
		}
	}
	IRBuilder<> printFuncBuilder(func.back().getTerminator());
	Function* printFunc = cast<Function>(pm->getOrInsertFunction(
		"printOutBranchInfo",
		Type::getVoidTy(ctx)
	));
	printFuncBuilder.CreateCall(printFunc, std::vector<Value*>{});
	//  return true if the original function is modified
	return true;
}

//  the value of ID doesn't matter. Its address is used to identify an LLVM pass.
char BranchBias::ID = 0;
static RegisterPass<BranchBias> cse231_bb(
	"cse231-bb",
	"cse231-bb",
	false,
	false);