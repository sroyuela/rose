#ifndef UADFLIVEANAL_HDR
#define UADFLIVEANAL_HDR

namespace BA {
     template <class Policy>
     class LivenessAnalysis : public RWAnalysis<Policy> {
          private:
               map<SgAsmStatement *, set<RegisterRep> *> outMap, inMap;
               
          protected:
               //Functions
                 virtual bool _analyzePost(CFG cfg) {
                    //Initialize...
                      for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
                         SgAsmBlock *bb = get(boost::vertex_name, cfg, *vP.first);
                         outMap[bb] = new set<RegisterRep>();
                         inMap[bb] = new set<RegisterRep>();
                      }
                    //Iterate till fix-point is reached...
                      for(bool changed = true; changed; ) {
                         changed = false;
                         for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
                              SgAsmBlock *bbCur = get(boost::vertex_name, cfg, *vP.first);
                              //Compute def & use (readers & writers)...
                                set<RegisterRep> *in = inMap[bbCur], *out = outMap[bbCur], inCopy(in->begin(), in->end()), rdS, wrS;
                                RWAnalysis<Policy>::_analyzePost(bbCur);
                                if( !(RWAnalysis<Policy>::get_readers(bbCur, rdS)) ||
                                    !(RWAnalysis<Policy>::get_writers(bbCur, wrS)) )
                                    return false;
                              //Compute data-flow equation (in[bb] = use[bb] U (out[bb] - def[bb]))
                                for(pair<CFGOEIter, CFGOEIter> pP = boost::out_edges(*vP.first, cfg); pP.first != pP.second; ++pP.first) {
                                    set<RegisterRep> *in = inMap[get(boost::vertex_name, cfg, target(*pP.first, cfg))];
                                    out->insert(in->begin(), in->end());
                                }
                                in->clear();
                                in->insert(rdS.begin(), rdS.end());
                                for(set<RegisterRep>::iterator it = out->begin(); it != out->end(); ++it)
                                   if(wrS.find(*it) == wrS.end())
                                        in->insert(*it);
                              changed |= !RWAnalysis<Policy>::equal(*in, inCopy);
                         }
                      }
                    return true;
                 }
                 virtual bool _analyzePost(SgNode *node) {
                    //Handle each node...
                      switch(node->variantT()) {
                        case V_SgProject:
                        case V_SgAsmFunction:
                        case V_SgAsmBlock:
                              return _analyzePost(BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(node));
                        case V_SgAsmInstruction:
                        case V_SgAsmArmInstruction:
                        case V_SgAsmPowerpcInstruction:
                        case V_SgAsmx86Instruction:  
                           RWAnalysis<Policy>::_analyzePost(node);  
                        default:
                           return true;
                      }
                 }

                 void _print(ostream &o) {
                    set<SgAsmBlock *> presented;
                    o << "Liveness Analysis" << endl << "-----" << endl;
                    //Print out the live in
                      o << "IN: {";
                      for(map<SgAsmStatement *, set<RegisterRep> *>::iterator it = inMap.begin(); it != inMap.end(); ++it) {
                          o << "(" << it->first << ", {";
                          for(set<RegisterRep>::iterator lIt = it->second->begin(); lIt != it->second->end(); ++lIt)
                              o << RWAnalysis<Policy>::getRegisterName(*lIt) << ", ";
                          o << "}), ";
                      }
                      o << "}" << endl;
                    //Print out the live out
                      o << "OUT: {";
                      for(map<SgAsmStatement *, set<RegisterRep> *>::iterator it = outMap.begin(); it != outMap.end(); ++it) {
                          o << "(" << it->first << ", {";
                          for(set<RegisterRep>::iterator lIt = it->second->begin(); lIt != it->second->end(); ++lIt)
                              o << RWAnalysis<Policy>::getRegisterName(*lIt) << ", ";
                          o << "}), ";
                      }
                      o << "}" << endl;
                  o << "-----" << endl; 
                  RWAnalysis<Policy>::_print(o);  
                 }
          public:
               //Constructors...
                 LivenessAnalysis(const LivenessAnalysis &lA) : RWAnalysis<Policy>(lA.getPolicyBlock().get_filter()) {
                    set<SgAsmStatement *> s;
                    //Copy outmap...
                      lA.getOutSet(s);
                      for(set<SgAsmStatement *>::iterator it = s.begin(); it != s.end(); ++it) {
                          set<RegisterRep> *outSet = lA.getLiveOutSet(*it), *outSetC = new set<RegisterRep>(outSet->begin(), outSet->end());
                          outMap[*it] = outSet;
                      }
                      s.clear();
                    //Copy inmap...
                      lA.getInSet(s);
                      for(set<SgAsmStatement *>::iterator it = s.begin(); it != s.end(); ++it) {
                          set<RegisterRep> *inSet = lA.getLiveInSet(*it), *inSetC = new set<RegisterRep>(inSet->begin(), inSet->end());
                          inMap[*it] = inSetC;
                      }                  
                 }
                 LivenessAnalysis(typename RWAnalysis<Policy>::FilterFunctor *f = NULL) : RWAnalysis<Policy>(f) {}
              
               //State getters
                 void getOutSet(set<SgAsmStatement *> &r) {
                    for(map<SgAsmStatement *, set<RegisterRep> *>::iterator it = outMap.begin();
                        it != outMap.end();
                        ++it)
                         r.insert(it->first);
                 }
                 void getInSet(set<SgAsmStatement *> &r) {
                    for(map<SgAsmStatement *, set<RegisterRep> *>::iterator it = inMap.begin();
                        it != inMap.end();
                        ++it)
                         r.insert(it->first);
                 }
                 set<RegisterRep> *getLiveInSet(SgAsmStatement *stm) {
                    if(inMap.find(stm) == inMap.end())
                         return NULL;
                    else
                         return inMap.find(stm)->second;
                 }
                 set<RegisterRep> *getLiveOutSet(SgAsmStatement *stm) {
                    if(outMap.find(stm) == outMap.end())
                         return NULL;
                    else
                         return outMap.find(stm)->second;
                 }
     };
};

#endif

