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
				errs() << item.first.first << item.first.second << "->(";
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
		return (!info1 && !info2) ||
		       (info1 && info2 && info1->pointTo == info2->pointTo);
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
		std::map<std::pair<char, unsigned>, std::unordered_set<unsigned>> temp_pointTo;
		if (info1) {
			temp_pointTo.insert(info1->pointTo.cbegin(), info1->pointTo.cend());
		}
		if (info2) {
			temp_pointTo.insert(info2->pointTo.cbegin(), info2->pointTo.cend());
		}
		result->pointTo = std::move(temp_pointTo);
	}

	std::map<std::pair<char, unsigned>, std::unordered_set<unsigned>> pointTo;
};

class MayPointToAnalysis : public DataFlowAnalysis<MayPointToInfo, true> {
public:
	MayPointToAnalysis(MayPointToInfo& bottom, MayPointToInfo& initialState) :
		DataFlowAnalysis<MayPointToInfo, true>(bottom, initialState)
	{
	}

	virtual ~MayPointToAnalysis() override = default;
private:
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
		auto& pointTo_info = outInfo.pointTo;

		for (unsigned preIndex : IncomingEdges) {
			Edge incomingEdge = std::make_pair(preIndex, curIndex);
			MayPointToInfo::join(EdgeToInfo[incomingEdge], &outInfo, &outInfo);
		}

		unsigned opcode = I->getOpcode();

		if (opcode == Instruction::Alloca) {
			pointTo_info[{ 'R',curIndex }].insert(curIndex);
		}
		else if (opcode == Instruction::BitCast || opcode == Instruction::GetElementPtr) {
			Instruction* RvInstr = cast<Instruction>(I->getOperand(0));
			unsigned Rv = InstrToIndex[RvInstr];
			const auto& pointees = pointTo_info[{ 'R',Rv }];
			pointTo_info[{ 'R',curIndex }].insert(pointees.cbegin(), pointees.cend());
		}
		else if (opcode == Instruction::Load) {
			LoadInst* loadInstr = cast<LoadInst>(I);
			Instruction* RpInstr = cast<Instruction>(loadInstr->getPointerOperand());
			unsigned Rp = InstrToIndex[RpInstr];

			for (unsigned X : pointTo_info[{ 'R',Rp }]) {
				const auto& Ys = pointTo_info[{ 'M',X }];
				pointTo_info[{ 'R',curIndex }].insert(Ys.cbegin(), Ys.cend());
			}
		}
		else if (opcode == Instruction::Store) {
			StoreInst* storeInstr = cast<StoreInst>(I);
			Instruction* RvInstr = cast<Instruction>(storeInstr->getValueOperand());
			Instruction* RpInstr = cast<Instruction>(storeInstr->getPointerOperand());

			unsigned Rv = InstrToIndex[RvInstr], Rp = InstrToIndex[RpInstr];
			const auto& Xs = pointTo_info[{ 'R',Rv }];

			for (unsigned Y : pointTo_info[{ 'R',Rp }]) {
				pointTo_info[{ 'M',Y }].insert(Xs.cbegin(), Xs.cend());
			}
		}
		else if (opcode == Instruction::Select) {
			SelectInst* selectInstr = cast<SelectInst>(I);
			Instruction* R1Instr = cast<Instruction>(selectInstr->getTrueValue());
			Instruction* R2Instr = cast<Instruction>(selectInstr->getFalseValue());

			unsigned R1 = InstrToIndex[R1Instr];
			const auto& Xs1 = pointTo_info[{ 'R',R1 }];
			pointTo_info[{ 'R',curIndex }].insert(Xs1.cbegin(), Xs1.cend());

			unsigned R2 = InstrToIndex[R2Instr];
			const auto& Xs2 = pointTo_info[{ 'R',R2 }];
			pointTo_info[{ 'R',curIndex }].insert(Xs2.cbegin(), Xs2.cend());
		}
		else if (opcode == Instruction::PHI) {
			for (auto pInstr = I; isa<PHINode>(pInstr); pInstr = pInstr->getNextNode()) {
				PHINode* phi = cast<PHINode>(pInstr);
				for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k) {
					Instruction* RkInstr = cast<Instruction>(phi->getIncomingValue(k));
					unsigned Rk = InstrToIndex[RkInstr];
					
					const auto& Xs = pointTo_info[{ 'R',Rk }];
					pointTo_info[{ 'R',curIndex }].insert(Xs.cbegin(), Xs.cend());
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