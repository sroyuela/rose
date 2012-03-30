/* Correct answer:
   -----------
     // Auto generated DOT graph.
     // Compiler .dot->.png: "dot -Tpng testWHILE+IF.dot > testWHILE+IF.png"
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
      "n3" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Self-loop"];
      "n4" [style="filled" penwidth=1 fillcolor="white" fontname="Courier New" label="Block"];
      n3 -> n2 [penwidth=1 fontsize=14 fontcolor="grey28" label="0"];
      n4 -> n0 [penwidth=1 fontsize=14 fontcolor="grey28" label="3"];
      n4 -> n1 [penwidth=1 fontsize=14 fontcolor="grey28" label="4"];
      n4 -> n3 [penwidth=1 fontsize=14 fontcolor="grey28" label="5"];
     }
*/

int main(int argc, char *argv) {
     infinite_loop:
          goto infinite_loop;
     return 0;
}
