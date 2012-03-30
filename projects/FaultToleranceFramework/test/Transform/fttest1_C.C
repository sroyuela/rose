#include<stdio.h>
#include<stdlib.h>

int arraySize = 20;
int numberOfIterations = 1000;

float array[10];

void initialize() {
      for (int i = 0; i < arraySize; i++)
         array[i] = 1.0;

      array[0] = 0.0;
      array[arraySize-1] = 0.0;
}


void relax () {
     #pragma resiliency
     for (int i = 1; i < arraySize-1; i++)
          array[i] = (array[i-1] + array[i+1]) / 2.0;
}

int main () {
     initialize();

     printf ("initial values = ");
     for (int i = 0; i < arraySize; i++)
        printf ("%e ",array[i]);
     printf ("\n");

     for (int i = 0; i < numberOfIterations; i++)
        {
        relax();
          //relax_tmr_elemental();
        }

     printf ("result ========= ");
     for (int i = 0; i < arraySize; i++)
        printf ("%e ",array[i]);
     printf ("\n");
   }
