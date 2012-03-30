#ifndef UADFREACHDEFANAL_HDR
#define UADFREACHDEFANAL_HDR

//Reaching definitions
namespace BA {
     template <class Policy>
     class ReachDefAnalysis : public RWAnalysis<Policy> {
          private:
               struct DefUseEntry {
                    SgAsmStatement *stm;
                    map<RegisterRep, unsigned long int> regMap;

                    DefUseEntry() {stm = NULL;}
                    DefUseEntry(SgAsmStatement *stm) {this->stm = stm;}

                    void _print(ostream &o, ReachDefAnalysis &p) const {
                         o << stm <<" {";
                         for(map<RegisterRep, unsigned long int>::const_iterator it = regMap.begin();
                             it != regMap.end();
                             ++it)
                              o << "(" << p.RWAnalysis<Policy>::getRegisterName(it->first) << ", " << it->second << "), ";
                         o << "}";
                    }
               };

               map<SgAsmStatement *, DefUseEntry *> duIds; //one-to-one
               map<unsigned long int, DefUseEntry *> duIdsLookup; //many-to-one
               unsigned long int currDUId;

               map<SgAsmStatement *, set<unsigned long int> *> outMap, inMap, genMap, killMap;
          protected:
               //Functions
                 unsigned int getDUIdByReg(set<unsigned long int> &duIds, RegisterRep r, set<unsigned long int> &foundDuIds) {
                    map<RegisterRep, unsigned long int>::iterator itF;
                    for(set<unsigned long int>::iterator it = duIds.begin(); it != duIds.end(); ++it) {
                         ROSE_ASSERT(duIdsLookup.find(*it) != duIdsLookup.end());
                         DefUseEntry *p = duIdsLookup.find(*it)->second;
                         if( ((itF = p->regMap.find(r)) != p->regMap.end()) &&
                             (itF->second == *it) )
                              foundDuIds.insert(*it);
                    }
                    return foundDuIds.size();
                 }
                 unsigned long int getDUId(RegisterRep r, SgAsmStatement *stm) {
                    DefUseEntry *p = NULL;
                    if(duIds.find(stm) == duIds.end()) {
                         p = new DefUseEntry(stm);
                         duIds[stm] = p;
                    } else
                         p = duIds.find(stm)->second;
                    typename map<RegisterRep, unsigned long int>::iterator it = p->regMap.find(r);
                    if(it == p->regMap.end()) {
                         //cout << "Adding (" << currDUId << ": (" << stm << ", " << r << "))" << endl;
                         p->regMap[r] = currDUId;
                         duIdsLookup[currDUId] = p;
                         return currDUId++;
                    } else
                         return it->second;
                 }


                 void _print(ostream &o) {
                    o << "Reaching-Def Analysis" << endl << "-----" << endl << "DefUseID's: ";
                    for(typename map<SgAsmStatement *, DefUseEntry *>::iterator it = duIds.begin(); it != duIds.end(); ++it) {
                         it->second->_print(o, *this);
                         o << ", ";
                    }
                    o << endl << "KILL: ";
                    for(map<SgAsmStatement *, set<unsigned long int> *>::iterator it = killMap.begin(); it != killMap.end(); ++it) {
                         o << "(" << it->first << ", {";
                         for(set<unsigned long int>::iterator it2 = it->second->begin(); it2 != it->second->end(); ++it2)
                              o << *it2 << ", ";
                         o << "}), ";
                    }
                    o << endl << "GEN: ";
                    for(map<SgAsmStatement *, set<unsigned long int> *>::iterator it = genMap.begin(); it != genMap.end(); ++it) {
                         o << "(" << it->first << ", {";
                         for(set<unsigned long int>::iterator it2 = it->second->begin(); it2 != it->second->end(); ++it2)
                              o << *it2 << ", ";
                         o << "}), ";
                    }
                    o << endl << "OUT: ";
                    for(map<SgAsmStatement *, set<unsigned long int> *>::iterator it = outMap.begin(); it != outMap.end(); ++it) {
                         o << "(" << it->first << ", {";
                         for(set<unsigned long int>::iterator it2 = it->second->begin(); it2 != it->second->end(); ++it2)
                              o << *it2 << ", ";
                         o << "}), ";
                    }
                    o << endl << "IN: ";
                    for(map<SgAsmStatement *, set<unsigned long int> *>::iterator it = inMap.begin(); it != inMap.end(); ++it) {
                         o << "(" << it->first << ", {";
                         for(set<unsigned long int>::iterator it2 = it->second->begin(); it2 != it->second->end(); ++it2)
                              o << *it2 << ", ";
                         o << "}), ";
                    }
                    o << endl << "-----" << endl;
                    RWAnalysis<Policy>::_print(o);
                 }

                 virtual bool _analyzePost(CFG cfg) {
                    //Initialize...
                      for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
                         SgAsmBlock *bb = get(boost::vertex_name, cfg, *vP.first);
                         outMap[bb] = new set<unsigned long int>();
                         inMap[bb] = new set<unsigned long int>();
                      }
                    //Iterate till fix-point is reached...
                      for(bool changed = true; changed; ) {
                         changed = false;
                         for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
                              SgAsmBlock *bbCur = get(boost::vertex_name, cfg, *vP.first);
                              //Compute gen & kill...
                                (*this)(bbCur);
                              //Compute data-flow equation (out[bb] = gen[bb] U (in[bb] - kill[bb]))
                                set<unsigned long int> *in = inMap[bbCur], *out = outMap[bbCur], outCopy(out->begin(), out->end()), 
                                                       *gen = genMap[bbCur], *kill = killMap[bbCur], rIds;
                                for(pair<CFGIEIter, CFGIEIter> pP = boost::in_edges(*vP.first, cfg); pP.first != pP.second; ++pP.first) {
                                    set<unsigned long int> *i = outMap[ get(boost::vertex_name, cfg, source(*pP.first, cfg)) ];
                                    in->insert(i->begin(), i->end());
                                }
                                out->clear();
                                out->insert(gen->begin(), gen->end());
                                for(set<RegisterRep>::iterator it = in->begin(); it != in->end(); ++it)
                                   if(getDUIdByReg(*kill, *it, rIds) == 0) {
                                        out->insert(*it);
                                        rIds.clear();
                                   }
                              changed |= !RWAnalysis<Policy>::equal(*out, outCopy);
                         }
                      }

                    return true;
                 }
                 virtual bool _analyzePost(SgNode *node) {
                    //Handle each node...
                      switch(node->variantT()) {
                        case V_SgProject:
                        case V_SgAsmFunction:
                              return (*this)(BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(node));
                        case V_SgAsmBlock: {
                              SgAsmBlock *bb = static_cast<SgAsmBlock *>(node);
                              if( (genMap.find(bb) != genMap.end()) &&
                                  (killMap.find(bb) != killMap.end()) )
                                   return true;
                              //Allocate gen & kill
                                set<unsigned long int> *gen = new set<unsigned long int>(), *kill = new set<unsigned long int>(), rIds;
                                genMap[bb] = gen;
                                killMap[bb] = kill;
                              //Collect gen & kill
                                set<RegisterRep> wrS;
                                for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin();
                                    it != bb->get_statementList().end();
                                    ++it) {
                                        //Get whic registers that are read or written
                                          wrS.clear();
                                          if( !(RWAnalysis<Policy>::get_writers(*it, wrS)) )
                                                  return false;
                                        //Update gen & kill
                                          for(set<RegisterRep>::iterator it2 = wrS.begin(); it2 != wrS.end(); ++it2) {
                                             unsigned long int duId = getDUId(*it2, *it);
                                             //See if r has already been defined...
                                               if(getDUIdByReg(*gen, *it2, rIds) > 0) {
                                                  kill->insert(rIds.begin(), rIds.end());
                                                  rIds.clear();
                                               }
                                             gen->insert(duId);                                     
                                          }
                                }
                         } return true;
                        case V_SgAsmInstruction:
                        case V_SgAsmArmInstruction:
                        case V_SgAsmPowerpcInstruction:
                        case V_SgAsmx86Instruction:  
                           RWAnalysis<Policy>::_analyzePost(node);  
                        default:
                           return true;
                      }
                 }
               
                 bool getGen(SgAsmStatement *stm, set<RegisterRep> &genSet) {
                    if(stm == NULL)
                         return false;
                    genSet.clear();

                    switch(stm->variantT()) {
                     case V_SgAsmInstruction:
                     case V_SgAsmArmInstruction:
                     case V_SgAsmPowerpcInstruction:
                     case V_SgAsmx86Instruction: {
                         typename map<SgAsmStatement *, DefUseEntry *>::iterator itS = duIds.find(stm);
                         if(itS == duIds.end())
                              _analyzePost(stm);
                         for(map<RegisterRep, unsigned long int>::iterator it = itS->second->regMap.begin();
                             it != itS->second->regMap.end();
                             ++it)
                              genSet.insert(it->first);
                      } return true;
                     case V_SgProject:
                     case V_SgAsmFunction:
                     case V_SgAsmBlock: {
                         map<SgAsmStatement *, set<unsigned long int> *>::iterator itS = genMap.find(stm);
                         if(itS == genMap.end())
                              _analyzePost(stm);
                         map<RegisterRep, unsigned long int>::iterator itF;
                         for(set<unsigned long int>::iterator it = itS->second->begin(); it != itS->second->end(); ++it) {
                              ROSE_ASSERT(duIdsLookup.find(*it) != duIdsLookup.end());
                              DefUseEntry *p = duIdsLookup.find(*it)->second;
                              for(map<RegisterRep, unsigned long int>::iterator it2 = p->regMap.begin(); it2 != p->regMap.end(); ++it2)
                                   if(it2->second == *it) {
                                        genSet.insert(it2->first);
                                        break;
                                   }
                         }
                      } return true;
                     default:
                         return false;
                    }
                 }
                 bool getKill(SgAsmStatement *stm, set<RegisterRep> &killSet) {
                    if(stm == NULL)
                         return false;
                    killSet.clear();

                    switch(stm->variantT()) {
                     case V_SgAsmInstruction:
                     case V_SgAsmArmInstruction:
                     case V_SgAsmPowerpcInstruction:
                     case V_SgAsmx86Instruction: {
                         
                      } return false;
                     case V_SgProject:
                     case V_SgAsmFunction:
                     case V_SgAsmBlock: {
                         map<SgAsmStatement *, set<unsigned long int> *>::iterator itS = killMap.find(stm);
                         if(itS == killMap.end())
                              _analyzePost(stm);
                         map<RegisterRep, unsigned long int>::iterator itF;
                         for(set<unsigned long int>::iterator it = itS->second->begin(); it != itS->second->end(); ++it) {
                              ROSE_ASSERT(duIdsLookup.find(*it) != duIdsLookup.end());
                              DefUseEntry *p = duIdsLookup.find(*it)->second;
                              for(map<RegisterRep, unsigned long int>::iterator it2 = p->regMap.begin(); it2 != p->regMap.end(); ++it2)
                                   if(it2->second == *it) {
                                        killSet.insert(it2->first);
                                        break;
                                   }
                         }
                      } return true;
                     default:
                         return false;
                    }
                 }
          public:
               //Constructor
                 ReachDefAnalysis(const ReachDefAnalysis &rd) : RWAnalysis<Policy>(rd.getPolicyBlock().get_filter()) {
                    //Set state...
                      currDUId = rd.getCurrDUId();
                 }
                 ReachDefAnalysis(typename RWAnalysis<Policy>::FilterFunctor *f = NULL) : RWAnalysis<Policy>(f) {currDUId = 0;}
               //Getters
                 unsigned long int getCurrDUId() {return currDUId;}
     };
};
#endif
