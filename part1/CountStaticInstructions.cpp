#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

namespace {
    struct CountStaticInstructions : public FunctionPass {
        static char ID;
        CountStaticInstructions() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function& func) override {
            std::map<const char*, int> instCount;
            for (auto iter = inst_begin(func); iter != inst_end(func); ++iter) {
                ++instCount[iter->getOpcodeName()];
            }

            for (auto& item : instCount) {
                errs() << item.first << '\t' << item.second << '\n';
            }
	    return false;
        }
    };
}

char CountStaticInstructions::ID = 0;
static RegisterPass<CountStaticInstructions> cse231_csi(
        "cse231-csi",
        "cse231-csi",
        false,
        false);
