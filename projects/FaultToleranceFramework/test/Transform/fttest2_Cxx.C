#include<vector>
#include<iostream>

using namespace std;

struct IntW {
     int i;
     IntW(int i) {this->i = i;}
};

int main(int argc, char *argv[]) {
     vector<IntW *> a;
     a.push_back(new IntW(0));
     a.push_back(new IntW(4));
     a.push_back(new IntW(10));
     a.push_back(new IntW(3));
     a.push_back(new IntW(7));
     a.push_back(new IntW(8));
     a.push_back(new IntW(0));
     a.push_back(new IntW(2));
     a.push_back(new IntW(5));

     cout << "Input:  ";
     for(int i = 0; i < a.size(); i++)
          cout << a[i]->i << ", ";
     cout << endl;
     bool swap;
     #pragma resiliency
     #pragma resiliency
     do {
          #pragma resiliency
          swap = false;
          #pragma resiliency
          for(int i = 1; i < a.size(); i++)
               if(a[i-1]->i > a[i]->i) {
                    IntW *tmp = a[i-1]; //side-effect analysis can't handle this...
                    a[i-1] = a[i];
                    a[i] = tmp;
                    swap = true;
               }
     } while(swap);
     cout << "Output: ";
     for(int i = 0; i < a.size(); i++)
          cout << a[i]->i << ", ";
     cout << endl;

     return 0;
}
