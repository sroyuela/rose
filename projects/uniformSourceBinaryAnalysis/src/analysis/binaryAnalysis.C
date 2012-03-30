#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/transitive_closure.hpp>
#include <boost/graph/depth_first_search.hpp>

#include <map>
#include <set>
#include <string>
#include <functional>

#include "rose.h"
#include "binaryAnalysis.h"
#include "BinaryControlFlow.h"

using namespace std;

//Return CFG of all nodes in "cfg" that are reachable from entry node of function "funcName"

CFG BA::create_subcfg(CFG cfg, string funcName) {
     //Compute call graph of g
       BinaryAnalysis::FunctionCall cg_analyzer;
       struct ExcludeLeftovers: public BinaryAnalysis::FunctionCall::VertexFilter {
          bool operator()(BinaryAnalysis::FunctionCall *analyzer, SgAsmFunction *func) {
               return func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS);
          }
       } exclude_leftovers;
       cg_analyzer.set_vertex_filter(&exclude_leftovers);
       CG cg = cg_analyzer.build_cg_from_cfg<CG>(cfg);
     //Iterate over all functions and find "funcName" and its entrynode...
       CFGVertex vMain = boost::graph_traits<CFG>::null_vertex();
       for(pair<CG_VIter, CG_VIter> vP = vertices(cg); vP.first != vP.second; ++vP.first) {
          SgAsmFunction *f = get(boost::vertex_name, cg, *vP.first);
          if(f->get_name() == funcName) {
             //Find the entry block vertex...
               for(pair<CFGVIter, CFGVIter> vPCFG = vertices(cfg); vPCFG.first != vPCFG.second; ++vPCFG.first) {
                   SgAsmBlock *bb = get(boost::vertex_name, cfg, *vPCFG.first);
                   if(f->get_entry_block() == bb)
                      vMain = *vPCFG.first;
               }
          }
       }
       ROSE_ASSERT(vMain != boost::graph_traits<CFG>::null_vertex());
     //Create sub-cfg
       boost::bimap<boost::graph_traits<CFG>::vertex_descriptor, boost::graph_traits<CFG>::vertex_descriptor> subMap;
       CFG subCFG = create_subg(cfg, vMain, subMap, true, false);
     //Update vertex name...
       boost::property_map<CFG, boost::vertex_name_t>::type vertGMap = get(boost::vertex_name, cfg), vertNewGMap = get(boost::vertex_name, subCFG);
       for(pair<CFGVIter, CFGVIter> vP = vertices(subCFG); vP.first != vP.second; ++vP.first)
           vertNewGMap[*vP.first] = vertGMap[subMap.right.at(*vP.first)];
     //Return results...
       return subCFG;
               
}
