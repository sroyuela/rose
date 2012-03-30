#ifndef BINARY_ANALYSIS_HDR
#define BINARY_ANALYSIS_HDR

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/transitive_closure.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/bimap.hpp>
#include <boost/unordered_map.hpp>
#include <boost/dynamic_bitset.hpp>

#include <map>
#include <set>
#include <string>
#include <functional>

#include "rose.h"
#include "BinaryControlFlow.h"
#include <BinaryFunctionCall.h>
#include "BinaryLoader.h"
#include "SymbolicSemantics.h"
#include "YicesSolver.h"

//Type definitions
typedef BinaryAnalysis::FunctionCall::Graph CG;
typedef boost::graph_traits<CG>::vertex_iterator CG_VIter;

typedef BinaryAnalysis::ControlFlow::Graph CFG;
typedef boost::graph_traits<CFG>::vertex_descriptor CFGVertex;
typedef boost::graph_traits<CFG>::vertex_iterator CFGVIter;
typedef boost::graph_traits<CFG>::edge_descriptor CFGEdge;
typedef boost::graph_traits<CFG>::edge_iterator CFGEIter;
typedef boost::graph_traits<CFG>::out_edge_iterator CFGOEIter;
typedef boost::graph_traits<CFG>::in_edge_iterator CFGIEIter;

#include "UA.h"

//#define DEBUG_BIN_ANAL

using namespace std;

namespace BA {
     //Return CFG of all nodes in "cfg" that are reachable from entry node of function "funcName"
     CFG create_subcfg(CFG cfg, string funcName) {
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

};

#endif
