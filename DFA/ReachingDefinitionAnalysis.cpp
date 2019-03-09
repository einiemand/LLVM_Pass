#include "231DFA.h"

namespace llvm {

class ReachingInfo : public Info {
public:
	ReachingInfo() = default;
	ReachingInfo(const ReachingInfo& other) :
		Info(other),
		insts(other.insts)
	{
	}
	explicit ReachingInfo(const std::unordered_set<unsigned>& instrSet) :
		insts(instrSet)
	{
	}
	ReachingInfo& operator=(const ReachingInfo& other) {
		insts = other.insts;
		return *this;
	}
	virtual ~ReachingInfo() override = default;
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

class ReachingDefinitionAnalysis : public DataFlowAnalysis<ReachingInfo, true> {

public:
	ReachingDefinitionAnalysis(ReachingInfo& bottom, ReachingInfo& initialState) :
		DataFlowAnalysis<ReachingInfo, true>(bottom, initialState)
	{
	}

	virtual ~ReachingDefinitionAnalysis() override = default;

private:
	static const std::unordered_set<unsigned> opCodes;

	/*
	* The flow function.
	*   Instruction I: the IR instruction to be processed.
	*   std::vector<unsigned> & IncomingEdges: the vector of the indices of the source instructions of the incoming edges.
	*   std::vector<unsigned> & OutgoingEdges: the vector of indices of the source instructions of the outgoing edges.
	*   std::vector<Info *> & Infos: the vector of the newly computed information for each outgoing eages.
	*
	*/
	virtual void flowfunction(Instruction* I,
							  std::vector<unsigned>& IncomingEdges,
							  std::vector<unsigned>& OutgoingEdges,
							  std::vector<ReachingInfo*>& Infos) override
	{
		unsigned curIndex = InstrToIndex[I];
		ReachingInfo outInfo;
		for (unsigned preIndex : IncomingEdges) {
			Edge incomingEdge = std::make_pair(preIndex, curIndex);
			ReachingInfo::join(EdgeToInfo[incomingEdge], &outInfo, &outInfo);
		}
		if (I && (I->isBinaryOp() ||
				  I->isShift() ||
				  I->isBitwiseLogicOp() ||
				  opCodes.count(I->getOpcode()))) {
			outInfo.getEdgeInfo().insert(curIndex);
		}

		Infos.clear();
		for (unsigned nexIndex : OutgoingEdges) {
			Infos.push_back(new ReachingInfo);
			Edge outgoingEdge = std::make_pair(curIndex, nexIndex);
			join(&outInfo, EdgeToInfo[outgoingEdge], Infos.back());
		}
	}
};

const std::unordered_set<unsigned> ReachingDefinitionAnalysis::opCodes{
	Instruction::Alloca,
	Instruction::Load,
	Instruction::GetElementPtr,
	Instruction::ICmp,
	Instruction::FCmp,
	Instruction::PHI,
	Instruction::Select
};

namespace {

struct ReachingDefinitionAnalysisPass : public FunctionPass {
	static char ID;
	ReachingDefinitionAnalysisPass() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function& func) override {
		ReachingInfo bottom;
		ReachingDefinitionAnalysis(bottom, bottom).runWorklistAlgorithm(&func);

		return false;
	}
};
}

char ReachingDefinitionAnalysisPass::ID = 0;
static RegisterPass<ReachingDefinitionAnalysisPass> cse231_reaching(
	"cse231-reaching",
	"cse231-reaching",
	false,
	false);

}
