/* Correct answer:
   -----------
     // Auto generated DOT graph.
     // Compiler .dot->.png: "dot -Tpng stree_out.dot > stree_out.png"
     // (The format was heavily insipred by Kalani Thielen's example at http://www.graphviz.org/content/psg)

     digraph G {
     compound=true;
     ranksep=1.25;
     fontsize=30;
     labelloc="t";label="Project: 'StructureTree'";
     bgcolor=white;

      "n0" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '0'"];
      "n1" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '1'"];
      "n2" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '2'"];
      "n3" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '3'"];
      "n4" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '4'"];
      "n5" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '5'"];
      "n6" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '6'"];
      "n7" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '7'"];
      "n8" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Node '8'"];
      "n9" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="If-Then"];
      "n10" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Block"];
      "n11" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="If-Then-Else"];
      "n12" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Block"];
      "n13" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="While"];
      "n14" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Block"];
      n9 -> n3 [penwidth=1 fontsize=14 fontcolor="grey28" label="0"];
      n9 -> n4 [penwidth=1 fontsize=14 fontcolor="grey28" label="1"];
      n10 -> n0 [penwidth=1 fontsize=14 fontcolor="grey28" label="3"];
      n10 -> n1 [penwidth=1 fontsize=14 fontcolor="grey28" label="4"];
      n11 -> n2 [penwidth=1 fontsize=14 fontcolor="grey28" label="0"];
      n11 -> n9 [penwidth=1 fontsize=14 fontcolor="grey28" label="1"];
      n11 -> n5 [penwidth=1 fontsize=14 fontcolor="grey28" label="2"];
      n12 -> n11 [penwidth=1 fontsize=14 fontcolor="grey28" label="3"];
      n12 -> n6 [penwidth=1 fontsize=14 fontcolor="grey28" label="4"];
      n13 -> n7 [penwidth=1 fontsize=14 fontcolor="grey28" label="0"];
      n13 -> n12 [penwidth=1 fontsize=14 fontcolor="grey28" label="1"];
      n14 -> n10 [penwidth=1 fontsize=14 fontcolor="grey28" label="3"];
      n14 -> n13 [penwidth=1 fontsize=14 fontcolor="grey28" label="4"];
      n14 -> n8 [penwidth=1 fontsize=14 fontcolor="grey28" label="5"];
     }
*/


int main(int argc, char *argv) {
     int ack = 2;
     int i;
     for(i = 0; i < 5; i++) {
         if(ack == 0) {
            if(i == 1)
                    ack++;
         } else
               ack--;
          ack += 2*ack;
     }
     return ack;
}
