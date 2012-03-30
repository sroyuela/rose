#include <stdlib.h>
#include <limits.h>
#include "rose.h"
#include "Transform/ft_transform.h"

int main(int argc, char *argv[]) {
     SgProject *project = frontend(argc, argv);
     ROSE_ASSERT(project != NULL);             

     FT::Transform::FailurePolicy_Adjudicator *fPIntra = new FT::Transform::FailurePolicy_Adjudicator(new FT::Transform::Adjucator_Voting());

     FT::Transform f(3, 2, *fPIntra);
     try {
          f.transformMulti(project);
     } catch(FT::Common::FTException &e) {
          cout << "FTException occured: " << e.what() << endl;
          return 1;
     }

     return backend(project);
}
