#ifndef BIN_DOT_GRAPH_HEADER
#define BIN_DOT_GRAPH_HEADER

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <string>

#include "rose.h"
#include "BinaryControlFlow.h"
#include "BinaryLoader.h"

using namespace std;

       class BinaryDotGenerator {
          private:
               typedef BinaryAnalysis::ControlFlow::Graph CFG;
               typedef BinaryAnalysis::FunctionCall::Graph CG;
               typedef boost::graph_traits<CFG>::vertex_iterator CFGVIter;
               typedef boost::graph_traits<CG>::vertex_iterator CG_VIter;

               ofstream f;
               map<SgAsmBlock *, string> nameMap;
               map<SgAsmBlock *, set<SgAsmBlock *> *> withinF, outsideF;
               string stringFormatter(string s) {
                    stringstream ss;
                    for(int i = 0; i < s.size(); i++)
                         switch(s[i]) {
                          case '&':     ss << "&amp;";      break;
                          case '\"':    ss << "&quot;";     break;
                          case '\'':    ss << "&apos;";     break;
                          case '<':     ss << "&lt;";       break;
                          case '>':     ss << "&gt;";       break;
                          default:      ss << s[i];         break;
                         }
                    return ss.str();                    
               }
          public:
               BinaryDotGenerator(SgProject &p, CFG &cfg, string programName, string filename) : f(filename.c_str()) {
                    //Initialize
                      if(!f.good())
                         return;
                      f << "/* Auto generated DOT graph." << endl
                        << "  Compiler .dot->.png: \"dot -Tpng " << filename << " > " << filename << ".png\"" << endl
                        << "  (The format was heavily insipred by Kalani Thielen's example at http://www.graphviz.org/content/psg)*/" << endl << endl
                        << "digraph G {" << endl << "compound=true;" << endl
                        << "ranksep=1.25;" << endl << "fontsize=30;" << endl << "labelloc=\"t\";" << "label=\"Project: '" << programName << "'\";" << endl
                        << "bgcolor=white;" << endl
                        << endl;
                    //Perform and callgraph analysis...
                      BinaryAnalysis::FunctionCall cg_analyzer;
                      /*struct ExcludeLeftovers: public BinaryAnalysis::FunctionCall::VertexFilter {
                         bool operator()(BinaryAnalysis::FunctionCall *analyzer, SgAsmFunction *func) {
                              return func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS);
    	                    }
                      } exclude_leftovers;
                      cg_analyzer.set_vertex_filter(&exclude_leftovers);*/
                      CG cg = cg_analyzer.build_cg_from_cfg<CG>(cfg);
                    //Iterate over all functions...
                      int index = 0;
                      for(pair<CG_VIter, CG_VIter> vP = vertices(cg); vP.first != vP.second; ++vP.first, ++index) {
                          stringstream name;
                          name << "func" << index;
                          (*this)(get(boost::vertex_name, cg, *vP.first), name.str());
                      }
                    //Add all outside edges...
                    f << endl;
                    for(map<SgAsmBlock *, set<SgAsmBlock *> *>::iterator it = outsideF.begin();
                        it != outsideF.end();
                        ++it) {
                         for(set<SgAsmBlock *>::iterator it2 = it->second->begin();
                             it2 != it->second->end();
                             ++it2)
                              f << " " << nameMap.find(it->first)->second << " -> " << nameMap.find(*it2)->second
                                << " [penwidth=1 fontsize=28 fontcolor=\"black\" label=\"\"];" << endl;
                         delete it->second;
                     }
                     f << "}" << endl;
                    //Finalize
                      f.close();
               }
               int operator()(SgAsmBlock *bb, string name, int index) {
                    if((bb == NULL) || (nameMap.find(bb) != nameMap.end()))
                         return 0;
                    stringstream bbName;
                    bbName << name << index;
                    nameMap[bb] = bbName.str();
                    f << " \"" << bbName.str() << "\" [style=\"filled\" penwidth=1 fillcolor=\"white\" fontname=\"Courier New\" shape=\"Mrecord\" label="
                      << "<"
                         << "<table border=\"0\" cellborder=\"0\" cellpadding=\"3\" bgcolor=\"white\">"
                              << "<tr>"
                                   << "<td bgcolor=\"black\" align=\"center\" colspan=\"2\">"
                                        << "<font color=\"white\">" << "BB #" << bb->get_id() << "</font>"
                                   << "</td>"
                              << "</tr>";
                    int Iindex = 0, Dindex = 0;
                    for(SgAsmStatementPtrList::iterator it = bb->get_statementList().begin(); it != bb->get_statementList().end(); ++it) {
                         stringstream ss;
                         switch((*it)->variantT()) {
                          case V_SgAsmBlock:
                          case V_SgAsmFunction:
                              ROSE_ASSERT(false);
                          case V_SgAsmInstruction:
                          case V_SgAsmArmInstruction:
                          case V_SgAsmPowerpcInstruction:
                          case V_SgAsmx86Instruction:
                              ss << "[I" << Iindex << "] " << stringFormatter(unparseInstructionWithAddress(static_cast<SgAsmInstruction *>(*it)));
                              Iindex++;
                              break;
                          case V_SgAsmStaticData:
                              ss << "[D" << Dindex << " (" << ")] "; //static_cast<SgAsmStaticData *>(*it)->get_size() <<
                              for(SgUnsignedCharList::iterator it2 = static_cast<SgAsmStaticData *>(*it)->get_raw_bytes().begin();
                                  it2 != static_cast<SgAsmStaticData *>(*it)->get_raw_bytes().end();
                                  ++it2)
                                   ss << std::hex << *it2 << " ";
                              Dindex++;
                              break;
                         }
                         f << "<tr><td align=\"left\" port=\"r4\">" << ss.str() << "</td></tr>";
                    }
                    f << "</table>> ];" << endl; 

                    set<SgAsmBlock *> *wF = new set<SgAsmBlock *>(), *oF = new set<SgAsmBlock *>();
                    int fIndex = 1;
                    for(SgAsmTargetPtrList::iterator it = bb->get_successors().begin(); it != bb->get_successors().end(); ++it) {
                         SgAsmTarget *t = *it;
                         if((t != NULL) && (t->get_block() != NULL)) {
                              if(t->get_block()->get_enclosing_function() != bb->get_enclosing_function())
                                   oF->insert(t->get_block());
                              else {
                                   wF->insert(t->get_block());
                                   fIndex += (*this)(t->get_block(), name, index+fIndex);
                              }
                         }
                    }
                    withinF[bb] = wF;
                    outsideF[bb] = oF;  

                    return fIndex;                                
               }
               void operator()(SgAsmFunction *func, string name) {
                    if(func == NULL)
                         return;
                    string kind, cc;
                    switch(func->get_function_kind()) {
                     case SgAsmFunction::e_unknown:    kind = "Unknown";   break;
                     case SgAsmFunction::e_standard:   kind = "Standard";  break;
                     case SgAsmFunction::e_library:    kind = "Library";   break;
                     case SgAsmFunction::e_imported:   kind = "Imported";  break;
                     case SgAsmFunction::e_thunk:      kind = "Thunk";     break;
                     case SgAsmFunction::e_last:       kind = "Last";      break;
                    }
                    /*switch(func->get_functionCallingConvention()) {
                     case SgAsmFunction::e_unknown_call:    cc = "Unknown";     break;
                     case SgAsmFunction::e_std_call:        cc = "Std";         break;
                     case SgAsmFunction::e_fast_call:       cc = "Fast";        break;
                     case SgAsmFunction::e_cdecl_call:      cc = "CDecl";       break;
                     case SgAsmFunction::e_this_call:       cc = "This";        break;
                     case SgAsmFunction::e_last_call:       cc = "Last";        break;
                    }*/
                    f << "subgraph cluster" << name << " {" << endl << "node [style=filled];" << endl 
                      << "color=blue;" << endl << "fontsize=20;" << endl
                      << "label=\"Function '" << func->get_name() << "' (Kind=" << kind << ", CC=" << cc << ")\";" << endl
                      << "labelloc=\"t\";" << endl << "overlap=false;" << endl
                      << "rankdir=\"LR\";" << endl;

                    (*this)(func->get_entry_block(), name, 0);

                    f << endl;
                    for(map<SgAsmBlock *, set<SgAsmBlock *> *>::iterator it = withinF.begin();
                        it != withinF.end();
                        ++it) {
                         for(set<SgAsmBlock *>::iterator it2 = it->second->begin();
                             it2 != it->second->end();
                             ++it2)
                              f << " " << nameMap.find(it->first)->second << " -> " << nameMap.find(*it2)->second
                                << " [penwidth=1 fontsize=14 fontcolor=\"grey28\" label=\"\"];" << endl;
                         delete it->second;
                    }
                    withinF.clear();
                    f << "}" << endl;                    
               }
               
       };
#endif
