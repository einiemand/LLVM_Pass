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
	explicit LivenessInfo(const std::unordered_set<unsigned>& instrSet) :
		insts(instrSet)
	{
	}
	LivenessInfo& operator=(const LivenessInfo& other) {
		insts = other.insts;
		return *this;
	}
	virtual ~LivenessInfo() override = default;
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

	virtual const std::unordered_set<unsigned>& getEdgeInfo() const override {
		return insts;
	}

	virtual std::unordered_set<unsigned>& getEdgeInfo() override {
		return insts;
	}

private:
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
						outInfo.getEdgeInfo().insert(InstrToIndex[pInstr]);
					}
				}
			}
			if (I->isBinaryOp() ||
				I->isShift() ||
				I->isBitwiseLogicOp() ||
				opCodes.count(I->getOpcode()))
			{
				outInfo.getEdgeInfo().erase(InstrToIndex[I]);
			}
		}

		Infos.clear();
		for (unsigned nexIndex : OutgoingEdges) {
			Infos.push_back(new LivenessInfo);
			Edge outgoingEdge = std::make_pair(curIndex, nexIndex);
			LivenessInfo::join(&outInfo, EdgeToInfo[outgoingEdge], Infos.back());
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