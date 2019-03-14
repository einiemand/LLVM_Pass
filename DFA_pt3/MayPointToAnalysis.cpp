#include "231DFA.h"

namespace llvm {

class MayPointToInfo : public Info {
public:
	MayPointToInfo() = default;
	MayPointToInfo(const MayPointToInfo& other) :
		Info(other),
		pointTo(other.pointTo)
	{
	}
	virtual ~MayPointToInfo() override = default;
	/*
	* Print out the information
	*
	* Direction:
	*   In your subclass you should implement this function according to the project specifications.
	*/
	virtual void print() override {
		for (const auto& item : pointTo) {
			if (!item.second.empty()) {
				unsigned ptr = item.first;
				errs() << 'R' << ptr << "->(";
				for (unsigned pte : item.second) {
					errs() << 'M' << pte << '/';
				}
				errs() << ")|";
			}
		}
		errs() << '\n';
	}

	/*
	 * Compare two pieces of information
	 *
	 * Direction:
	 *   In your subclass you need to implement this function.
	 */
	static bool equals(MayPointToInfo* info1, MayPointToInfo* info2) {
		return info1->pointTo == info2->pointTo;
	}
	/*
	 * Join two pieces of information.
	 * The third parameter points to the result.
	 *
	 * Direction:
	 *   In your subclass you need to implement this function.
	 */
	static void join(MayPointToInfo* info1, MayPointToInfo* info2, MayPointToInfo* result) {
		if (!result) {
			return;
		}
		std::unordered_map<unsigned, std::unordered_set<unsigned>> temp;
		if (info1) {
			temp.insert(info1->pointTo.cbegin(), info1->pointTo.cend());
		}
		if (info2) {
			temp.insert(info2->pointTo.cbegin(), info2->pointTo.cend());
		}
		result->pointTo = std::move(temp);
	}

	std::unordered_map<unsigned, std::unordered_set<unsigned>> pointTo;
};

class MayPointToAnalysis : public DataFlowAnalysis<MayPointToInfo, true> {
public:
	MayPointToAnalysis(MayPointToInfo& bottom, MayPointToInfo& initialState) :
		DataFlowAnalysis<MayPointToInfo, true>(bottom, initialState)
	{
	}

	virtual ~MayPointToAnalysis() override = default;
private:
	static const std::unordered_set<unsigned> opCodes;

	virtual void flowfunction(Instruction* I,
							  std::vector<unsigned>& IncomingEdges,
							  std::vector<unsigned>& OutgoingEdges,
							  std::vector<MayPointToInfo*>& Infos) override
	{
		if (!I) {
			return;
		}
		unsigned curIndex = InstrToIndex[I];
		MayPointToInfo outInfo;
		auto& curInfo = outInfo.pointTo;
		for (unsigned preIndex : IncomingEdges) {
			Edge incomingEdge = std::make_pair(preIndex, curIndex);
			MayPointToInfo::join(EdgeToInfo[incomingEdge], &outInfo, &outInfo);
		}
		unsigned opcode = I->getOpcode();

		if (opcode == Instruction::Alloca) {
			curInfo[curIndex].insert(curIndex);
		}
		else if (opcode == Instruction::BitCast || opcode == Instruction::GetElementPtr) {
			Instruction* operand = cast<Instruction>(I->getOperand(0));
			unsigned idx = InstrToIndex[operand];
			for (unsigned pte : curInfo[idx]) {
				curInfo[curIndex].insert(pte);
			}
		}
		// else if (opcode == Instruction::Load) {
		// 	if (isa<Instruction>(I->getOperand(0))) {
		// 		Instruction* operand = cast<Instruction>(I->getOperand(0));
		// 		unsigned ptr = InstrToIndex[operand];
		// 		for (unsigned pte1 : curInfo[ptr]) {
		// 			for (unsigned pte2 : curInfo[pte1]) {
		// 				curInfo[curIndex].insert(pte2);
		// 			}
		// 		}
		// 	}
		// }
		else if (opcode == Instruction::Store) {
			Instruction* val = cast<Instruction>(I->getOperand(0));
			Instruction* ptr = cast<Instruction>(I->getOperand(1));
			unsigned idx1 = InstrToIndex[val], idx2 = InstrToIndex[ptr];
			for (unsigned y : curInfo[idx2]) {
				for (unsigned x : curInfo[idx1]) {
					curInfo[y].insert(x);
				}
			}
		}
		else if (opcode == Instruction::Select) {
			Instruction* val1 = cast<Instruction>(I->getOperand(1));
			Instruction* val2 = cast<Instruction>(I->getOperand(2));
			unsigned idx1 = InstrToIndex[val1], idx2 = InstrToIndex[val2];
			for (unsigned pte : curInfo[idx1]) {
				curInfo[curIndex].insert(pte);
			}
			for (unsigned pte : curInfo[idx2]) {
				curInfo[curIndex].insert(pte);
			}
		}
		else if (opcode == Instruction::PHI) {
			for (auto pInstr = I; isa<PHINode>(pInstr); pInstr = pInstr->getNextNode()) {
				PHINode* phi = cast<PHINode>(pInstr);
				for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k) {
					if (isa<Instruction>(phi->getIncomingValue(k))) {
						Instruction* val = cast<Instruction>(phi->getIncomingValue(k));
						unsigned idx = InstrToIndex[val];
						for (unsigned pte : curInfo[idx]) {
							curInfo[curIndex].insert(pte);
						}
					}
				}
				if (pInstr->isTerminator()) {
					break;
				}
			}
		}
		for (unsigned nexIndex : OutgoingEdges) {
			Edge outgoingEdge{ curIndex,nexIndex };
			Infos.push_back(new MayPointToInfo);
			MayPointToInfo::join(&outInfo, EdgeToInfo[outgoingEdge], Infos.back());
		}
	}

};

namespace {

struct MayPointToAnalysisPass : public FunctionPass {
	static char ID;
	MayPointToAnalysisPass() : FunctionPass(ID) {}

	virtual bool runOnFunction(Function& func) override {
		MayPointToInfo bottom;
		MayPointToAnalysis(bottom, bottom).runWorklistAlgorithm(&func);

		return false;
	}
};
}

char MayPointToAnalysisPass::ID = 0;
static RegisterPass<MayPointToAnalysisPass> cse231_maypointto(
	"cse231-maypointto",
	"cse231-maypointto",
	false,
	false);

}