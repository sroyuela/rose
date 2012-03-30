#include "rose.h"
#include "TI.h"
#include "binaryTransform.h"
#include "schedulerList.h"
#include "regAllocLS.h"

#include "binaryAnalysis.h"
#include "binaryDotGraph.h"

#include <boost/graph/graphviz.hpp>

using namespace BT;

typedef BinaryAnalysis::ControlFlow::Graph CFG;
typedef BinaryAnalysis::FunctionCall::Graph CG;
typedef boost::graph_traits<CG>::vertex_iterator CG_VIter;

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
       cmd.append(" -O0 -m32 -g -o ftOutput > outputGCC.txt 2>&1");
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

       //SgProject *binProject = frontend(argc, argv);
       //ROSE_ASSERT(binProject != NULL);
       /*struct Visitor : public ROSE_VisitTraversal {
          private:
               void visit(SgNode* node) {
                    if(node == NULL)
                       return;
                    cout << node->class_name() << " at" << node << endl;
               }
          protected:
             
          public:
                 Visitor() {}
       } t;
       t.traverseMemoryPool();*/

     //Query AST nodes for interpretations...
       vector<SgAsmInterpretation *> interps = SageInterface::querySubTree<SgAsmInterpretation>(binProject);
       if(interps.size() == 0) {
          cout << "No interpretations found" << endl;
          return 1;
       }
     //Perform CFG analysis...
       CFG cfg = BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(interps.back());
       CFG cfgSubset = BA::create_subcfg(cfg, "main");

     BinaryDotGenerator dot(*binProject, cfgSubset, "ftOutput", "dotG.dot");
     BinaryDotGenerator dotw(*binProject, cfg, "ftOutput", "dot_whole.dot");

     /*typedef boost::graph_traits<CFG>::vertex_iterator FGVIter;
     cout << "BBs: " << endl;
     for(pair<FGVIter, FGVIter> vP = vertices(cfgSubset); vP.first != vP.second; ++vP.first) {
          SgAsmBlock *bb = static_cast<SgAsmBlock *>(get(boost::vertex_name, cfgSubset, *vP.first));
          cout << bb << " {";
          for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin(); it != bb->get_statementList().end(); ++it)
               cout << *it << ", ";
          cout << "}" << endl;
     }
     cout << endl;

          struct FilterFunctor : public BA::RWAnalysis<BinTransPolicyX86>::FilterFunctor {
                 virtual bool operator()(unsigned long int i) const {
                    if(BinTransPolicyX86::isGpReg(i) || BinTransPolicyX86::isSegReg(i))
                         return true;
                    else
                         return false;
                 }
                 virtual bool operator()(SymbolicSemantics::ValueType<32> addr) const {
                    return false;
                 }
          } f;

     BA::LivenessAnalysis<BinTransPolicyX86> s(&f);
     try {
          if(!s(cfgSubset))
               cout << "Unable to compute liveness analysis" << endl;
     } catch(const BA::AnalysisBase::BAException &e) {
          cout << "Exception occured: " << e.what() << endl;
     }
     cout << s << endl;

     /*struct FA : public BA::DependencyAnalysis<BinTransPolicyX86>::FilterFunctor {
          virtual bool operator()(BA::DependencyAnalysis<BinTransPolicyX86>::DEPENDENCE_TYPE &t) {
               //t.INPUT = false;
               return true;
          }
     } f1;
     BA::DependencyAnalysis<BinTransPolicyX86> d(&f1, &f);
     try {
          for(pair<FGVIter, FGVIter> vP = vertices(cfgSubset); vP.first != vP.second; ++vP.first) {
               if(!d(get(boost::vertex_name, cfgSubset, *vP.first)))
                    cout << "Unable to compute dependency analysis" << endl;
          }
     } catch(const BA::AnalysisBase::BAException &e) {
          cout << "Exception occured: " << e.what() << endl;
     }
     cout << d << endl;*/

     //BinaryTransform<BinTransPolicyX86> b();
           

     return 0;
}
