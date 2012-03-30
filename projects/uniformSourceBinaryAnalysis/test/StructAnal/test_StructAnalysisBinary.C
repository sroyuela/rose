#include <iostream>
#include <rose.h>
#include "analysis/binaryAnalysis.h"

using namespace std;

int main(int argc, char *argv[]) {
     //Handle command line arguments and open potential project...
       SgProject *binProject = frontend(argc, argv);
       ROSE_ASSERT(binProject != NULL);
     //Query AST nodes for interpretations...
       vector<SgAsmInterpretation *> interps = SageInterface::querySubTree<SgAsmInterpretation>(binProject);
     //Run internal testgraphs?
       if(interps.size() == 0) {
          cout << "No SgAsmInterpretation nodes found..." << endl;
          return 1;
       } else {
          //Compute CFG
            CFG cfg = BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(interps.back());
            CFG cfgSubset = BA::create_subcfg(cfg, "main");
          //Perform Structure analysis
            BA::StructAnal<CFG> sA = BA::StructAnal<CFG>();
            BA::StructAnal<CFG>::StructureTree st = sA(cfgSubset);
            cout << sA.printSTree(st, "test_stree.dot") << endl; 
       }
     //Return success...
       return 0;
}
