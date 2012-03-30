/* C Test #3:
     1) Check various basic statements
     2) Make sure the transformer doesn't "fault-tolerize" a stm more than once.
     3) Make sure BB's are not mixed up.
     4) Test floating point voter
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//Test multi
#define M_ 10
#define N_ 10

float abs(float x) {
     if(x < 0.0)
          return -x;
     return x;
}

int main () {
     float **A;

     srand ( time(NULL) );

     int i = 0, j, k, l;
     A = (float **) malloc(sizeof(float *) * M_);
     printf("Input:\n");
     do {
          j = 0;
          A[i] = (float *) malloc(sizeof(float) * N_);
          while(j < N_) {
               float d = (float)(rand() % 1000);
               A[i][j] = (rand() % 100) + (1.0 / (d == 0.0 ? 1.0 : d));
               j++;
          }
          for(j = 0; j < N_; j++)
               printf("%e ", A[i][j]);
          printf("\n");
          i = i+1;
     } while(i < M_);             

     #pragma resiliency
     { 
          #pragma resiliency
          printf("Output:\n");
          #pragma resiliency
          for (i = 0, j = 0; i < M_ && j < N_; j++) {
               int maxi = i;
               for(k = i+1; k < M_; ) {
                    if( abs(A[k][j]) > abs(A[maxi][j]) )
                         maxi = k;
                    k++;
               }
               #pragma resiliency
               if(A[maxi][j] != 0.0) {
                    //Swap...
                      #pragma resiliency
                      {
                           float *s = A[maxi];
                           A[maxi] = A[i];
                           A[i] = s;
                      }
                    //Divide
                      for(k = 0; k < N_; k++)
                         A[i][k] /= A[i][j];
                    //Subtract
                      for(k = i+1; k < M_; k++)
                         for(l = 0; l < N_; l++)
                              A[k][l] -= A[k][j] * A[i][l];
                    i = i + 1;
               }                     
          }
     }
     for(i = 0; i < M_; i++) {
          for(j = 0; j < N_; j++)
               printf("%e ", A[i][j]);
          printf("\n");
     }

     return 0;
}
