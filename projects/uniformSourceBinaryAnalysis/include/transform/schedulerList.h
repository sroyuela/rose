#ifndef LIST_SCHEDULER_HEADER
#define LIST_SCHEDULER_HEADER

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/copy.hpp>

#include "binaryAnalysis.h"
#include "binaryAbstract.h"

#include <queue>
#include <vector>

using namespace std;

//Generic List scheduler, simplified version of that mentioned in Muchnick's
//"Advanced Compiler Design and Implementation".
namespace BT {
     
     //List scheduler main class
       template <class Policy> class List_Scheduler : public SchInterface<Policy>  {
          public:
               typedef vector<unsigned long int> ResourceVector;

               //Functors used in List_Scheduler...
                 struct LSHeurFunc {
                    //State initialization
                      virtual bool initState(ResourceVector &v) = 0;
                    //Operators...
                      virtual unsigned long int operator()(SgNode *i) = 0; //operator() returns the priority of the instruction                    
                 };
                 struct LSRescFunc  {
                    //State initialization
                      virtual bool initState(ResourceVector &v) = 0;
                    //Get instruction to use when no other works...
                      virtual SgNode *getStallInstruction() = 0;
                    //Operators...
                      virtual bool operator[](SgNode *i) = 0; //operator[] is called to confirm which instruction that was scheduled.
                      virtual bool operator()(SgNode *i) = 0; //operator() is called to check whether instruction can be scheduled.                    
                 };
               //Default implementations
                 //Resource constrain functors
                   struct LSRescIgnore : public LSRescFunc {
                         bool initState(ResourceVector &v) {} //ignore resource vector
                         SgNode *getStallInstruction() {return SageBuilderAsm::buildx86Instruction(x86_nop);}

                         bool operator()(SgNode *i) {return true;} //No hardware constrains on scheduling
                         bool operator[](SgNode *i) {return true;} //State can always be updated...
                   };
                 //Heuristics functor
                   struct LSHeurUnitPrio : public LSHeurFunc {
                         bool initState(ResourceVector &v) {} //ignore resource vector
                         unsigned long int operator()(SgNode *i) {return 0;}
                   };
          private:
               typedef typename BA::DependencyAnalysis<Policy>::DependencyGraph BBDepGraph;
               typedef typename BA::DependencyAnalysis<Policy>::DepGraphVertex BBDepVertex; 
               typedef typename BA::DependencyAnalysis<Policy>::DepGraphVertexIter BBDepVIter;

               struct prioQComp : public std::binary_function<BBDepVIter, BBDepVIter, bool> {
                    typename boost::property_map<BBDepGraph, boost::vertex_name_t>::type &vertMap;
                    std::map<SgAsmInstruction *, unsigned long int> &prioQMap;         
               
                    prioQComp(typename boost::property_map<BBDepGraph, boost::vertex_name_t>::type &vM, 
                              std::map<SgAsmInstruction *, unsigned long int> &mPQ) : prioQMap(mPQ), vertMap(vM) {}
                    bool operator()(const BBDepVertex x, const BBDepVertex y) const {
                          SgAsmInstruction *insX = vertMap[x], *insY = vertMap[y];
                          if(prioQMap[insX] < prioQMap[insY])
                             return true;
                          else
                              return false;
                    }
               };

               LSHeurFunc &hF;
               LSRescFunc &rC;
               BA::DependencyAnalysis<Policy> &DA;
          public:
               List_Scheduler() : hF(LSHeurUnitPrio()), rC(LSRescIgnore()), DA(BA::DependencyAnalysis<Policy>()) {}
               List_Scheduler(const List_Scheduler &lS) : hF(lS.getHF()), rC(lS.getRC()), DA(lS.getDA()) {}
               List_Scheduler(const LSHeurFunc &hF_ = LSHeurUnitPrio(), const LSRescFunc &rC_ = LSRescIgnore(), 
                              const BA::DependencyAnalysis<Policy> DA_ = BA::DependencyAnalysis<Policy>()) : hF(hF_), rC(rC_), DA(DA_) {}

               //State getters...
                 LSHeurFunc &getHF() {return hF;}
                 LSRescFunc &getRC() {return rC;}
                 BA::DependencyAnalysis<Policy> &getDA() {return DA;}
          
               //Scheduler operator
                 bool operator()(FlowGraph gI, FlowGraph &gO) {
                    //Initialize
                      ResourceVector resVect;
                      hF.initState(resVect);
                      rC.initState(resVect);
                    //Compute schedule for each basic block...
                      for(pair<FGVIter, FGVIter> vP = vertices(gI); vP.first != vP.second; ++vP.first) {
                          SgAsmBlock *bb = get(boost::vertex_name, gI, *vP.first);
                          if(!DA(bb))
                             return false;
                          BBDepGraph gBB = DA.getDependencyGraph(bb);

                          //Assign priorities to each node...
                            std::map<SgAsmInstruction *, unsigned long int> prioMap;
                            for(std::pair<BBDepVIter, BBDepVIter> vP = vertices(gBB); vP.first != vP.second; ++vP.first) {
                                SgAsmInstruction *ins = get(boost::vertex_name, gBB, *vP.first);
                                prioMap[ins] = hF(ins);
                            }
                          //Configure priority queue
                            typename boost::property_map<BBDepGraph, boost::vertex_name_t>::type vertBDDMap = get(boost::vertex_name, gBB);
                            priority_queue<BBDepVertex, vector<BBDepVertex>, prioQComp> cands(prioQComp(vertBDDMap, prioMap));
                            cands.push(BA::get_root<BBDepGraph>(gBB, true));
                          //Execute list scheduler
                            for(SgAsmStatementPtrList::iterator it = bb->get_statementList().begin(), itF; num_vertices(gBB) > 0; ++it) {
                              BBDepVertex iV = cands.top(); cands.pop();
                              //Schedule instruction of iV
                                for(itF = it; itF != bb->get_statementList().end(); ++itF)
                                    if(*itF == vertBDDMap[iV])
                                       break;
                                if(itF == bb->get_statementList().end())
                                   return false;
                                if(it != itF)
                                   std::iter_swap(it, itF);
                                rC[vertBDDMap[iV]];
                                clear_vertex(iV, gBB);
                                remove_vertex(iV, gBB);
                              //Find more suitable candidates...
                                for(std::pair<BBDepVIter, BBDepVIter> vP = vertices(gBB); vP.first != vP.second; ++vP.first)
                                     if( (in_degree(*vP.first, gBB) == 0) && //Does the instruction satisfy all dependencies?..
                                         rC(vertBDDMap[*vP.first]) )       //... Can it be allowed to execute in the current machine state?
                                        cands.push(*vP.first);
                              //Make sure there is a valid candidate...
                                if(cands.empty()) {
                                   BBDepVertex i = add_vertex(gBB);
                                   SgAsmInstruction *ins = vertBDDMap[i] = rC.getStallInstruction();
                                   prioMap[ins] = hF(ins);
                                   cands.push(ins);
                                }
                                   
                            }
                      }
                    //Return schedule...
                      gO = gI;
                      return true;
               }
       };
};

#endif
