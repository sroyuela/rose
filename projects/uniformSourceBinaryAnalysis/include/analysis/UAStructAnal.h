#ifndef UASTRUCTANAL_HDR
#define UASTRUCTANAL_HDR

/*Sources used in implementing this algorithm:
     * S. S. Muchnick, "Advanced Compiler Design & Implementation", 1997, Morgan Kaufman
     * M. Sharir, "Structural analysis: A new approach to flow analysis in optimizing compilers", Computer Languages, Vol. 5, p. 141-153
     * J. Stainer's Structural Analysis implementation for the LLVM compiler as was used for the paper:
       (J. Stainer, D. Watson, "A study on irreducibility in C programs", Software - practice and experience, Vol. 42, p. 117-130, 2012)
*/

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/copy.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/unordered_map.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/exception.hpp>
#include <boost/graph/graphviz.hpp>

#include <boost/version.hpp>

#include <set>
#include <list>
#include <map>
#include <vector>
#include <queue>

#include "rose.h"
#include "BinaryControlFlow.h"
#include "BinaryLoader.h"

#include "binaryAnalysis.h"

using namespace std;

/* Muchnick's Structure analysis algorithm for a graph with vertex type V*/
namespace BA {
     /*Requirements on G:
          - Has to be a boost::adjacency_list
          -VertexList = boost::vecS 
          - Directed = boost::bidirectionalS*/     
     template<class G> struct StructAnal {
       public:
          //G descriptions...
            typedef typename boost::graph_traits<G>::vertex_iterator GVIter;
            typedef typename boost::graph_traits<G>::edge_iterator GEIter;
            typedef typename boost::graph_traits<G >::vertex_descriptor GVertex;
            typedef typename boost::graph_traits<G >::edge_descriptor GEdge;
            typedef typename boost::graph_traits<G>::out_edge_iterator GOEIter;
            typedef typename boost::graph_traits<G>::in_edge_iterator GIEIter;
          //Exception class
            struct StructAnalException : public std::exception {
                    string s;
                    StructAnalException(string ss) : s(ss) {}
                    virtual ~StructAnalException() throw() {}
                    const char *what() const throw() {return s.c_str();}
            };
          //StructureTree graph...
            /* See [Muchnick, "Advanced compiler design and implementation", p. 203] for a definition of the following types */
            typedef enum {
                    NONE = 0,
                    ACYCLIC_BLOCK, ACYCLIC_IF_THEN, ACYCLIC_IF_THEN_ELSE, ACYCLIC_CASE_SWITCH, ACYCLIC_PROPER,
                    CYCLIC_SELF_LOOP, CYCLIC_WHILE, CYCLIC_NATURAL_LOOP, CYCLIC_IMPROPER_INTERVAL
            } STRUCTURE_TYPE;
            struct StructNode {
               bool isStructure; //= True: "SType" describes the structure type of the nodes, 
                                 //= False: "SNodeID" is the vertex node ID of the structure
               STRUCTURE_TYPE SType;
               typename boost::graph_traits<G>::vertices_size_type SNodeID;

               StructNode() {}
               StructNode(typename boost::graph_traits<G>::vertices_size_type SNId) : isStructure(false), SType(NONE), SNodeID(SNId) {}
               StructNode(STRUCTURE_TYPE ST) : isStructure(true), SType(ST), SNodeID(boost::graph_traits<G>::null_vertex()) {}

               friend istream &operator>>(istream &i, StructNode &s) {
                    char c;
                    i >> c;
                    i.seekg(-1,ios::cur);
                    if((c <= '9') && (c >= '0')) {
                         s.isStructure = false;
                         i >> s.SNodeID;
                    } else {
                         s.isStructure = true;
                         string str;
                         i >> str;
                         if(str == "None")                       s.SType = NONE;
                         else if(str == "Block")                 s.SType = ACYCLIC_BLOCK;
                         else if(str == "If-Then")               s.SType = ACYCLIC_BLOCK;
                         else if(str == "If-Then-Else")          s.SType = ACYCLIC_BLOCK;
                         else if(str == "Case")                  s.SType = ACYCLIC_BLOCK;
                         else if(str == "Proper interval")       s.SType = ACYCLIC_BLOCK;
                         else if(str == "Self-loop")             s.SType = ACYCLIC_BLOCK;
                         else if(str == "While")                 s.SType = ACYCLIC_BLOCK;
                         else if(str == "Natural loop")          s.SType = ACYCLIC_BLOCK;
                         else if(str == "Improper interval")     s.SType = ACYCLIC_BLOCK; 
                         else                                    ROSE_ASSERT(false);    
                    }
                    return i;
               }
               friend ostream &operator<<(ostream &o, StructNode &s) {
                    if(s.isStructure)
                         switch(s.SType) {
                          case NONE:                        o << "None";               break;
                          case ACYCLIC_BLOCK:               o << "Block";              break; 
                          case ACYCLIC_IF_THEN:             o << "If-Then";            break;
                          case ACYCLIC_IF_THEN_ELSE:        o << "If-Then-Else";       break; 
                          case ACYCLIC_CASE_SWITCH:         o << "Case";               break;
                          case ACYCLIC_PROPER:              o << "Proper interval";    break;
                          case CYCLIC_SELF_LOOP:            o << "Self-loop";          break;
                          case CYCLIC_WHILE:                o << "While";              break;
                          case CYCLIC_NATURAL_LOOP:         o << "Natural loop";       break;
                          case CYCLIC_IMPROPER_INTERVAL:    o << "Improper interval";  break;
                         }
                    else
                         o << s.SNodeID;
                    return o;
               }
            };
          //StructureTree descriptions...
            typedef typename boost::property<boost::vertex_name_t, StructNode> STreeVName;
            typedef typename boost::property<boost::edge_name_t, unsigned int> STreeEName;
            typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, STreeVName, STreeEName> StructureTree;
            typedef typename boost::graph_traits<StructureTree>::vertex_descriptor STreeVertex; 
            typedef typename boost::graph_traits<StructureTree>::edge_descriptor STreeEdge;
            typedef typename boost::graph_traits<StructureTree>::vertex_iterator STVIter;
            typedef typename boost::graph_traits<StructureTree>::edge_iterator STEIter;
            typedef typename boost::graph_traits<StructureTree>::out_edge_iterator STOEIter;
            typedef typename boost::graph_traits<StructureTree>::in_edge_iterator STIEIter;

          StructAnal() {}
          virtual ~StructAnal() {}
          //Compute structure tree given a graph
          StructureTree operator()(G &g) {
               return (*this)(g, get_root(g, false));
          }
          StructureTree operator()(const G &g, const GVertex &entryNode) {
               //Initialization
                 set<GVertex> ReachUnder;
                 unsigned int PostCtr = 0, foundStructures[2] = {1, 0};
                 typename boost::graph_traits<G>::vertices_size_type eN = get(boost::vertex_index, g, entryNode);
                 STreeVertex q;
                 G gN;
                 translationVector.clear();
               //Posttraversal of nodes by DFS
                 Post = vector<GVertex>(num_vertices(g));
                 DFSVis<G> DFS_Postorder(Post);
                 vector<boost::default_color_type> color(num_vertices(g)); 
                 boost::depth_first_search(g, DFS_Postorder, &color[0], vertex(entryNode, g));
               //Computation
                 s = StructureTree(num_vertices(g));

                 typename boost::property_map<StructureTree, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, s);
                 translationVector = vector<typename boost::graph_traits<StructureTree>::vertices_size_type>( num_vertices(g) );
                 for(std::pair<GVIter, GVIter> vP = vertices(g); vP.first != vP.second; ++vP.first) {
                    vertMap[*vP.first] = StructNode( get(boost::vertex_index, g, *vP.first) );
                    translationVector[get(boost::vertex_index, g, *vP.first)] = get(boost::vertex_index, g, *vP.first);
                 }
                 for(copy_graph(g, gN); num_vertices(gN) > 1; PostCtr = 0, foundStructures[0] = foundStructures[1], foundStructures[1] = 0) {
                         //Print work graph?
                           if(SgProject::get_verbose() > 0) {
                              cout << "-----" << endl << "Graph " << "<V = {";
                              for(std::pair<GVIter, GVIter> vP = vertices(gN); vP.first != vP.second; ++vP.first)
                                  cout << *vP.first << ", ";
                              cout << "}, E = {";
                              for(pair<GEIter, GEIter> eP = edges(gN); eP.first != eP.second; ++eP.first)
                                  cout << "<" << source(*eP.first, gN) << ", " << target(*eP.first, gN) << ">, ";
                              cout << "}" << endl << "Post-order <";
                              for(typename vector<GVertex>::iterator it = Post.begin(); it != Post.end(); ++it)
                                  cout << *it << ", ";
                              cout << ">" << endl << "-----" << endl;
                           }        
                         //Check for structural patterns...
                           while((num_vertices(gN) > 1) && (PostCtr <= Post.size()-1)) {
                             //(Debug message?)
                               if(SgProject::get_verbose() > 0)
                                  cout << PostCtr << ") Trying " << Post[PostCtr] << "[" 
                                       << get(boost::vertex_name, s, translationVector[Post[PostCtr]]) << "] / " << num_vertices(gN) << endl;
                             //... acyclic
                               if( (q = Acyclic_Region_Type(gN, Post[PostCtr], foundStructures[0]+foundStructures[1])) != boost::graph_traits<StructureTree>::null_vertex()) {
                                   eN = reduce(gN, q, Post, eN);
                                   PostCtr = getPostOrderIndex(vertex(eN, g), Post);
                                   foundStructures[1]++;
                             //... cyclic
                               } else {
                                   ReachUnder = set<GVertex>();
                                   ReachUnder.insert(Post[PostCtr]);
                                   PathBack p = PathBack(Post);
                                   set<GVertex> nodeSet = set<GVertex>(vertices(g).first, vertices(g).second);

                                   for(std::pair<GVIter, GVIter> vP = vertices(gN); vP.first != vP.second; ++vP.first)
                                        if( PathFunc(gN, *vP.first, Post[PostCtr], nodeSet, p) )
                                             ReachUnder.insert(*vP.first);                                   
                                   if( (q = Cyclic_Region_Type(gN, Post[PostCtr], ReachUnder)) != boost::graph_traits<StructureTree>::null_vertex()) {
                                        eN = reduce(gN, q, Post, eN);
                                        PostCtr = getPostOrderIndex(vertex(eN, g), Post);
                                        foundStructures[1]++;
                                   } else
                                        PostCtr++;
                               }
                           }           
                         //Make sure algorithm is able to reduce the graph...
                           ROSE_ASSERT((foundStructures[0] != 0) || (foundStructures[1] != 0));
                 }
               //Return results...                                
                 return s;
          }

          string printSTree(StructureTree &s, string fileName = "") {
               //Output to file?
                 if(fileName != "") {
                    ofstream f(fileName.c_str());
                    boost::dynamic_properties p;
                    p.property("label", get(boost::edge_name, s));
                    p.property("node_id", get(boost::vertex_index, s));
                    p.property("label", boost::get(boost::vertex_name, s));
                    #if( ((BOOST_VERSION / 100000) > 1) || \
                         ( ((BOOST_VERSION / 100000) == 1) && (((BOOST_VERSION / 100) % 1000) >= 44) ) )
                              boost::write_graphviz_dp(f, s, p);
                    #else
                              boost::write_graphviz(f, s, p);
                    #endif
                    f.close();
                 }
               //Print the structure to terminal
                 stringstream ss;
                 typename boost::property_map<StructureTree, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, s);
                 typename boost::property_map<StructureTree, boost::edge_name_t>::type edgeMap = get(boost::edge_name, s);
                 for(std::pair<STVIter, STVIter> vP = vertices(s); vP.first != vP.second; ++vP.first) {
                    StructNode SN = vertMap[*vP.first];
                    ss << get(boost::vertex_index, s, *vP.first) << "-" << SN << " (";
                    for(std::pair<STOEIter, STOEIter> eP = out_edges(*vP.first, s); eP.first != eP.second; ++eP.first)
                         ss << edgeMap[*eP.first] << "=" << target(*eP.first, s) << ", ";
                    ss << ")" << endl;
                 }
                 return ss.str();
          }
       private:
          //State instance...
            StructureTree s;
            vector<GVertex> Post;
            vector<typename boost::graph_traits<StructureTree>::vertices_size_type> translationVector;
          //Helper structures
            template <typename Graph> class DFSVis : public boost::default_dfs_visitor {
               private:
                    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
                    mutable int PostMax;
                    vector<GVertex> &Post;
               public:
                    DFSVis(vector<GVertex> &P) : Post(P) {PostMax = 0;}
                    void finish_vertex(Vertex v, const Graph &g) const {Post[PostMax++] = v;}
            };

            template <class Name> class dotVLblWriter {
               private:
                    Name name;
               public:
                    dotVLblWriter(Name _name) : name(_name) {}
                    template<class Vertex> void operator()(std::ostream& out, const Vertex& v) const {
                      out << "[style=\"filled\" penwidth=1 fillcolor=\"white\" fontname=\"Courier New\" label=\"" << name[v] << "\"]";
                    }
            };
          //Helper functions
            STreeVertex addStructNodes(const G &g, STRUCTURE_TYPE ST, vector<GVertex> nodes, GVertex nX = boost::graph_traits<G>::null_vertex(), 
                                       GVertex nY = boost::graph_traits<G>::null_vertex(), GVertex nZ = boost::graph_traits<G>::null_vertex()) {
               typename boost::property_map<StructureTree, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, s);
               typename boost::property_map<StructureTree, boost::edge_name_t>::type edgeMap = get(boost::edge_name, s);
               typename boost::property_map<G, boost::vertex_index_t>::type vertIndexMap = get(boost::vertex_index, g);
               //Add node...
                 STreeVertex vS = add_vertex(s);
                 vertMap[vS] = StructNode(ST);

                 if(nX != boost::graph_traits<G>::null_vertex())
                    edgeMap[add_edge(vS, vertex(translationVector[vertIndexMap[nX]],s), s).first] = 0;
                 if(nY != boost::graph_traits<G>::null_vertex())
                    edgeMap[add_edge(vS, vertex(translationVector[vertIndexMap[nY]],s), s).first] = 1;
                 if(nZ != boost::graph_traits<G>::null_vertex())
                    edgeMap[add_edge(vS, vertex(translationVector[vertIndexMap[nZ]],s), s).first] = 2;

                 unsigned int i = 3;
                 for(typename vector<GVertex>::iterator it = nodes.begin(); it != nodes.end(); ++it, i++) {
                     ROSE_ASSERT(!edge(vS, vertex(translationVector[vertIndexMap[*it]],s), s).second);
                     edgeMap[add_edge(vS, vertex(translationVector[vertIndexMap[*it]],s), s).first] = i;
                 }
               return vS;
            }
            static typename boost::graph_traits<G>::vertices_size_type getPostOrderIndex(const GVertex x, const vector<GVertex> &Post) {
               typename boost::graph_traits<G>::vertices_size_type i = 0;
               for(typename vector<GVertex>::const_iterator it = Post.begin(); it != Post.end(); ++it, i++)
                   if(*it == x)
                      return i;
               return boost::graph_traits<G>::null_vertex();
            }

            //Functor for implementing Path/PathBack function in PathFunc
              struct PredicateFunctor {
                    PredicateFunctor() {}
                    virtual ~PredicateFunctor() {}

                    virtual bool addCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const = 0;
                    virtual bool succCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const = 0;
              };
              struct Path : public PredicateFunctor {
                    Path() {}
                    virtual ~Path() {}
                    bool succCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const {return (v == vExit);}
                    bool addCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const {return true;}
              };
              struct PathBack : public PredicateFunctor {
                 vector<GVertex> &Post;

                 PathBack(vector<GVertex> &Post_) : Post(Post_) {}
                 virtual ~PathBack() {}
                 bool succCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const {
                    typename boost::graph_traits<G>::vertices_size_type iVX = getPostOrderIndex(v, Post), iVY = getPostOrderIndex(vExit, Post);
                    ROSE_ASSERT( (iVX != boost::graph_traits<G>::null_vertex()) && (iVY != boost::graph_traits<G>::null_vertex()) );
                    return (edge(v, vExit, g).second && (iVX < iVY));
                 }
                 bool addCond(const G &g, const GVertex vStart, const GVertex v, const GVertex vExit) const {
                    return (v != vExit);
                 }
              };

            bool PathFunc(G &g, GVertex entry, GVertex exit, set<GVertex> nodeSet, PredicateFunctor &f) {
               stack<GVertex> sV;
               set<GVertex> visitedSet;
               for(visitedSet.insert(entry), sV.push(entry); !sV.empty(); ) {
                    GVertex v = sV.top(); sV.pop();
                    if(f.succCond(g, entry, v, exit))
                         return true;
                    for(pair<GOEIter, GOEIter> oP = boost::out_edges(v, g); oP.first != oP.second; ++oP.first) {
                        GVertex node = target(*oP.first, g);
                        if( (nodeSet.find(node) != nodeSet.end()) && (visitedSet.find(node) == visitedSet.end()) && f.addCond(g, entry, node, exit) ) {
                              sV.push(node);
                              visitedSet.insert(node);
                        }
                    }
               }
               return false;
            }

          //Detect acyclic patterns...
            STreeVertex Acyclic_Region_Type(G &g, GVertex v, int nrFoundStructures) {
               GVertex m, n;
               set<GVertex> succSet, predSet;
               vector<GVertex> Nset;
               bool p, s;
               //Initialization...
                 Nset.clear();
               //Check for block with node in it.
                 //Get all successors as long as they have in and out degree = 0
                 for(n = v, s = (out_degree(v, g) == 1), p = true;
                     p && s;
                     n = target(*out_edges(n,g).first, g), s = (out_degree(n, g) == 1), p = (in_degree(n, g) == 1))
                         Nset.push_back(n);
                 if(p)
                    Nset.push_back(n);
                 //Get all predecessors as long as they have in and out degree = 0
                 if(Nset.size() == 0)             n = v;
                 else if(in_degree(v, g) == 1)    n = source(*in_edges(v,g).first, g);
                 else                             n = boost::graph_traits<G>::null_vertex();
                 if(n != boost::graph_traits<G>::null_vertex()) {
                      for(p = (in_degree(n, g) == 1), s = (out_degree(n, g) == 1);
                          p && s;
                          n = source(*in_edges(n,g).first, g), s = (out_degree(n, g) == 1), p = (in_degree(n, g) == 1))
                              Nset.insert(Nset.begin(), n);
                      if(s)
                         Nset.insert(Nset.begin(), n);
                 }

                 succ(g,v,succSet);
                 pred(g,v,predSet);
                 if(Nset.size() >= 2)
                    return addStructNodes(g, ACYCLIC_BLOCK, Nset);
               //Check for conditional structures
                 else if(succSet.size() == 2) {
                    set<GVertex> succSetM, predSetM, succSetN, predSetN;
                    //Pick successor blocks
                      m = pick<G>(succSet);  n = pick<G>(succSet,m);
                    //Compute predecessors and successors
                      pred(g,m,predSetM);      pred(g,n,predSetN);
                      succ(g,m,succSetM);      succ(g,n,succSetN);               
                    //If-then-else
                    if( (succSetM == succSetN) && (succSetM.size() <= 1) &&
                        (predSetN.size() == 1) && (predSetM.size() == 1) )
                         return addStructNodes(g, ACYCLIC_IF_THEN_ELSE, vector<GVertex>(), v, m, n);
                    //If-then (m is exit)
                    else if( (succSetN.size() == 1)     && (predSetN.size() == 1) &&
                             (*(succSetN.begin()) == m) )
                              return addStructNodes(g, ACYCLIC_IF_THEN, vector<GVertex>(), v, n);
                    //If-then (n is exit)
                    else if( (succSetM.size() == 1)   && (predSetM.size() == 1) &&
                             (*(succSetM.begin()) == n) )
                              return addStructNodes(g, ACYCLIC_IF_THEN, vector<GVertex>(), v, m);
                 } else if (succSet.size() > 2) {
                    //Check for switch/case structure...
                      //Check that all nodes have entry node as only pred. and same succ. node (exitNode)
                        bool validCase = true;
                        GVertex exitNode = boost::graph_traits<G>::null_vertex();
                        for(typename set<GVertex>::iterator it = succSet.begin(); validCase && (it != succSet.end()); ++it) {
                            //... Succ...
                              switch(in_degree(*it, g)) {
                               case 0:  validCase = (exitNode == boost::graph_traits<G>::null_vertex());  break;
                               case 1:  validCase = (exitNode == target(*out_edges(*it,g).first, g));     break;
                               default: validCase = false;                                                break;
                              }
                            //... Pred
                              if( !((in_degree(*it, g) == 1) && (source(*in_edges(n,g).first, g) == v)) )
                                  validCase = false;
                        }
                        if(validCase) {
                              if(exitNode != boost::graph_traits<G>::null_vertex())
                                   return addStructNodes(g, ACYCLIC_CASE_SWITCH, vector<GVertex>(succSet.begin(), succSet.end()), v, exitNode);
                              else
                                   return addStructNodes(g, ACYCLIC_CASE_SWITCH, vector<GVertex>(succSet.begin(), succSet.end()), v);
                        }
                 }
               //Check for proper acyclic regions?
                 if(nrFoundStructures > 0)
                    return boost::graph_traits<StructureTree>::null_vertex();
               //Compute dominance...
                 typedef map<GVertex, std::size_t> IndexMap;
                 boost::bimap<GVertex, GVertex> subMap;
                 G subG = create_subg(g, v, subMap);
                 IndexMap mapIndex;
                 boost::associative_property_map<IndexMap> indexMap(mapIndex);
                 int i = 0;
                 GVertex vEntry = subMap.left.at(v), vExit = boost::graph_traits<G>::null_vertex();
                 for(std::pair<GVIter, GVIter> vP = vertices(subG); vP.first != vP.second; ++vP.first)
                     //Initialize index map...
                       put(indexMap, *vP.first, i++);

                 vector<GVertex> domTreePredVector = vector<GVertex>(num_vertices(subG), boost::graph_traits<G>::null_vertex());
                 boost::iterator_property_map<typename vector<GVertex>::iterator, boost::associative_property_map<IndexMap> > domTreePredMap = 
                        boost::make_iterator_property_map(domTreePredVector.begin(), indexMap);
                 boost::lengauer_tarjan_dominator_tree(subG, vEntry, domTreePredMap);

                 for(std::pair<GVIter, GVIter> vP = vertices(subG); vP.first != vP.second; ++vP.first)
                     if( (get(domTreePredMap, *vP.first) == vEntry) &&
                         ( (vExit == boost::graph_traits<G>::null_vertex()) || 
                           (getPostOrderIndex(subMap.right.at(*vP.first), Post) > getPostOrderIndex(subMap.right.at(vExit), Post)) ) &&
                         (out_degree(*vP.first, subG) <= 1) )
                              vExit = *vP.first;
               //Validate proposed region...
                 bool isProperRegion = true;
                 //Make sure there's a exit...
                   if((vExit == boost::graph_traits<G>::null_vertex()) || (vEntry == vExit))
                      isProperRegion = false;
                 //Make sure all predecessors of each node (except entry) is in the region
                   for(std::pair<GVIter, GVIter> vP = vertices(subG); isProperRegion && (vP.first != vP.second); ++vP.first) {
                       if(*vP.first == vEntry)
                          continue;
                       for(pair<GIEIter, GIEIter> iP = boost::in_edges(subMap.right.at(*vP.first), g); isProperRegion && (iP.first != iP.second); ++iP.first)
                           if(subMap.right.find(source(*iP.first,g)) == subMap.right.end() )
                              isProperRegion = false;
                   }
                 if(!isProperRegion)
                    return boost::graph_traits<StructureTree>::null_vertex();
               //Create region...
                 //Project "subG" nodes back into current graph...
                   vector<GVertex> vSet;
                   for(std::pair<GVIter, GVIter> vP = vertices(subG); vP.first != vP.second; ++vP.first)
                       if( (*vP.first != vEntry) && (*vP.first != vExit) )
                             vSet.push_back(subMap.right.at(*vP.first));

                 return addStructNodes(g, ACYCLIC_PROPER, vSet, subMap.right.at(vEntry), subMap.right.at(vExit));
            }

          //Detect cyclic patterns...
            STreeVertex Cyclic_Region_Type(G &g, GVertex v, set<GVertex> &Nset) {
               set<GVertex> succSet, succSetM, predSet, predSetM;
               typename boost::property_map<StructureTree, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, s);
               typename boost::property_map<StructureTree, boost::edge_name_t>::type edgeMap = get(boost::edge_name, s);
               //Check for self-loop
                 if(Nset.size() == 1) {
                    succ(g, v, succSet);
                    if(succSet.find(v) != succSet.end())
                         return addStructNodes(g, CYCLIC_SELF_LOOP, vector<GVertex>(), v);
                    else
                         return boost::graph_traits<StructureTree>::null_vertex();
                 }
               //Check for improper regions...
                 Path p = Path();
                 set<GVertex> nodeSet = set<GVertex>(vertices(g).first, vertices(g).second);
                 for(typename set<GVertex>::iterator it = Nset.begin(); it != Nset.end(); ++it)
                    if(!PathFunc(g, v, (GVertex &) *it, nodeSet, p)) {
                         GVertex entryNode = Minimize_Improper(g, v, Nset);
                         if(entryNode != boost::graph_traits<StructureTree>::null_vertex())
                              return addStructNodes(g, CYCLIC_IMPROPER_INTERVAL, vector<GVertex>(Nset.begin(), Nset.end()), entryNode);
                         else
                              return boost::graph_traits<StructureTree>::null_vertex();
                    }
               //Check for {While, Natural}Loop
                 Nset.erase(Nset.find(v));
                 GVertex m = pick<G>(Nset, v);
                 pred(g,m,predSetM);
                 pred(g,v,predSet);
                 succ(g,m,succSetM);
                 succ(g,v,succSet);
                 if( (succSet.size() == 2) && (succSetM.size() == 1) &&
                     (predSet.size() == 2) && (predSetM.size() == 1) )
                    return addStructNodes(g, CYCLIC_WHILE, vector<GVertex>(Nset.begin(), Nset.end()), v);
                 else
                    return addStructNodes(g, CYCLIC_NATURAL_LOOP, vector<GVertex>(Nset.begin(), Nset.end()), v);
            }

          //Graph reduction
            struct PQReduceComp : public std::binary_function<GVertex, GVertex, bool> {
               G &g;
               PQReduceComp(G &g_) : g(g_) {}
               bool operator()(const GVertex &v1, const GVertex &v2) const {
                    return (get(boost::vertex_index, g, v1) < get(boost::vertex_index, g, v2) );
               }
            };
            typename boost::graph_traits<G>::vertices_size_type reduce(G &g, STreeVertex node, vector<GVertex> &Post, typename boost::graph_traits<G>::vertices_size_type eN) {
                 //Error check
                   if(node == boost::graph_traits<StructureTree>::null_vertex())
                     return eN;
                 //Replace subgraph with a new nodes...
                   GVertex superNode = add_vertex(g);

                   bool isEntryInCluster = false;
                   typename boost::graph_traits<G>::vertices_size_type eNStepsBack = 0;
                   priority_queue<GVertex, vector<GVertex>, PQReduceComp> pqV = priority_queue<GVertex, vector<GVertex>, PQReduceComp>(PQReduceComp(g));
                   typename boost::property_map<StructureTree, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, s);
                   //Initialization
                     //Debug message?
                       if(SgProject::get_verbose() > 0)
                          cout << "Found " << get(boost::vertex_index, s, node) << "-" << get(boost::vertex_name, s, node) << " (";
                     for(pair<STOEIter, STOEIter> e = boost::out_edges(node, s); e.first != e.second; ++e.first) {
                         typename boost::graph_traits<G>::vertices_size_type id = boost::graph_traits<G>::null_vertex(), idCnt = 0;
                         //Find G index...
                           for(typename vector<typename boost::graph_traits<StructureTree>::vertices_size_type>::iterator it = translationVector.begin();
                               it != translationVector.end();
                               ++it, ++idCnt)
                                if(*it == target(*e.first, s)) {
                                   id = idCnt;
                                   break;
                                }
                         ROSE_ASSERT(id != boost::graph_traits<G>::null_vertex());
                         //Debug message?
                           if(SgProject::get_verbose() > 0)
                              cout << get(boost::edge_name, s, *e.first) << "= (" << id << "," 
                                   << target(*e.first,s) <<")[" << get(boost::vertex_name, s, target(*e.first,s)) << "], ";
                         //Should new entry node be decided?
                           if(id == eN)
                              isEntryInCluster = true;
                         pqV.push( vertex(id, g) );
                     }
                     //Debug message?
                       if(SgProject::get_verbose() > 0)
                          cout << ")" << endl;
                   //Replace nodes with new abstract region...
                     for(; !pqV.empty(); pqV.pop()) {
                         GVertex v = pqV.top();
                         //Debug message?
                           if(SgProject::get_verbose() > 0)
                              cout << "Removing vertex " << v << endl;
                         //Handle adjustment of eN vertex index
                           if(eN > get(boost::vertex_index, g, v))
                              eNStepsBack++;
                         //Copy all out_edges (as long as target isn't the supernode) of vertex *it...
                           for(pair<GOEIter, GOEIter> eO = boost::out_edges(v, g); eO.first != eO.second; ++eO.first)
                               if((target(*eO.first, g) != superNode) && !edge(superNode, target(*eO.first, g), g).second)
                                  add_edge(superNode, target(*eO.first, g), g);
                         //Copy all in_edges (as long as source isn't the supernode) of vertex *it...
                           for(pair<GIEIter, GIEIter> eI = boost::in_edges(v, g); eI.first != eI.second; ++eI.first)
                               if((source(*eI.first, g) != superNode) && !edge(source(*eI.first, g), superNode, g).second)
                                  add_edge(source(*eI.first, g), superNode, g);
                         //Remove node...
                           clear_vertex(v, g);
                           translationVector.erase(translationVector.begin()+get(boost::vertex_index, g, v));
                           remove_vertex(v, g);
                         //Handle adjustment of superNode (last node) vertex index
                           superNode--;
                     }

                   translationVector.push_back(node);
                   if(SgProject::get_verbose() > 0)
                      cout << "Added supernode as " << (translationVector.size()-1) << endl;
                 //Decide result
                   if(isEntryInCluster)
                     eN = superNode;
                   else
                     eN -= eNStepsBack;
                 //Update post order...
                   Post = vector<GVertex>(num_vertices(g));
                   DFSVis<G> DFS_Postorder(Post);
                   vector<boost::default_color_type> color(num_vertices(g)); 
                   boost::depth_first_search(g, DFS_Postorder, &color[0], vertex(eN, g));
                   
                   return eN;
            }
          //Improper-interval minimization
            //Can't use local types of Minimize_Improper()::NCA (works in --std=c++0x) hence we declare them globally...
            typedef enum {WHITE, BLACK} COLOR;
            //Commuting comparator and hash function for pair of vertices...
            struct PWNCAComp : public std::binary_function<pair<GVertex, GVertex>, pair<GVertex, GVertex>, bool> {
               bool operator()(const pair<GVertex, GVertex> &p1, const pair<GVertex, GVertex> &p2) const {
                    if( ((p1.first == p2.first) && (p1.second == p2.second)) ||
                        ((p1.first == p2.second) && (p1.second == p2.first)) )
                         return true;
                    else
                        return false;
               }
            };
            struct PWNCAHash : public std::unary_function<pair<GVertex, GVertex>, size_t> {
               size_t operator()(const pair<GVertex, GVertex> &p) const {
                    return p.first * p.second;
               }
            };
            GVertex Minimize_Improper(G &g, GVertex v, set<GVertex> &NodeSet) {
               int i;
               //Find entry node...
                 GVertex vEntry = boost::graph_traits<G>::null_vertex();
                 for(std::pair<GVIter, GVIter> vP = vertices(g); vP.first != vP.second; ++vP.first)
                     if( (vEntry == boost::graph_traits<G>::null_vertex()) ||
                         (getPostOrderIndex(*vP.first,Post) > getPostOrderIndex(vEntry, Post)) )
                         vEntry = *vP.first;
               //Calculate smallest multiple-entry cycle where v is included
                 std::vector<unsigned int> component(num_vertices(g)), discover_time(num_vertices(g));
                 std::vector<boost::default_color_type> color(num_vertices(g));
                 std::vector<GVertex> root(num_vertices(g));
                 boost::strong_components(g, &component[0], boost::root_map(&root[0]).color_map(&color[0]).discover_time_map(&discover_time[0]));
                 set<GVertex> ISCCNodes, I; 
                 for(typename boost::graph_traits<G>::vertices_size_type cV = component[get(boost::vertex_index, g, v)], i = 0; i < component.size(); i++)
                    if( (component[i] == cV) && (NodeSet.find(vertex(i, g)) != NodeSet.end()) )
                       ISCCNodes.insert(vertex(i, g));
                 for(typename set<GVertex>::iterator it = ISCCNodes.begin(); it != ISCCNodes.end(); ++it) {
                     //Is '*it' an entry node? TRUE if in_degree_outside_SCC(G,v) = |{u \in G.V | <u,v> \in G.E AND u \not\in SCC(v)}| > 1.
                       typename boost::graph_traits<G>::degree_size_type inEdgesOutsideSCC = 0;
                       for(pair<GIEIter, GIEIter> eI = boost::in_edges(*it, g); eI.first != eI.second; ++eI.first)
                         if(ISCCNodes.find(source(*eI.first, g)) == ISCCNodes.end())
                            inEdgesOutsideSCC++;
                       if(inEdgesOutsideSCC > 0)
                          I.insert(*it);
                 }
               //Calculate nearest common dominator by nearest common ancestor on immediate dominator tree...
               //(Harel, D., Tarjan, "Fast algorithms for finding nearest common ancestors", SIAM Journal on Computing, Vol. 13, Issue 2, pp. 338-355, May 1984)
                 typedef map<GVertex, std::size_t> IndexMap;
                 IndexMap mapIndex;
                 boost::associative_property_map<IndexMap> indexMap(mapIndex);
                 i = 0;
                 for(std::pair<GVIter, GVIter> vP = vertices(g); vP.first != vP.second; ++vP.first)
                    put(indexMap, *vP.first, i++);

                 vector<GVertex> domTreePredVector = vector<GVertex>(num_vertices(g), boost::graph_traits<G>::null_vertex());
                 boost::iterator_property_map<typename vector<GVertex>::iterator, boost::associative_property_map<IndexMap> > domTreePredMap = 
                         boost::make_iterator_property_map(domTreePredVector.begin(), indexMap);
                 boost::lengauer_tarjan_dominator_tree(g, vEntry, domTreePredMap);

                 struct NCA : public std::unary_function<set<GVertex>, set<GVertex> > {
                    private:
                         G &g;
                         vector<GVertex> IDom, ancestor;
                         set<GVertex> I;
                         vector<COLOR> color;
                         boost::unordered_map<pair<GVertex, GVertex>, GVertex, PWNCAHash, PWNCAComp> pairWiseNCA;

                         typedef std::map<GVertex, std::size_t> rank_t;
                         typedef std::map<GVertex, GVertex> parent_t;
                         rank_t rank_map;
                         parent_t parent_map;
                         boost::associative_property_map<rank_t>   rank_pmap;
                         boost::associative_property_map<parent_t> parent_pmap;
                         boost::disjoint_sets<boost::associative_property_map<rank_t>, boost::associative_property_map<parent_t> > disjSet;
                         
                         void NCA_pre(GVertex u) {
                              disjSet.make_set(u);
                              ancestor[u] = u;
                              typename boost::graph_traits<G>::vertices_size_type id = 0;
                              for(typename vector<GVertex>::iterator it = IDom.begin(); it != IDom.end(); ++it, id++)
                                  if(*it == u) {
                                        GVertex v = vertex(id, g);
                                        NCA_pre(v);
                                        disjSet.link(u, v);
                                        ancestor[disjSet.find_set(u)] = u;
                                  }
                              color[u] = BLACK;
                              if(I.find(u) != I.end())
                                   for(typename set<GVertex>::iterator it = I.begin(); it != I.end(); ++it)
                                       if( (*it != u) && (color[*it] == BLACK) && (I.find(*it) != I.end()) )
                                          pairWiseNCA[ pair<GVertex, GVertex>(*it, u) ] = ancestor[disjSet.find_set(*it)];
                         }
                    public:
                         NCA(G &_g, vector<GVertex> &_IDom, GVertex root, set<GVertex> &_I) : 
                                                      g(_g), IDom(_IDom), ancestor(_IDom.size()), I(_I.begin(), _I.end()), color(_IDom.size(), WHITE),
                                                      rank_pmap(rank_map), parent_pmap(parent_map), disjSet(rank_pmap, parent_pmap) {
                              NCA_pre(root);
                         }
                         GVertex operator()(GVertex v1, GVertex v2) {
                              typename boost::unordered_map<pair<GVertex, GVertex>, GVertex, PWNCAComp>::iterator pIt;
                              if(v1 == v2)
                                   return ancestor[v1];
                              else if((pIt = pairWiseNCA.find(pair<GVertex, GVertex>(v1,v2))) != pairWiseNCA.end()) {
                                   return pIt->second;
                              } else
                                   ROSE_ASSERT(false);
                         }
                         GVertex operator()(set<GVertex> &o) {
                              //Error check
                                if(o.size() == 0)
                                   return boost::graph_traits<G>::null_vertex();
                              //Start with ancestor of arbitrary node...
                                typename set<GVertex>::iterator it = o.begin();
                                GVertex res = *it;
                              //Find NCA of "o" as a sequence of binary operations on (res, o.element)
                                for(++it; it != o.end(); ++it)
                                   res = (*this)(res, *it);
                              return res;
                         }
                         void printNCATable() {
                              cout << "----- NCA Table -----" << endl; 
                              for(typename boost::unordered_map<pair<GVertex, GVertex>, GVertex, PWNCAComp>::iterator it = pairWiseNCA.begin(); it != pairWiseNCA.end(); ++it)
                                  cout << "(" << it->first.first << "," << it->first.second << ") = " << it->second << endl;
                              cout << "-----           -----" << endl; 
                         }
                 } t(g, domTreePredVector, vEntry, I);
                 GVertex vNCD = t(I);
               //Construct minimal improper set...
                 set<GVertex> inclSet = set<GVertex>(vertices(g).first, vertices(g).second), exclSet(vertices(g).first, vertices(g).second);
                 exclSet.erase(exclSet.find(vNCD)); 
                 Path p = Path();
                 for(typename set<GVertex>::iterator it = exclSet.begin(); it != exclSet.end(); ++it)
                    if( PathFunc(g, vNCD, (GVertex &) *it, inclSet, p) )
                         for(typename set<GVertex>::iterator it2 = I.begin(); it2 != I.end(); ++it2)
                              if(PathFunc(g, (GVertex &) *it, (GVertex &) *it2, exclSet, p))
                                   I.insert(*it);
               //Move I to NodeSet
                 NodeSet.clear();
                 NodeSet.insert(I.begin(), I.end());
                 if(NodeSet.find(vNCD) != NodeSet.end())
                    NodeSet.erase(NodeSet.find(vNCD));
               //Return entry node...
                 return vNCD;                   
             }
     };
};
#endif
