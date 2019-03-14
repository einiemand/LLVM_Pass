#include "231DFA.h"
namespace llvm {

class LivenessInfo : public Info {
public:
	LivenessInfo() = default;
	LivenessInfo(const LivenessInfo& other) :
		Info(other),
		insts(other.insts)
	{
	}
	virtual ~LivenessInfo() override = default;

	/*
	 * Compare two pieces of information
	 *
	 * Direction:
	 *   In your subclass you need to implement this function.
	 */
	static bool equals(LivenessInfo* info1, LivenessInfo* info2) {
		return info1->insts == info2->insts;
	}
	/*
	 * Join two pieces of information.
	 * The third parameter points to the result.
	 *
	 * Direction:
	 *   In your subclass you need to implement this function.
	 */
	static void join(LivenessInfo* info1, LivenessInfo* info2, LivenessInfo* result) {
		if (!result) {
			return;
		}
		std::unordered_set<unsigned> temp;
		if (info1) {
			temp.insert(info1->insts.cbegin(), info1->insts.cend());
		}
		if (info2) {
			temp.insert(info2->insts.cbegin(), info2->insts.cend());
		}
		result->insts = std::move(temp);
	}

	/*
	* Print out the information
	*
	* Direction:
	*   In your subclass you should implement this function according to the project specifications.
	*/
	virtual void print() override {
		for (unsigned index : insts) {
			errs() << index << '|';
		}
		errs() << '\n';
	}

	std::unordered_set<unsigned> insts;
};

class LivenessAnalysis : public DataFlowAnalysis<LivenessInfo, false> {
public:
	LivenessAnalysis(LivenessInfo& bottom, LivenessInfo& initialState) :
		DataFlowAnalysis<LivenessInfo, false>(bottom, initialState)
	{
	}

	virtual ~LivenessAnalysis() override = default;

private:
	static const std::unordered_set<unsigned> opCodes;

	virtual void flowfunction(Instruction* I,
							  std::vector<unsigned>& IncomingEdges,
							  std::vector<unsigned>& OutgoingEdges,
							  std::vector<LivenessInfo*>& Infos) override
	{
		if (!I) {
			return;
		}
		unsigned curIndex = InstrToIndex[I];
		LivenessInfo outInfo;
		for (unsigned preIndex : IncomingEdges) {
			Edge incomingEdge = std::make_pair(preIndex, curIndex);
			LivenessInfo::join(EdgeToInfo[incomingEdge], &outInfo, &outInfo);
		}

		if (!isa<PHINode>(I)) {
			for (auto opIter = I->op_begin(); opIter != I->op_end(); ++opIter) {
				Value* val = opIter->get();
				if (isa<Instruction>(val)) {
					Instruction* pInstr = cast<Instruction>(val);
					if (InstrToIndex.count(pInstr)) {
						outInfo.insts.insert(InstrToIndex[pInstr]);
					}
				}
			}
			if (I->isBinaryOp() ||
				I->isShift() ||
				I->isBitwiseLogicOp() ||
				opCodes.count(I->getOpcode()))
			{
				outInfo.insts.erase(InstrToIndex[I]);
			}
			for (unsigned nexIndex : OutgoingEdges) {
				Infos.push_back(new LivenessInfo);
				Edge outgoingEdge = std::make_pair(curIndex, nexIndex);
				LivenessInfo::join(&outInfo, EdgeToInfo[outgoingEdge], Infos.back());
			}
		}
		else {
			for (auto pInstr = I; isa<PHINode>(pInstr); pInstr = pInstr->getNextNode()) {
				outInfo.insts.erase(InstrToIndex[pInstr]);
				if (pInstr->isTerminator()) {
					break;
				}
			}
			for (unsigned nexIndex : OutgoingEdges) {
				Instruction* nexInstr = IndexToInstr[nexIndex];
				Infos.push_back(new LivenessInfo);
				Edge outgoingEdge{ curIndex,nexIndex };
				LivenessInfo::join(&outInfo, EdgeToInfo[outgoingEdge], Infos.back());

				for (auto pInstr = I; isa<PHINode>(pInstr); pInstr = pInstr->getNextNode()) {
					PHINode* phi = cast<PHINode>(pInstr);
					for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k) {
						if (phi->getIncomingBlock(k) == nexInstr->getParent() && isa<Instruction>(phi->getIncomingValue(k))) {
							Instruction* val = cast<Instruction>(phi->getIncomingValue(k));
							Infos.back()->insts.insert(InstrToIndex[val]);
						}
					}
					if (pInstr->isTerminator()) {
						break;
					}
				}
				
			}
		}

	}

};

const std::unordered_set<unsigned> LivenessAnalysis::opCodes{
	Instruction::Alloca,
	Instruction::Load,
	Instruction::GetElementPtr,
	Instruction::ICmp,
	Instruction::FCmp,
	Instruction::PHI,
	Instruction::Select
};

namespace {

struct LivenessAnalysisPass : public FunctionPass {
	static char ID;
	LivenessAnalysisPass() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function& func) override {
		LivenessInfo bottom;
		LivenessAnalysis(bottom, bottom).runWorklistAlgorithm(&func);

		return false;
	}
};
}

char LivenessAnalysisPass::ID = 0;
static RegisterPass<LivenessAnalysisPass> cse231_liveness(
	"cse231-liveness",
	"cse231-liveness",
	false,
	false);

}