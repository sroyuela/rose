#ifndef UGRAPHFUNC_HDR
#define UGRAPHFUNC_HDR

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/transitive_closure.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/bimap.hpp>

#include <set>

using namespace std;

namespace BA {

            template<class G> typename boost::graph_traits<G>::vertex_descriptor get_exit(const G &g, bool pickFirstOnMulti) {
               typedef typename boost::graph_traits<G>::vertex_descriptor GVertex;
               typedef typename boost::graph_traits<G>::vertex_iterator VIter;

               GVertex vExit = boost::graph_traits<G>::null_vertex();
               for(std::pair<VIter, VIter> vP = vertices(g); vP.first != vP.second; ++vP.first)
                   if(out_degree(*vP.first, g) == 0) {
                      if(!pickFirstOnMulti) {
                         ROSE_ASSERT(vExit == boost::graph_traits<G>::null_vertex());
                         vExit = *vP.first;
                      } else
                              return *vP.first;
                   }
                   return vExit;               
            }
            template<class G> typename boost::graph_traits<G>::vertex_descriptor get_root(const G &g, bool pickFirstOnMulti) {
               typedef typename boost::graph_traits<G>::vertex_descriptor GVertex;
               typedef typename boost::graph_traits<G>::vertex_iterator VIter;

               GVertex vEntry = boost::graph_traits<G>::null_vertex();
               for(std::pair<VIter, VIter> vP = vertices(g); vP.first != vP.second; ++vP.first)
                   if(in_degree(*vP.first, g) == 0) {
                      if(!pickFirstOnMulti) {
                         ROSE_ASSERT(vEntry == boost::graph_traits<G>::null_vertex());
                         vEntry = *vP.first;
                      } else
                              return *vP.first;
                   }
               return vEntry;
            }
            //Collect successors into succSet
            template<class G> unsigned int succ(const G &g, 
                                                const typename boost::graph_traits<G>::vertex_descriptor v, 
                                                set<typename boost::graph_traits<G>::vertex_descriptor> &succSet) {
               typedef typename boost::graph_traits<G>::out_edge_iterator GOEIter;

               succSet.clear();
               if(v == boost::graph_traits<G>::null_vertex())
                    return 0;
               for(pair<GOEIter, GOEIter> vP = boost::out_edges(v, g); vP.first != vP.second; ++vP.first)
                   succSet.insert( target(*vP.first,g) );
               return succSet.size();
            }
            //Collect predecessors into predSet
            template<class G> unsigned int pred(const G &g, 
                                                const typename boost::graph_traits<G>::vertex_descriptor v, 
                                                set<typename boost::graph_traits<G>::vertex_descriptor> &predSet) {
               typedef typename boost::graph_traits<G>::in_edge_iterator GIEIter;

               predSet.clear();
               if(v == boost::graph_traits<G>::null_vertex())
                    return 0;
               for(pair<GIEIter, GIEIter> vP = boost::in_edges(v, g); vP.first != vP.second; ++vP.first)
                   predSet.insert( source(*vP.first,g) );
               return predSet.size();
            }
            //Randomly return a vertex from (vSet-ignore)...
            template<class G> const typename boost::graph_traits<G>::vertex_descriptor pick(
               set<typename boost::graph_traits<G>::vertex_descriptor> &vSet, 
               set<typename boost::graph_traits<G>::vertex_descriptor> &ignore) {
               typedef typename boost::graph_traits<G>::vertex_descriptor GVertex;

               vector<GVertex> result;
               for(typename set<GVertex>::iterator it = vSet.begin(); it != vSet.end(); ++it)
                    if(ignore.find(*it) == ignore.end())
                         result.push_back(*it);

               if(result.size() == 0)
                    return boost::graph_traits<G>::null_vertex();
               else {
                    std::random_shuffle(result.begin(), result.end());
                    return *(result.begin());
               }
            }
            //Randomly return a vertex from vSet-{v}
            template<class G> const typename boost::graph_traits<G>::vertex_descriptor pick(
               set<typename boost::graph_traits<G>::vertex_descriptor> &vSet, 
               typename boost::graph_traits<G>::vertex_descriptor v = boost::graph_traits<G>::null_vertex()) {

               set<typename boost::graph_traits<G>::vertex_descriptor> ignore;
               if(v != boost::graph_traits<G>::null_vertex())
                    ignore.insert(v);
               return pick<G>(vSet, ignore);
            }

            template<class G> G create_subg(G g, 
               set<typename boost::graph_traits<G>::vertex_descriptor> include, 
               boost::bimap<typename boost::graph_traits<G>::vertex_descriptor, typename boost::graph_traits<G>::vertex_descriptor> &subMap,
               bool incNodesLinkedFromInclude = true,
               bool outputReverseGraph = false) {
               typedef typename boost::graph_traits<G>::vertex_descriptor GVertex;
               typedef typename boost::graph_traits<G>::edge_iterator GEIter;
               //Translate nodes includes in "include" to current graph...
                 G gSubset;
                 subMap.clear();
                 subMap.insert( typename boost::bimap<GVertex, GVertex>::value_type(
                                   boost::graph_traits<G>::null_vertex(),
                                   boost::graph_traits<G>::null_vertex()) );
                 for(typename set<GVertex>::iterator it = include.begin(); it != include.end(); ++it) {
                     GVertex vN = add_vertex(gSubset);
                     subMap.insert( typename boost::bimap<GVertex, GVertex>::value_type(*it,vN) );
                 }
               //Should neighbors to "include" nodes be added?
                 typename boost::bimap<GVertex, GVertex>::left_map::iterator itS, itT;
                 if(incNodesLinkedFromInclude) {
                    //Calculate transitive closure of CFG...
                      G gTC;
                      boost::transitive_closure(g, gTC);
                    //Add all nodes reachable from any in include
                      for(pair<GEIter, GEIter> eP = edges(gTC); eP.first != eP.second; ++eP.first)
                          if((itS = subMap.left.find(source(*eP.first, gTC))) != subMap.left.end()) {
                              if(subMap.left.find(target(*eP.first, gTC)) != subMap.left.end())
                                 continue;
                              //Add node to new g
                                GVertex vN = add_vertex(gSubset), vO = target(*eP.first, gTC);
                                subMap.insert( typename boost::bimap<GVertex, GVertex>::value_type(vO,vN) );
                          }
                 }
               //Add all edges <u,v> where u,v \in subMap
                 for(pair<GEIter, GEIter> eP = edges(g); eP.first != eP.second; ++eP.first)
                     if( ((itS = subMap.left.find( source(*eP.first, g) )) != subMap.left.end()) &&
                         ((itT = subMap.left.find( target(*eP.first, g) )) != subMap.left.end()) )
                              if(outputReverseGraph)
                                add_edge(itT->second, itS->second, gSubset);
                              else
                                add_edge(itS->second, itT->second, gSubset);
               //Return results 
                 return gSubset;
            }

            template<class G> G create_subg(G g, typename boost::graph_traits<G>::vertex_descriptor include, 
               boost::bimap<typename boost::graph_traits<G>::vertex_descriptor, typename boost::graph_traits<G>::vertex_descriptor> &subMap,
               bool incNodesLinkedFromInclude = true,
               bool outputReverseGraph = false) {
                    typedef typename boost::graph_traits<G>::vertex_descriptor GVertex;

                    set<GVertex> tmp;
                    tmp.insert(include);
                    return create_subg(g, tmp, subMap, incNodesLinkedFromInclude, outputReverseGraph);
            }

};

#endif
