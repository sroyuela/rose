#ifndef UADATAFLOWANAL_HDR
#define UADATAFLOWANAL_HDR

//Data-flow analysis
/*template <class LatticeT, class SetT, class SetIdent, class ElementT>
class DataFlowAnalysis {
     public:
          typedef unsigned long int ElemIndexRep;

          //Set representations as containers for elements...
            class SetBit {
               private:
                    LatticeT *parentL;
                    boost::dynamic_bitset<> s_;
               public:
                    //Constructor...
                      SetBit(boost::dynamic_bitset<> &b_) : s_(b_) {}
                      SetBit(LatticeT *parentL_, set<ElemIndexRep> &setS, ElemIndexRep maxE) : s_(maxE, false) {
                          for(set<ElemIndexRep>::iterator itS = setS.begin(); itS != setS.end(); ++itS)
                              s_[*itS] = true;
                          this->parentL = parentL_;
                      }
                      virtual ~SetBit() {
                         this->parentL->registerSet(this);
                      }
                    //Functions
                      std::size_t size() {return s_.size();}
                      void allocate(set<ElemIndexRep> &setS, ElemIndexRep min, ElemIndexRep max) {
                         if(max > size())
                            s_.resize(max-size(), false); 
                      }
                      boost::dynamic_bitset<> &set() {return s_;}
                    //Operators
                      //Meet
                      virtual SetBit<LatticeT, SetT, SetIdent, ElementT> &operator&(const SetBit &x, const SetBit &y) {
                         if(x.size() != y.size())
                            throw BA::BAException("LHS and RHS doesn't model the same set");
                         else {
                               boost::dynamic_bitset<> c_(s_);
                               c_ &= s_;
                               return c_;
                         }
                      }
                      //Join
                      virtual friend SetBit &operator|(const SetBit &x, const SetBit &y) {
                         if(x.size() != y.size())
                            throw BA::BAException("LHS and RHS doesn't model the same set");
                         else {
                               boost::dynamic_bitset<> c_(s_);
                               c_ |= y.set();
                               return SetBool(c_);
                         }
                      }
            };
            class SetSet {
               private:
                    LatticeT *parentL;
                    set<ElemIndexRep> s_;
               public:
                    //Constructor
                      SetSet(LatticeT *parentL_, set<ElemIndexRep> &setS, ElemIndexRep maxE) : s_(setS.begin(), setS.end()) {
                         this->parentL = parentL_;
                      }
                      virtual ~SetSet() {
                         this->parentL->deregisterSet(this);
                      }
                    //Functions
                      set<ElemIndexRep> &set() {return s_;}
                      void allocate(set<ElemIndexRep> &setS, ElemIndexRep min, ElemIndexRep max) {}
                    //Operators
                      //Meet
                      virtual friend SetSet &operator&(const SetSet &x, const SetSet &y) {
                         set<ElemIndexRep> s;
                         for(set<ElemIndexRep>::iterator it = y.set().begin(); it != y.set().end(); ++it)
                             if(s_.find(*it) != s_.end())
                                s.insert(*it);
                         return SetSet(this->parentL, s);
                      }
                      //Join
                      virtual friend SetBool &operator|(const SetSet &x, const SetSet &y) {
                         set<ElemIndexRep> s(s_.begin(), s_.end());
                         s.insert(y.set().begin(), y.set().end());
                         return SetSet(this->parentL, s);
                      }
            };
          //Finite lattice implementation...
            class FiniteLattice {
               private:
                    ElemIndexRep elementIndex;
                    boost::bimap<ElemIndexRep, ElementT> distinctElemSet;
                    map<SetT *, SetIdent> elemSetTtI;
                    map<SetIdent, vector<SetT *> *> elemSetItT;
               public:
                    //Constructors...
                      Lattice() {
                         //Initialize state...
                           this->elementIndex = 0;
                      }
                      virtual ~Lattice() {
                         for(map<SetT *, SetIdent>::iterator it = elemSetTtI.begin(); it != elemSetTtI.end(); ++it)
                             delete it->first;
                         for(map<SetIdent, vector<SetT *> *>::iterator it = elemSet.begin(); it != elemSet.end(); ++it)
                             delete it->second;
                      }
                    //Functions
                      virtual boost::bimap<ElemIndexRep, ElementT> &getElementMapping() {return distinctElemSet;}
                      virtual void deregisterSet(SetT *s) {
                         map<SetT *, SetIdent>::iterator it = elemSetTtI.find(s);
                         if(it == elemSetTtI.end())
                            return;
                         //Remove set in I -> T
                           vector<SetT *> *vS = elemSetItT[it->second];
                           if(vS.size() == 1)
                              vS->erase(vS->begin());
                           else
                              for(vector<SetT *>::iterator it = vS->begin(); it != vS->end(); ++it)
                                  if(*it == s) {
                                     vS->erase(*it);
                                     break;
                                  }
                         //Remove set in T -> I
                           elemSetTtI.erase(it);
                      }
                      virtual unsigned int addSet(SetIdent i, set<ElementT> &s) {
                              //Translate elements to indexes...
                                set<ElemIndexRep> setS;
                                ElemIndexRep elemId;
                                for(set<ElementT>::iterator it = s.begin(); it != s.end(); ++it) {
                                    addElement(*it, elemId);
                                    setS.insert(elemId);
                                }
                              //Create new set...
                                return addSet(i, setS);
                      }
                      virtual unsigned int addSet(SetIdent i, set<ElemIndexRep> &s) {
	                         //Create new set...
                                SetT *sI = new SetT(this, setS, elementIndex); 
                                elemSet[sI] = i;
                              //Make sure ident is not already registred...
                                map<SetIdent, vector<SetT *> *>::iterator it;
                                if((it = elemSetItT.find(i)) != elemSetItT.end()) {
                                   it->second->push_back(sI);
                                   return it->second->size()-1;
                                } else {
                                   vector<SetT *> *vecS = new vector<SetT *>();
                                   vecS->push_back(sI);
                                   elemSetItT[i] = vecS;
                                   return 0;
                                }
                      }
                      virtual bool addElement(ElementT t, ElemIndexRep &index) {
                         boost::bimap<ElemIndexRep, ElementT>::right_map::iterator it = distinctElemSet.right.find(t);
                         if(it == distinctElemSet.right.end()) {
                            index = it->second;
                            return false;
                         }

                         index = elementIndex++;
                         distinctElemSet[index] = t;
                         return true;
                      }
                    //Operator
                      virtual SetT &bottom() = 0;
                      virtual SetT &top() = 0;
            };
     private:
          LatticeT lat;
     protected:
          //Typedefs / structs
            typedef enum {
               DF_DIR_FORWARD,
               DF_DIR_BACKWARD,
               DF_DIR_BOTH,
            } DIRECTION dir;

            struct PredicateF {
               virtual bool operator()(ElementT t) = 0;
            };

          //(De)constructors
            DataFlowAnalysis(DIRECTION dir_) : dir(dir_) {
            
            }
            ~DataFlowAnalysis() {
          
            }
          //Operators
            bool operator()(CFG cfg, GVertex entry, GVertex exit) {
               //Initialize 
            }
          //Abstract functions
            virtual unsigned int createSet(SetIdent i, set<ElementT> &s = set<ElementT>()) {return lat.addSet(i, s);}  
            virtual unsigned int createSet(SetIdent i, PredicateF &pF) {
               set<unsigned long int> s;
               for(boost::bimap<unsigned long int, ElementT>::left_iterator it = lat.getElementMapping().left.begin();
                   it != lat.getElementMapping().left.end();
                   ++it)
                    if(pF(it->second))
                       s.insert(it->first);
               return lat.addSet(i, s);
            }
            virtual ElemIndexRep addElement(ElementT t) {
                    ElemIndexRep eIndex;
                    lat.addElement(t, eIndex);
                    return eIndex;
            }
};

template <class Policy, class SetT = DataFlowAnalysis::SetBit>
class ReachingDefAnal : public RWAnalysis<Policy>, 
                        public DataFlowAnalysis<DataFlowAnalysis::FiniteLattice, SetT, SgAsmBlock *, RegisterRep> {
     private:
          map<SgAsmStatement *, unsigned int> genMap, killMap;
          struct DefPred : public DataFlowAnalysis<DataFlowAnalysis::FiniteLattice, SetT, SgAsmBlock *, RegisterRep>::PredicateF {
               RegisterRep &r;
               DefPred(RegisterRep &r_) : r(r_) {}
               bool operator()(RegisterRep r_) {
                    if(r_ == r)
                         return true;
                    else
                         return false;
               }
          };
     protected:
            //Transfer function implementation
              bool 
            //Collect GEN & KILL set...
            virtual bool _analyzePost(SgNode *node) {
               //Handle each node...
                 switch(node->variantT()) {
                   case V_SgProject:
                   case V_SgAsmFunction:
                         //Collect gen & kill...
                           for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
                              (*this)(get(boost::vertex_name, cfg, *vP.first));
                         //Find solution to DF problem...
                           return (*this)(BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(node));
                   case V_SgAsmBlock: {
                         SgAsmBlock *bb = static_cast<SgAsmBlock *>(node);
                         if( (genMap.find(bb) != genMap.end()) &&
                             (killMap.find(bb) != killMap.end()) )
                              return true;
                         //Allocate gen & kill
                           set<RegisterRep> *gen = new set<RegisterRep>(), *kill = new set<RegisterRep>(), rIds;
                           genMap[bb] = gen;
                           killMap[bb] = kill;
                         //Collect gen & kill
                           set<RegisterRep> wrS;
                           map<RegisterRep, 
                           for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin();
                               it != bb->get_statementList().end();
                               ++it) {
                                   //Get which registers that are read or written
                                     wrS.clear();
                                     if( !(RWAnalysis<Policy>::get_writers(*it, wrS)) )
                                         return false;
                                   //Update gen & kill
                                     for(set<RegisterRep>::iterator it2 = wrS.begin(); it2 != wrS.end(); ++it2) {
                                        //Add element...
                                          ElemIndexRep eIndex = addElement(*it2);
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

     public:

};*/

#endif
