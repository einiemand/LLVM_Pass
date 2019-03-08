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
	struct CountDynamicInstructions : public FunctionPass {
		static char ID;
		CountDynamicInstructions() : FunctionPass(ID) {}

		virtual bool runOnFunction(Function& func) override;
	};
}

bool CountDynamicInstructions::runOnFunction(Function& func) {
	LLVMContext& ctx = func.getContext();
	Module* pm = func.getParent();

	for (BasicBlock& bBlock : func) {
		//  store # of each instruction occurrence. use std::unordered_map if don't care about order.
		std::map<uint32_t, uint32_t> instCount;
		for (Instruction& inst : bBlock) {
			++instCount[inst.getOpcode()];
			/*if (strcmp(inst.getOpcodeName(), "ret") == 0) {
				IRBuilder<> printFuncBuilder(&inst);
				Function* printFunc = cast<Function>(pm->getOrInsertFunction(
					"printOutInstrInfo",
					Type::getVoidTy(ctx)
				));
				printFuncBuilder.CreateCall(printFunc, std::vector<Value*>{});
			}*/
		}
		//  turn instructions and their # of occurrence to std::vector.
		std::vector<uint32_t> keyVec, valVec;
		for (auto& item : instCount) {
			keyVec.push_back(item.first);
			valVec.push_back(item.second);
		}

		//  prepare function call arguments
		unsigned size = instCount.size();
		IntegerType* intTy = Type::getInt32Ty(ctx);
		ArrayType* arrayTy = ArrayType::get(IntegerType::get(ctx, 32), size);

		Constant* num = ConstantInt::get(intTy, size);
		Constant* keyArray = ConstantDataArray::get(ctx, *(new ArrayRef<uint32_t>(keyVec)));
		Constant* valArray = ConstantDataArray::get(ctx, *(new ArrayRef<uint32_t>(valVec)));

		GlobalVariable* keyArgs = new GlobalVariable(
			*pm,
			arrayTy,
			true,
			GlobalValue::InternalLinkage,
			keyArray);

		GlobalVariable* valArgs = new GlobalVariable(
			*pm,
			arrayTy,
			true,
			GlobalValue::InternalLinkage,
			valArray);

		IRBuilder<> updateFuncBuilder(bBlock.getTerminator());

		Function* updateFunc = cast<Function>(pm->getOrInsertFunction(
			"updateInstrInfo",
			Type::getVoidTy(ctx),
			Type::getInt32Ty(ctx),
			Type::getInt32PtrTy(ctx),
			Type::getInt32PtrTy(ctx)
		));

		Value* key = updateFuncBuilder.CreatePointerCast(keyArgs, Type::getInt32PtrTy(ctx));
		Value* val = updateFuncBuilder.CreatePointerCast(valArgs, Type::getInt32PtrTy(ctx));

		std::vector<Value*> updateFuncArgs{ num, key, val };

		//  call function
		updateFuncBuilder.CreateCall(updateFunc, updateFuncArgs);
	}
	IRBuilder<> printFuncBuilder(func.back().getTerminator());
	Function* printFunc = cast<Function>(pm->getOrInsertFunction(
		"printOutInstrInfo",
		Type::getVoidTy(ctx)
	));
	printFuncBuilder.CreateCall(printFunc, std::vector<Value*>{});
	//  return true if the original function is modified
	return true;
}

//  the value of ID doesn't matter. Its address is used to identify an LLVM pass.
char CountDynamicInstructions::ID = 0;
static RegisterPass<CountDynamicInstructions> cse231_cdi(
	"cse231-cdi",
	"cse231-cdi",
	false,
	false);