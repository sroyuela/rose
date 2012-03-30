//#define PC_COMP

#include <iostream>
#include "PCSet.h"
#ifdef PC_COMP
     #include <sstream>
#endif

using namespace std;

#pragma UQ_DECLARE exp(exp)
#pragma UQ_PROCESS variables(x,y,z)
int main() {
  #ifdef PC_COMP
	  //Stochastic/PC portion of the code

	  const int dim = 1; //Dimension
	  const int ord = 3; //Order

	  PCSet pc(ord,dim,"HG");

	  const int nPts = pc.GetNumberPCTerms(); //# of basis functions
  #endif

  const double defaultVal = 1.0e0;

  //Kernel
  const int N = 10;
  const double ALPHA = 1.2;
  #ifdef PC_COMP
	  UQTKArray1D<double> x[N], y[N], z[N], tmpReg0[N];
	  for(int i = 0; i < N; i++) {
	     x[i] = UQTKArray1D<double>(nPts,defaultVal);
	     y[i] = UQTKArray1D<double>(nPts,defaultVal);
	     z[i] = UQTKArray1D<double>(nPts,defaultVal);
	     tmpReg0[i] = UQTKArray1D<double>(nPts,defaultVal);
	  }

	  for(int i = 0; i < N; i++)
	     pc.Add( pc.Multiply(x[i], ALPHA, tmpReg0[i]), y[i], z[i] );

  	  for(int i = 0; i < N; i++) {
     	stringstream ss_z, ss_x, ss_y;
     	for(int j = 0; j < nPts; j++) {
          	ss_z << z[i](j);
          	ss_x << x[i](j);
          	ss_y << y[i](j);
          	if(j != nPts-1) {
               	ss_z << ", ";
               	ss_x << ", ";
               	ss_y << ", ";
          	}
     	}
     	cout << i << ") [" << ss_z.str() << "] = " << ALPHA << "*[" << ss_x.str() << "] + [" << ss_y.str() << "]" << endl;
  	  }
  #else
	  double x[N], y[N], z[N];
	  for(int i = 0; i < N; i++) {
	     x[i] = defaultVal;
	     y[i] = defaultVal;
	     z[i] = defaultVal;
	  }
	  for(int i = 0; i < N; i++)
	     z[i] = ALPHA * x[i] + y[i];
  	  //for(int i = 0; i < N; i++)
      //	cout << i << ") " << z[i] << " = " << ALPHA << "*" << x[i] << " + " << y[i] << endl;
  #endif
  return(0);
}
