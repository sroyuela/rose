/* C Test #4:
*/

int main () {
     int a=1,b=2,c=3,d=4,e=5,f=6,g=7,h=8;
     #pragma resiliency
     {
          a = g*(b = a + 2);     
          a = 2*g;
     }
}
