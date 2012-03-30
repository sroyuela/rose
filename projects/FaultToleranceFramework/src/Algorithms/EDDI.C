#include <stdlib.h>
#include <iostream>
#include "rose.h"
#include "ft.h"

#include "binaryTransform.h"
#include "schedulerList.h"
#include "regAllocLS.h"
#include "binaryAnalysis.h"

using namespace std;


class EDDI {
     private:
          typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, boost::property<boost::vertex_name_t, SgAsmStatement*> > BGraph;
          typedef boost::subgraph<BGraph> BSubGraph;
          typedef boost::graph_traits<BGraph >::vertex_descriptor BSubVertex;

          SgNode *topNode;
          Regis
     public:
          EDDI(SgNode *topNode) {this->topNode = topNode;}
          ~EDDI() {}

          bool operator()(SgNode *topNode = NULL) {
               if(topNode == NULL) {
                    if(this->topNode == NULL)
                         return false;
                    topNode = this->topNode;
               }
               switch(topNode->variantT()) {
                case V_SgProject: {
                    SgProject *p = static_cast<SgProject *>(topNode);
                    //Query AST nodes for interpretations...
                      std::vector<SgNode*> interps = NodeQuery::querySubTree(p, V_SgAsmInterpretation);
                      if(interps.size() == 0)
                         return false;
                    //Perform CFG analysis...
                      typedef BinaryAnalysis::ControlFlow::Graph CFG
                      CFG cfg = BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(interps.back());
                    //... and callgraph analysis...
                      BinaryAnalysis::FunctionCall cg_analyzer;
                      struct ExcludeLeftovers: public BinaryAnalysis::FunctionCall::VertexFilter {
                         bool operator()(BinaryAnalysis::FunctionCall *analyzer, SgAsmFunction *func) {
                              return func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS);
    	                    }
                      } exclude_leftovers;
                      cg_analyzer.set_vertex_filter(&exclude_leftovers);
                      typedef BinaryAnalysis::FunctionCall::Graph CG;
                      CG cg = cg_analyzer.build_cg_from_cfg<CG>(cfg);
                    //Perform EDDI operation for each function...
                      typedef boost::graph_traits<CG>::vertex_iterator FVIter; 
                      for(std::pair<FVIter, FVIter> vP = vertices(cg); vP.first != vP.second; ++vP.first) {
                          if( !(*this)(get(boost::vertex_name, cg, *vP.first)) )
                              return false;

                      }
                  } return true;
                case V_SgAsmFunction: {
                    SgAsmFunction *f = static_cast<SgAsmFunction *>(topNode);
                    //Initialize state...
                      
                    //Perform EDDI operator for each block...
                      for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin(), itS = it; it != bb->get_statementList().end(); ++it)
                         if( !(*this)( *it ) )
                              return false;
                  } return true;
                case V_SgAsmBlock: {
                    SgAsmBlock *b = static_cast<SgAsmBlock *>(topNode);
                    //Compute dependency relation for instructions in BB...
                      BA::DependencyAnalysis<BinTransPolicyX86> d();
                      try {
                           if(!d(cfgSubset))
                              cout << "Unable to compute dependency analysis" << endl;
                      } catch(const BA::AnalysisBase::BAException &e) {
                         cout << "Exception occured: " << e.what() << endl;
                      }
                    /*Convert BB to a graph with SBB (Storeless Basic Block,
                      one or no store instructions at end of block) nodes.*/
                      BSubGraph Gbb, Gsbb = Gbb.create_subgraph(), Gsbb_entry = Gsbb;
                      BSubVertex prevNode = boost::graph_traits<BSubGraph>::null_vertex(); 
                      boost::property_map<BGraph, boost::vertex_name_t>::type bVertMap = get(boost::vertex_name, Gsbb);
                      for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin(), itS = it; it != bb->get_statementList().end(); ++it) {
                         //Add instruction as node to graph...
                           BSubVertex node = add_vertex(Gbb);
                           bVertMap[node] = *it;
                           if(prevNode != boost::graph_traits<BSubGraph>::null_vertex())
                              add_edge(prevNode, node, Gbb);
                         //Should "node" to moves into local Gsbb or a new subgraph?
                           switch( (*it)->variantT() ) {
                            case V_SgAsmArmInstruction:
                            case V_SgAsmPowerpcInstruction:
                              ROSE_ASSERT(false);
                            case V_SgAsmx86Instruction:
                              /*Evaluate state change...
                                (x86 is not a load-store architecture -> use symbolic semantics to
                                 tell if memory was updated)*/
                                
                              //Is this a store instruction?
                                if(semPolicy.get_state().mem.begin() == semPolicy.get_state().mem.end()) {
                                   BSubGraph Gsbb_new = Gbb.create_subgraph();
                                   add_edge(Gsbb, Gsbb_new, Gbb);
                                   Gsbb = Gsbb_new;
                                }
                              break;
                            case V_SgAsmStaticData:
                              break:
                            case V_SgAsmBlock:
                            case V_SgAsmFunction:
                            case V_SgAsmInstruction:
                            default:
                              ROSE_ASSERT(false);
                           }
                           add_vertex(node, Gsbb);
                      }
                  } return true;
                default:
                    return false;
               }
          }
};

int main(int argc, char *argv[]) {
     //Compile sources...
       SgProject *project = frontend(argc, argv);
       ROSE_ASSERT(project != NULL); 
       string cmd;
       char *pPath = getenv("CXX");
       if(pPath == NULL) {
            cout << "Unable to get CXX varible..." << endl;
            _exit(1);
       }
       cmd.append(pPath);
       SgStringList &includeList = project->get_includePathList(),
                    &srcList = project->get_sourceFileNameList();
       for(SgStringList::iterator it = includeList.begin();
           it != includeList.end();
           ++it)
               cmd.append(" -I" + *it);
       for(SgStringList::iterator it = srcList.begin();
           it != srcList.end();
           ++it)
               cmd.append(" " + *it);
       cmd.append(" -m32 -g -o ftOutput > outputGCC.txt 2>&1");
       cout << "==EXEC== '" << cmd << "'" << endl;
       int status = system(cmd.c_str());
       if(status != 0) {
          cout << "Error during compilation, quitting!" << endl;
          exit(status);
       }
       TInterface::closeProject(project);

     //Analyze binary
       char *argvB[] = {argv[0], "-rose:verbose", "6", "ftOutput"};
       SgProject *binProject = frontend(4, argvB);
       ROSE_ASSERT(binProject != NULL);
       FTAnalysis ftA(binProject);
       ftA.analyzeMulti(binProject);
       
     /*
     //Apply transforms...
     FTOp::FailurePolicy_DieOnError *fPInter = new FTOp::FailurePolicy_DieOnError();
     FTOp::FailurePolicy_Voting *fPIntra2 = new FTOp::FailurePolicy_Voting();
     FTOp::FailurePolicy_SecondChance *fPIntra = new FTOp::FailurePolicy_SecondChance(2, *fPIntra2);
     FTOp f(0, *fPInter, 2, *fPIntra2, project, SageInterface::getFirstGlobalScope(project));
     try {
          f.transformMulti(project);
     } catch(FTOp::FTException &e) {
          cout << "FTException occured: " << e.what() << endl;
     }*/

     //return backend(project);
     return 0;
}
