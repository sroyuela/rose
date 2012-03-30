/* Correct answer:
   -----------
     // Auto generated DOT graph.
     // Compiler .dot->.png: "dot -Tpng testWHILE+IF.dot > testWHILE+IF.png"
     // (The format was heavily insipred by Kalani Thielen's example at http://www.graphviz.org/content/psg)

*/


int main(int argc, char *argv) {
     int i,j,k,l;
     int ack = argc;
     for(i = 0; i < argc; i++) {
         for(j = 0; j < i; j++) {
             ack *= 2;
             for(k = i; k < argc; k++)
                 for(l = 0; l < k; l++)
                     if(k+l > ack)
                        goto ignore;
             ack += 100;
             ignore:
               ack++;
         }
         for(k = i; k < argc; k++)
             ack = i*k + 3*ack;
     }
     return ack;                 
}
