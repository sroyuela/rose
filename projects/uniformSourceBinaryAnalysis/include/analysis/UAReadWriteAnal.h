#ifndef UARWANAL_HDR
#define UARWANAL_HDR

#include <set>
#include <map>
#include <string>

#include "analysis/UABase.h"
#include "analysis/binaryAnalysis.h"

namespace BA {
     template <class Policy>
     class RWAnalysis : public AnalysisBase {
          private:
     		//RW struct containers...
                 struct RWStruct {
                    set<RegisterRep> readMap;
                    set<RegisterRep> writeMap;

                    RWStruct() {}
                    RWStruct(set<RegisterRep> &readMap, set<RegisterRep> &writeMap) {
                              this->readMap(readMap.begin(), readMap.end());
                              this->writeMap(writeMap.begin(), writeMap.end());
                    }

                    void _print(ostream &o, RWAnalysis &r) {
                         o << "READ {";
                         for(set<RegisterRep>::iterator it = readMap.begin(); it != readMap.end(); ++it)
                             o << r.getRegisterName(*it) << ", ";
                         o << "}, WRITE {";
                         for(set<RegisterRep>::iterator it = writeMap.begin(); it != writeMap.end(); ++it)
                             o << r.getRegisterName(*it) << ", ";
                         o << "}";
                    }
                 };
                 map<SgAsmStatement *, RWStruct *> rwMap;

               //Semantics
                 typedef typename Policy::InstSemantics InstSemantics;
                 YicesSolver smtS;
                 Policy semPolicyBlock;
          protected:
               //Functions
                 string getRegisterName(RegisterRep v) {
                    return semPolicyBlock.toString(v);
                 }
                 void UnionDiff(set<RegisterRep> &u, const set<RegisterRep> &x, const set<RegisterRep> &y) const {
                    //Computes U = `Union`(U, `Difference`(X, Y))
                      for(set<RegisterRep>::iterator it = x.begin(); it != x.end(); ++it)
                         if(y.find(*it) == y.end())
                              u.insert(*it);
                 }
                 bool equal(const set<unsigned long int> &x, set<unsigned long int> &y) const {
                    if(x.size() != y.size())
                         return false;
                    for(set<unsigned long int>::iterator it = x.begin(), itF; it != x.end(); ++it)
                         if((itF = y.find(*it)) == y.end())
                              return false;
                         else
                              y.erase(itF);
                    return true;
                 }


               virtual bool _analyzePost(SgNode *node) {
                    typename map<SgAsmStatement *, RWStruct *>::iterator itF;
                    switch(node->variantT()) {
                     case V_SgAsmBlock: {
                       SgAsmBlock *bb = static_cast<SgAsmBlock *>(node);
                       if(rwMap.find(bb) != rwMap.end())
                         return true;
                       RWStruct *dus = new RWStruct();
                       for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin(); it != bb->get_statementList().end(); ++it)
                           if((itF = rwMap.find(*it)) != rwMap.end()) {
                              UnionDiff(dus->readMap, itF->second->readMap, dus->writeMap);
                              dus->writeMap.insert( itF->second->writeMap.begin(), itF->second->writeMap.end() );
                           } else
                                 ROSE_ASSERT(false);
                           rwMap[bb] = dus;
                       } return true;
                     case V_SgAsmInstruction:
                     case V_SgAsmArmInstruction:
                     case V_SgAsmPowerpcInstruction:
                     case V_SgAsmx86Instruction: {
                         SgAsmInstruction *i = static_cast<SgAsmInstruction *>(node);
                         if(rwMap.find(i) != rwMap.end())
                            return true;
                         //Execute instruction...
                           semPolicyBlock.processInstruction(i);
                         //Check state changes...
                           set<RegisterRep> rReg = semPolicyBlock.get_rReg(), wReg = semPolicyBlock.get_wReg();
                           set<SymbolicSemantics::ValueType<32>, typename Policy::SVComp> rMem = semPolicyBlock.get_rMem(), wMem = semPolicyBlock.get_wMem();
                         //Update graph with a vertex...
                           RWStruct *dus = new RWStruct();
                           dus->writeMap.insert(wReg.begin(), wReg.end());
                           dus->readMap.insert(rReg.begin(), rReg.end());
                           rwMap[i] = dus;
                       } return true; 
                     case V_SgAsmFunction:
                     case V_SgProject:                  
                     default:
                            return false;
                    }
               }

               void _print(ostream &o) {
                    o << "RW Analysis" << endl << "-----" << endl;
                    for(typename map<SgAsmStatement *, RWStruct *>::iterator it = rwMap.begin(); it != rwMap.end(); ++it) {
                        o << it->first << " (";
                        switch(it->first->variantT()) {
                          case V_SgAsmBlock:
                          case V_SgAsmFunction:
                          case V_SgAsmStaticData:
                              o << it->first->class_name() << " at " << it->first << ")";
                              break;
                          case V_SgAsmInstruction:
                          case V_SgAsmArmInstruction:
                          case V_SgAsmPowerpcInstruction:
                          case V_SgAsmx86Instruction:
                              o << "'" << unparseInstructionWithAddress(static_cast<SgAsmInstruction *>(it->first)) << "')";
                              break;
                        }
                        o << ": ";
                        it->second->_print(o, *this);
                        o << endl;
                    }
                    o << "-----" << endl;
               }
          public:
               //Functor to filter out certain registers / memory locations
               struct FilterFunctor : public Policy::FilterFunctor {
                      virtual bool operator()(unsigned long int i) const {
                         return true;
                      }
                      virtual bool operator()(SymbolicSemantics::ValueType<32> addr) const {
                         return false;
                      }
               };

               //Constructor
               RWAnalysis(const RWAnalysis &rw) : semPolicyBlock(&smtS, rw.getPolicyBlock().get_filter()) {
                    set<SgAsmStatement *> *stmSet = rw.getStmSet();
                    for(set<SgAsmStatement *>::iterator it = stmSet->begin(); it != stmSet->end(); ++it) {
                         set<RegisterRep> writersMap, readersMap;
                         //Fetch R/W of statement...
                           if(!rw.get_readers(*it) || !rw.get_writers(*it))
                              continue;
                         //Create RWstruct...
                           rwMap[*it] = new RWStruct(readersMap, writersMap);
                    }
               }
               RWAnalysis(FilterFunctor *f) : semPolicyBlock(&smtS, f) {
                    //Initialize semantics layer
                      smtS.set_linkage(YicesSolver::LM_EXECUTABLE);
               }

               //State getters
               Policy &getPolicyBlock() {return semPolicyBlock;}
               set<SgAsmStatement *> *getStmSet() {
                    set<SgAsmStatement *> *stmSet = new set<SgAsmStatement *>();
                    for(typename map<SgAsmStatement *, RWStruct *>::iterator it = rwMap.begin();
                        it != rwMap.end();
                        ++it)
                         stmSet->insert(it->first);
                    return stmSet;
               }

               //Returns which registers that are read in a statement
               bool get_readers(SgAsmStatement *stm, set<RegisterRep> &readersMap) {
                    typename map<SgAsmStatement *, RWStruct *>::iterator itF = rwMap.find(stm);
                    if(itF == rwMap.end())
                         return false;
                    readersMap.insert(itF->second->readMap.begin(), itF->second->readMap.end());
                    return true;
               }
               //Returns which registers that are written in a statement
               bool get_writers(SgAsmStatement *stm, set<RegisterRep> &writersMap) {
                    typename map<SgAsmStatement *, RWStruct *>::iterator itF = rwMap.find(stm);
                    if(itF == rwMap.end())
                         return false;
                    writersMap.insert(itF->second->writeMap.begin(), itF->second->writeMap.end());
                    return true;
               }
     };
};

#endif
