/* C Test #3:
*/

int main () {
     int i, j, k;
     #pragma resiliency
     for(i = 0; ; i++)
          for(j = 0; ; j++)
               for(k = 0; ; k++)
                    switch(i+j+k) {
                     case 0:
                     case 1:
                         break;
                     case 2:
					switch(k) {
                          case 0: j++;   break;
                          case 1: return 1;
                          default:break;
                         }
                         goto out;
                     default:
                         break;
                    }
     out:
          return 0;
}
