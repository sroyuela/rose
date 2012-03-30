#include <stdlib.h>
#include <iostream>
#include "rose.h"
#include "ft.h"

using namespace std;

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
