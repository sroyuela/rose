#include <iostream>
#include <iomanip>
#include <math.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <assert.h>
#include <fstream>

using namespace std;

#define SIZE 10000
float array[SIZE];

void relax ()
   {
     #pragma resiliency elemental
     for (int i = 1; i < SIZE-1; i++)
          array[i] = (array[i-1] + array[i+1]) / 2.0;
   }


int main ()
   {
     struct timeval start, end;
     gettimeofday(&start, NULL);
          relax();
     gettimeofday(&end, NULL);
     cout << "TIME: " << ((end.tv_usec  - start.tv_usec) * (1e-6) + (end.tv_sec - start.tv_sec)) << endl;
   }
