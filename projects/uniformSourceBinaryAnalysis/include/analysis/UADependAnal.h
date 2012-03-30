#ifndef UADEP_ANAL
#define UADEP_ANAL


namespace BA {
     template <class Policy>
     class DependencyAnalysis : public RWAnalysis<Policy> {
          public:
               struct DEPENDENCE_TYPE {
                    bool FLOW;    //RAW
                    bool ANTI;    //WAR
                    bool OUTPUT;  //WAW
                    bool INPUT;   //RAR (for scheduling purposes)

                    DEPENDENCE_TYPE() {
                         this->FLOW = false;
                         this->ANTI = false;
                         this->OUTPUT = false;
                         this->INPUT = false;
                    }
                    DEPENDENCE_TYPE(bool INPUT, bool FLOW, bool ANTI, bool OUTPUT) {
                         this->FLOW = FLOW;
                         this->ANTI = ANTI;
                         this->OUTPUT = OUTPUT;
                         this->INPUT = INPUT;
                    }
                    DEPENDENCE_TYPE(const DEPENDENCE_TYPE &c) {*this = c;}
                    bool isDependent() {return (this->FLOW || this->ANTI || this->OUTPUT || this->INPUT);}
                    DEPENDENCE_TYPE& operator=(const DEPENDENCE_TYPE &c) {
                         this->FLOW = c.FLOW;     this->ANTI = c.ANTI;     this->OUTPUT = c.OUTPUT;      this->INPUT = c.INPUT;
                         return *this;
                    }
                    
               };
               struct FilterFunctor {
                    virtual bool operator()(DEPENDENCE_TYPE &t) {return true;}
               };

               typedef boost::property<boost::vertex_name_t, SgAsmInstruction*> DepGraphVName;
               typedef boost::property<boost::edge_name_t, DEPENDENCE_TYPE> DepGraphEName;
               typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, DepGraphVName, DepGraphEName> DependencyGraph;
               typedef typename boost::graph_traits<DependencyGraph>::vertex_descriptor DepGraphVertex; 
               typedef typename boost::graph_traits<DependencyGraph>::vertex_iterator DepGraphVertexIter;
               typedef typename boost::graph_traits<DependencyGraph>::edge_descriptor DepGraphEdge;
               typedef typename boost::graph_traits<DependencyGraph>::edge_iterator DepGraphEdgeIter;

               DEPENDENCE_TYPE getDepType(const DependencyGraph &g, const SgAsmInstruction *x, const SgAsmInstruction *y) {
                    DepGraphVertex vX = boost::graph_traits<DependencyGraph>::null_vertex(), 
                                   vY = boost::graph_traits<DependencyGraph>::null_vertex();
                    //Find which vertex the two instructions belong to...
                      for(pair<DepGraphVertexIter, DepGraphVertexIter> vP = vertices(g); vP.first != vP.second; ++vP.first) {
                         if(get(boost::vertex_name, g, *vP.first) == x)
                              vX = *vP.first;
                         if(get(boost::vertex_name, g, *vP.first) == y)
                              vY = *vP.first;
                      }
                      if( (vX == boost::graph_traits<DependencyGraph>::null_vertex()) ||
                          (vY == boost::graph_traits<DependencyGraph>::null_vertex()) )
                              return DEPENDENCE_TYPE();
                    //Find edge...
                      if(!edge(vX, vY, g).second)
                         return DEPENDENCE_TYPE(false, false, false, false);
                      else {
                         typename boost::property_map<DependencyGraph, boost::edge_name_t>::type edgeDepMap = get(boost::edge_name, g);
                         return edgeDepMap[edge(vX, vY, g).first];
                      }
               }
          private:
               struct Comp : public std::binary_function<pair<SgAsmInstruction *, SgAsmInstruction *>, pair<SgAsmInstruction *, SgAsmInstruction *>, bool> {
                    bool operator()(const pair<SgAsmInstruction *, SgAsmInstruction *> &p1, const pair<SgAsmInstruction *, SgAsmInstruction *> &p2) const {
                         if( ((p1.first == p2.first) && (p1.second == p2.second)) ||
                             ((p1.first == p2.second) && (p1.second == p2.first)) )
                              return true;
                         else
                             return false;
                    }
               };
               struct Hash : public std::unary_function<pair<SgAsmInstruction *, SgAsmInstruction *>, size_t> {
                    size_t operator()(const pair<SgAsmInstruction *, SgAsmInstruction *> &p) const {
                         return (size_t) p.first + (size_t) p.second;
                    }
               };
               boost::unordered_map< pair<SgAsmInstruction *, SgAsmInstruction *>, DEPENDENCE_TYPE, Hash, Comp> depMap;
               FilterFunctor *f;
          protected:
               //Functions
                 void _print(ostream &o) {
                    o << "Dependency Analysis" << endl << "-----" << endl;
                    for(typename boost::unordered_map< pair<SgAsmInstruction *, SgAsmInstruction *>, DEPENDENCE_TYPE, Hash, Comp>::iterator it = depMap.begin(); 
                        it != depMap.end();
                        ++it) {
                         o << it->first.first << " -> " << it->first.second << " {";
                         if(it->second.FLOW)    o << "FLOW, ";
                         if(it->second.ANTI)    o << "ANTI, ";
                         if(it->second.OUTPUT)  o << "OUTPUT, ";
                         if(it->second.INPUT)   o << "INPUT, ";
                         o << "}" << endl;
                    }
                    o << "-----" << endl;
                    RWAnalysis<Policy>::_print(o);
                 }

                 virtual bool _analyzePost(CFG cfg) {
                    if(num_vertices(cfg) > 1)
                         throw BAException("Dependency analysis can only handle basic-block");

                    bool results = true;
                    for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first)
                         results &= _analyzePost( get(boost::vertex_name, cfg, *vP.first) );

                    return results;
                 }
                 virtual bool _analyzePost(SgNode *node) {
                    //Handle each node...
                      switch(node->variantT()) {
                        case V_SgProject:
                        case V_SgAsmFunction:
                              throw BAException("Dependency analysis can't handle function or project nodes");
                        case V_SgAsmBlock: {
                              SgAsmBlock *bb = static_cast<SgAsmBlock *>(node);

                              map<RegisterRep, SgAsmInstruction*> lastRead, lastWrite;
                              map<RegisterRep, SgAsmInstruction*>::iterator lastIt;
                              typename boost::unordered_map< pair<SgAsmInstruction *, SgAsmInstruction *>, DEPENDENCE_TYPE, Hash, Comp>::iterator itDep;
                              for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin();
                                  it != bb->get_statementList().end();
                                  ++it) {
                                        SgAsmInstruction *ins = isSgAsmInstruction(*it);
                                        if(ins == NULL)
                                             continue;
                                        //Get which registers that are read or written
                                          set<RegisterRep> rdS, wrS;
                                          if( !RWAnalysis<Policy>::get_writers(ins, wrS) ||
                                              !RWAnalysis<Policy>::get_readers(ins, rdS) )
                                              return false;
                                        //Deduce dependencies...
                                          //Read-after-X
                                            for(set<RegisterRep>::iterator itR = rdS.begin(); itR != rdS.end(); ++itR) {
                                              //RAR
                                                DEPENDENCE_TYPE d = DEPENDENCE_TYPE(true, false, false, false);
                                                if( ((lastIt = lastRead.find(*itR)) != lastRead.end()) && (lastIt->second != ins) &&
                                                    (f == NULL ? true : (*f)(d)) && d.isDependent()) {
                                                   pair<SgAsmInstruction *, SgAsmInstruction *> p = pair<SgAsmInstruction *, SgAsmInstruction *>(lastIt->second, ins);
                                                   if( (itDep = depMap.find(p)) == depMap.end() )
                                                       depMap[p] = d;
                                                   else
                                                       itDep->second.INPUT = d.INPUT;
                                                }
                                              //RAW
                                                d = DEPENDENCE_TYPE(false, true, false, false);
                                                if( ((lastIt = lastWrite.find(*itR)) != lastWrite.end()) && (lastIt->second != ins) &&
                                                    (f == NULL ? true : (*f)(d)) && d.isDependent()) {
                                                   pair<SgAsmInstruction *, SgAsmInstruction *> p = pair<SgAsmInstruction *, SgAsmInstruction *>(lastIt->second, ins);
                                                   if( (itDep = depMap.find(p)) == depMap.end() )
                                                       depMap[p] = d;
                                                   else
                                                       itDep->second.FLOW = d.FLOW;
                                                }
                                            }
                                          //Write-after-X
                                            for(set<RegisterRep>::iterator itW = wrS.begin(); itW != wrS.end(); ++itW) {
                                              //WAR
                                                DEPENDENCE_TYPE d = DEPENDENCE_TYPE(false, false, true, false);
                                                if( ((lastIt = lastRead.find(*itW)) != lastRead.end()) && (lastIt->second != ins) &&
                                                    (f == NULL ? true : (*f)(d)) && d.isDependent()) {
                                                   pair<SgAsmInstruction *, SgAsmInstruction *> p = pair<SgAsmInstruction *, SgAsmInstruction *>(lastIt->second, ins);
                                                   if( (itDep = depMap.find(p)) == depMap.end() )
                                                       depMap[p] = d;
                                                   else
                                                       itDep->second.ANTI = d.ANTI;

                                                   lastRead.erase(lastIt);
                                                }
                                              //WAW
                                                d = DEPENDENCE_TYPE(false, false, false, true);
                                                if( ((lastIt = lastWrite.find(*itW)) != lastWrite.end()) && (lastIt->second != ins) &&
                                                    (f == NULL ? true : (*f)(d)) && d.isDependent()) {
                                                   pair<SgAsmInstruction *, SgAsmInstruction *> p = pair<SgAsmInstruction *, SgAsmInstruction *>(lastIt->second, ins);
                                                   if( (itDep = depMap.find(p)) == depMap.end() )
                                                       depMap[p] = d;
                                                   else
                                                       itDep->second.OUTPUT = d.OUTPUT;
                                                }
                                            }
                                        //Update lastWrite & lastRead...
                                          for(set<RegisterRep>::iterator itR = rdS.begin(); itR != rdS.end(); ++itR)
                                             lastRead[*itR] = ins;
                                          for(set<RegisterRep>::iterator itW = wrS.begin(); itW != wrS.end(); ++itW)
                                             lastWrite[*itW] = ins;
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
          public:
               DependencyAnalysis(const DependencyAnalysis &dA) : RWAnalysis<Policy>(dA) {
                    //Set internal state...
                      f = dA.get_filter();
                    //Copy dependency information...
                      DependencyGraph gDD = dA.getDependencyGraph();
                      typename boost::property_map<DependencyGraph, boost::edge_name_t>::type edgeDepMap = get(boost::edge_name, gDD);
                      typename boost::property_map<DependencyGraph, boost::vertex_name_t>::type vertexDepMap = get(boost::vertex_name, gDD);
                      for(pair<DepGraphEdgeIter, DepGraphEdgeIter> eP = edges(gDD); eP.first != eP.second; ++eP.first) {
                             SgAsmInstruction *u = vertexDepMap[source(*eP.first, gDD)], *v = vertexDepMap[target(*eP.first, gDD)];
                             depMap[pair<SgAsmInstruction *, SgAsmInstruction *>(u, v)] = edgeDepMap[*eP.first];
                      }
               }
               DependencyAnalysis(FilterFunctor *f1 = NULL, typename RWAnalysis<Policy>::FilterFunctor *f2 = NULL) : RWAnalysis<Policy>(f2) {f = f1;}

               FilterFunctor *get_filter() {return f;}

               DependencyGraph getDependencyGraph(SgAsmBlock *bb) {
                    //Make sure test has been performed...
                      for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin();
                          it != bb->get_statementList().end();
                          ++it) {
                              SgAsmInstruction *ins = isSgAsmInstruction(*it);
                              if(ins == NULL)
                                   continue;
                              if(depMap.find(ins) == depMap.end()) {
                                   (*this)(bb);
                                   break;
                              }
                      }
                    //Build dependency graph...
                      DependencyGraph gDD;
                      typename boost::property_map<DependencyGraph, boost::edge_name_t>::type edgeDepMap = get(boost::edge_name, gDD);
                      typename boost::property_map<DependencyGraph, boost::vertex_name_t>::type vertexDepMap = get(boost::vertex_name, gDD);
                      map<SgAsmInstruction *, DepGraphVertex> vertexMap;
                      //Add all nodes...
                        for(typename boost::unordered_map< pair<SgAsmInstruction *, SgAsmInstruction *>, DEPENDENCE_TYPE, Hash, Comp>::iterator it = depMap.begin();
                            it != depMap.end(); 
                            ++it) {
                            //Add node...
                              DepGraphVertex v = boost::graph_traits<DependencyGraph>::null_vertex(),
                                             u = boost::graph_traits<DependencyGraph>::null_vertex();
                              if(vertexMap.find(it->first.first) == vertexMap.end()) {
                                   v = add_vertex(gDD);
                                   vertexMap[it->first.first] = v;
                              }
                              if(vertexMap.find(it->first.second) == vertexMap.end()) {
                                   u = add_vertex(gDD);
                                   vertexMap[it->first.second] = u;
                              }
                            //Add edge
                              add_edge( v, u, it->second );                 
                        }

                    return gDD;
               }
     };
};

#endif
