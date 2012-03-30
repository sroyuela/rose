#ifndef REG_ALLOC_LS_HEADER
#define REG_ALLOC_LS_HEADER

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <set>
#include <map>
#include <queue>

#include "binaryAbstract.h"
#include "binaryAnalysis.h"

using namespace std;

/* Linear Scan register allocator --
   (M. Poletto, V. Sarkar, "Linear Scan Register Allocation", */
namespace BT {
     template <class Policy> class RegAllocLS : public RegAllocInterface<Policy> {
          public:
               typedef unsigned long int InsIndex;
          private:
               template <typename Graph> class DFSVis : public boost::default_dfs_visitor {
                    private:
                         typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
                         InsIndex index;
                    public:
                         map<SgAsmInstruction *, InsIndex> insIndexMap;

                         DFSVis() {index = 0;}
                         void finish_vertex(Vertex v, const Graph &g) {
                            SgAsmBlock *bb = get(boost::vertex_name, g, v);
                            //Give each instruction a index
                              for(SgAsmStatementPtrList::iterator it = bb->get_statementList().begin(); it != bb->get_statementList().end(); ++it)
                                   switch( (*it)->variantT() ) {
                                    case V_SgAsmInstruction:
                                    case V_SgAsmArmInstruction:
                                    case V_SgAsmPowerpcInstruction:
                                    case V_SgAsmx86Instruction:
                                        insIndexMap[*it] = index++;
                                        break;
                                    case V_SgAsmStaticData:
                                    case V_SgAsmBlock:
                                    case V_SgAsmFunction:
                                        break;
                                   }
                         }
               };
               struct LessLiveInt : public std::less<BA::RegisterRep> {
                    map<BA::RegisterRep, pair<InsIndex, InsIndex> > &regInterval;
                    bool startPoint;

                    LessLiveInt(bool startPoint_, map<BA::RegisterRep, pair<InsIndex, InsIndex> > &regInterval_) : regInterval(regInterval_) {
                         this->startPoint = startPoint_;
                    }
                    bool operator()(const BA::RegisterRep &x, const BA::RegisterRep &y) const {
                         return (startPoint ? (regInterval[x].first < regInterval[y].first) : 
                                              (regInterval[x].second < regInterval[y].second));
                    }
               };

               BA::LivenessAnalysis<Policy> liveAnal;
          public:
               RegAllocLS() {}
               RegAllocLS(BA::LivenessAnalysis<Policy> lA) : liveAnal(lA) {}
               
               FlowGraph operator()(FlowGraph gI, set<BA::RegisterRep> availableReg) { (*this)(gI, BA::get_root(gI, false)); }          
               FlowGraph operator()(FlowGraph gI, FGVertex entryNode, set<BA::RegisterRep> availableReg) {
                    //Name each instruction...
                      DFSVis<FlowGraph> DFS_Postorder();
                      vector<boost::default_color_type> color(num_vertices(gI)); 
                      boost::depth_first_search(gI, DFS_Postorder, &color[0], vertex(entryNode, gI));
                    //Compute liveness...
                      BA::LivenessAnalysis<Policy> liveAnal;
                      try {
                           if(!liveAnal(gI))
                              cout << "Unable to compute liveness analysis" << endl;
                      } catch(const BA::BAException &e) {
                              cout << "Exception occured: " << e.what() << endl;
                      }
                      cout << liveAnal << endl;
                    //Compute live intervals...
                      map<BA::RegisterRep, pair<InsIndex, InsIndex> > liveIntervals;
                      map<BA::RegisterRep, pair<InsIndex, InsIndex> >::iterator liveIntIt;
                      map<InsIndex, set<BA::RegisterRep> *>::iterator sLiveIntIt;
                      for(std::pair<FGVIter, FGVIter> vP = vertices(gI); vP.first != vP.second; ++vP.first) {
                         //Handle bb
                           SgAsmBlock *bb = get(boost::vertex_name, gI, *vP.first);
                           if(!(bb->get_statementList().size() > 0))
                              continue;
                           InsIndex bbStartIndex = DFS_Postorder.insIndexMap[bb->get_statementList().front()],
                                    bbEndIndex = DFS_Postorder.insIndexMap[bb->get_statementList().back()];
                         //Get live vars for bb...
                           set<BA::RegisterRep> *liveVars = liveAnal.getLiveOutSet(bb);
                         //Update live range for each register...
                           for(set<BA::RegisterRep>::iterator it = liveVars->begin(); it != liveVars->end(); ++it)
                               if((liveIntIt = liveIntervals.find(*it)) == liveIntervals.end())
                                   liveIntervals[*it] = pair<InsIndex, InsIndex>(bbStartIndex, bbEndIndex);
                               else
                                   liveIntervals[*it] = pair<InsIndex, InsIndex>( min(bbStartIndex, liveIntIt->second.first),
                                                                                  max(bbEndIndex, liveIntIt->second.second) );
                      }
                    //Perform LSRA
                      set<BA::RegisterRep, LessLiveInt> sortedLiveIntervals(LessLiveInt(true, liveIntervals));
                      for(liveIntIt = liveIntervals.begin(); liveIntIt != liveIntervals.end(); ++liveIntIt)
                          sortedLiveIntervals.insert( liveIntIt->first );

                      set<BA::RegisterRep> availReg(availableReg.begin(), availableReg.end()), spilledRegisters;
                      map<BA::RegisterRep, BA::RegisterRep> registerMap;
                      unsigned int R = availableReg.size();
                      for(set<BA::RegisterRep, LessLiveInt> activeList(LessLiveInt(false, liveIntervals)); sortedLiveIntervals.size() > 0; ) {
                         BA::RegisterRep i = *sortedLiveIntervals.begin();   sortedLiveIntervals.erase(sortedLiveIntervals.begin());
                         //Expire element if neccessary...
                           for(typename set<BA::RegisterRep, LessLiveInt>::iterator it = activeList.begin(); it != activeList.end(); ++it) {
                               BA::RegisterRep j = *it; ++it;
                               if(liveIntervals[j].second > liveIntervals[i].first)
                                  break;
                               activeList.erase(activeList.find(j)); //remove j from active
                               availReg.insert(registerMap[j]); //add register[j] to pool of free registers...
                           }
                         //Should register be spilled?
                           if(activeList.size() == R) {
                              BA::RegisterRep spill = *activeList.rbegin();
                              if(liveIntervals[spill].second > liveIntervals[i].second) {
                                   spilledRegisters.insert(spill); //mark "spill" as spilled...
                                   registerMap[i] = registerMap[spill];
                                   activeList.erase(activeList.find(spill)); //remove "spill" from active...
                                   activeList.insert(i); //insert i
                              } else
                                   spilledRegisters.insert(i); //spill i
                           } else {
                              //Don't reassign reg if its reg is already available
                                if(availReg.find(i) == availReg.end())
                                   registerMap[i] = *availReg.begin();
                                else
                                   registerMap[i] = i;
                                availReg.erase(availReg.find(registerMap[i])); //assign available reg.
                              //Make i active
                                activeList.insert(i); //insert i
                           }
                      }
                    //Debug message?
                      if(SgProject::get_verbose() > 0) {
                         cout << "LSRA" << endl << "-----" << endl;
                         stringstream ssSpilled, ssReg;
                         for(map<BA::RegisterRep, BA::RegisterRep>::iterator it = registerMap.begin(); it != registerMap.end(); ++it)
                             if(spilledRegisters.find(it->first) == spilledRegisters.end())
                                   ssReg << "(" << it->first << ", " << it->second << "), ";
                             else
                                   ssSpilled << it->first << ", ";
                         cout << "Register mapped: {" << ssReg.str() << "}" << endl << "Spilled: {" << ssSpilled.str() << "}" << endl << "-----" << endl;
                      }
               }
     };
};

#endif
