#ifndef BINARY_TRANSFORM_HEADER
#define BINARY_TRANSFORM_HEADER

#include <set>
#include <string>
#include <functional>

#include "rose.h"
#include "binarySemantics.h"
//#include "BinaryControlFlow.h"
#include "BinaryLoader.h"
#include "SymbolicSemantics.h"
#include "YicesSolver.h"

#include "binaryAnalysis.h"
#include "binaryAbstract.h"
#include "regAllocLS.h"
#include "schedulerList.h"

using namespace std;

namespace BT {

     template <class Policy> class BinaryTransform {
          private:
                 typedef typename Policy::InstSemantics InstSemantics;

     		  SchInterface<Policy> &schInst;
                 RegAllocInterface<Policy> &regAllocInst;
                 YicesSolver smtS;
                 Policy semPolicyBlock;

                 set<SgAsmBlock *> bbs;
               //Virtual register handler...
                 struct RegState : public std::binary_function<RegState, RegState, bool> {
                      enum {
                         VIRTUAL_REGISTER = 0,
                         PHYSICAL_REGISTER = 1
                      } regType;

                      virtual bool operator()(const RegState &x, const RegState &y) = 0;
                 };
                 struct RegStatePhysical : public RegState {
                         unsigned long int r;
                         struct RegStateVirtual *shadowReg;

                         RegStatePhysical(unsigned long int r, RegStateVirtual *shadowReg) {
                              this->r = r;
                              this->shadowReg = shadowReg;
                         }
                         bool operator()(const RegState &x, const RegState &y) {
                              if(x.regType == y.regType)
                                   return (static_cast<RegStatePhysical>(x).r < 
                                           static_cast<RegStatePhysical>(y).r);
                              else
                                  return (x.regType < y.regType);
                         }
                 };
                 struct RegStateVirtual : public RegState {
                    unsigned long int vRegIndex;
                    struct RegStatePhysical *physicalReg;

                    RegStateVirtual(unsigned long int vRegIndex, RegStatePhysical *physicalReg) {
                         this->vRegIndex = vRegIndex;
                         this->physicalReg = physicalReg;
                    }
                    bool operator()(const RegState &x, const RegState &y) {
                         if(x.regType == y.regType)
                              return (static_cast<RegStateVirtual>(x).vRegIndex < 
                                      static_cast<RegStateVirtual>(y).vRegIndex);
                         else
                             return (x.regType < y.regType);
                    }
                 };
                 set<RegState> RegStateMap;
                 unsigned long int currentVRegIndex;

                 map<SgAsmInstruction *, map<unsigned long int, pair<RegState, bool> > *> nodeRegMap;
                 //map<pair<SgNode *, SgNode *>, BA::DependencyAnalysis<Policy>::DEPENDENCE_TYPE, >
                 //map<SgAsmBlock *, pair<BA::DependencyAnalysis<Policy>::

               //Private functions
                 RegStateVirtual allocateVRegState(RegStatePhysical &p) {
                    ROSE_ASSERT(p.shadowReg == NULL);
                    return RegStateVirtual(currentVRegIndex++, &p);
                 }

                 template<class T> T *copyNode(T &x) {return new T(x);}
          public:
               BinaryTransform(const BinaryTransform<Policy> &bT) : schInst(bT.getScheduler()), regAllocInst(bT.getRegAlloc()), semPolicyBlock(&smtS) {
                    //Initialize semantics layer
                      smtS.set_linkage(YicesSolver::LM_EXECUTABLE);
                    //Initialize state
                      currentVRegIndex = Policy::getNrRegs();
               }
               BinaryTransform(SchInterface<Policy> &sch_ = List_Scheduler<Policy>(), RegAllocInterface<Policy> &reg_ = RegAllocLS<Policy>()) 
                         : schInst(sch_), regAllocInst(reg_), semPolicyBlock(&smtS) {
                    //Initialize semantics layer
                      smtS.set_linkage(YicesSolver::LM_EXECUTABLE);
                    //Initialize state
                      currentVRegIndex = Policy::getNrRegs();
               }
               ~BinaryTransform() {}

               //Getters
                 SchInterface<Policy> &getScheduler() {return schInst;}
                 RegAllocInterface<Policy> &getRegAlloc() {return regAllocInst;}                

               //User functions
                 //Analyze functions...
                   
                 //Basic block
                   SgAsmBlock *buildBasicBlock() {
                      SgAsmBlock *bb = new SgAsmBlock(0);
                      bbs.insert(bb);
                      return bb;
                   }
                   bool insertStatement(SgAsmBlock *bb, SgAsmFunction *func) {
                         return false;
                   }
                 //Statement

			//Operators
                 bool operator()(SgAsmInstruction *i, SgAsmInstruction *reference, 
                                 PreprocessingInfo::RelativePositionType position=PreprocessingInfo::after,  
                                 bool allowBreakingDependency = false) {
                    //Error check inputs
                      switch(position) {
                       case PreprocessingInfo::before:                                                   break;
                       case PreprocessingInfo::after:                                                    break;
                       case PreprocessingInfo::after_syntax:     position = PreprocessingInfo::after;    break;
                       case PreprocessingInfo::before_syntax:    position = PreprocessingInfo::before;   break;
                       case PreprocessingInfo::defaultValue:     
                       case PreprocessingInfo::undef:            position = PreprocessingInfo::after;    break;
                       case PreprocessingInfo::inside:                                                   break;
                      }
                      //Make sure no dependencies are violated if not explicitly allowed...
                        if(!allowBreakingDependency) {
                           //If before check all incoming dependencies, if after check all outgoing dependencies...
                             //switch(
                        }
                      //Make sure i has been added...
                        if(nodeRegMap.find(i) == nodeRegMap.end())
                           i = (*this)(i);
                 }
                 SgAsmInstruction *operator()(SgAsmInstruction *i) { //Update i with new regs if neccessary
                    //Execute instruction...
                      semPolicyBlock.processInstruction(i);
                    //Check state changes...
                      set<unsigned long int> rReg = semPolicyBlock.get_rReg(), wReg = semPolicyBlock.get_wReg();
                      set<SymbolicSemantics::ValueType<32> > rMem = semPolicyBlock.get_rMem(), wMem = semPolicyBlock.get_wMem();
                    //If nothing has changed return original
                      ROSE_ASSERT(wMem.size() == 0); //TODO! Handle mem
                      map<unsigned long int, pair<RegState, bool> > *regVMapping = new map<unsigned long int, pair<RegState, bool> >();
                      for(set<unsigned long int>::iterator it = rReg.begin(); it != rReg.end(); ++it) {
                         typename set<RegState>::iterator itF = RegStateMap.find(RegStatePhysical(*it, NULL));
                         if(itF != RegStateMap.end()) {
                              RegStatePhysical rP = static_cast<RegStatePhysical>(*itF);
                              RegState rState = (rP.shadowReg == NULL ? rP : *(rP.shadowReg));
                              (*regVMapping)[*it] = pair<RegState, bool>(rState, true); // true denotes reading
                         }
                      }
                      for(set<unsigned long int>::iterator it = wReg.begin(); it != wReg.end(); ++it) {
                         typename set<RegState>::iterator itF = RegStateMap.find(RegStatePhysical(*it, NULL));
                         if(itF != RegStateMap.end()) {
                              RegStatePhysical rP = static_cast<RegStatePhysical>(*itF);
                              RegState rState = (rP.shadowReg == NULL ? allocateVRegState(rP) : *(rP.shadowReg));
                              (*regVMapping)[*it] = pair<RegState, bool>(rState, false); //False denotes writing
                         } else {
                              RegStatePhysical rP = RegStatePhysical(*it, NULL);
                              RegStateVirtual rV = allocateVRegState(rP);
                              rP.shadowReg = &rV;
                              RegStateMap.insert(rP);
                              (*regVMapping)[*it] = pair<RegState, bool>(rV, false); //False denotes writing
                         }
                      } 
                      if(regVMapping->size() == 0)
                         return i;
                    //Add instruction to collection...
                      nodeRegMap[i] = regVMapping;
                    
                    return i;
                 }
     };
};

#endif
