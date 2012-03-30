#include <boost/graph/adjacency_list.hpp>

#include <rose.h>
#include "analysis/UA.h"

int main(int argc, char *argv[]) {
     //Test to make sure other graphs than CFGs can be handled...
          /*Muchnick example p. 213, Fig. 7.49a
            Result = Improper(Entry=0,Nodes={1,2,3,4}), Block=(Entry=Improper,Nodes={5})*/
          typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, boost::property<boost::vertex_name_t, unsigned int>, boost::property<boost::edge_name_t, char> > GraphA;
          cout << "Graph test A)" << endl;
          GraphA a(6);
          add_edge(0,1,a); add_edge(0,2,a);
          add_edge(1,3,a); add_edge(1,4,a);
          add_edge(2,3,a); add_edge(2,4,a);
          add_edge(3,4,a); add_edge(3,5,a);
          add_edge(4,3,a); add_edge(4,5,a);
          BA::StructAnal<GraphA> sA = BA::StructAnal<GraphA>();
          BA::StructAnal<GraphA>::StructureTree stA = sA(a);
          cout << sA.printSTree(stA, "test_streeA.dot") << endl;
          /*Muchnick example p. 213, Fig. 7.49b
            Result = Improper(Entry=0,Nodes={1,2,3,4})*/
          typedef boost::adjacency_list<boost::listS, boost::vecS, boost::bidirectionalS, boost::property<boost::edge_index_t, char> > GraphB;
          cout << "Graph test B)" << endl;
          GraphB b(5);
          add_edge(0,1,b); add_edge(0,2,b); add_edge(0,3,b);
          add_edge(1,4,b);
          add_edge(2,3,b); add_edge(2,4,b);
          add_edge(3,2,b);
          add_edge(4,1,b);
          BA::StructAnal<GraphB> sB = BA::StructAnal<GraphB>();
          BA::StructAnal<GraphB>::StructureTree stB = sB(b);
          cout << sB.printSTree(stB, "test_streeB.dot") << endl;
          /*Muchnick example p. 211, Fig. 7.46
            Result = 9-If-Then(0=5, 1=6), 10-Block(3=0, 4=1), 11-Block (4=7, 3=9), 12-If-Then-Else (0=3, 1=4, 2=11), 
                     13-Block(4=8, 3=12), 14-Self-loop (0=2), 15-If-Then (0=10, 1=14), 16-Block (4=13, 3=15)*/
          typedef boost::adjacency_list<boost::multisetS, boost::vecS, boost::bidirectionalS> GraphC;
          cout << "Graph test C)" << endl;
          GraphC c(9);
          add_edge(0,1,c);
          add_edge(1,2,c); add_edge(1,3,c);
          add_edge(2,2,c); add_edge(2,3,c);
          add_edge(3,4,c); add_edge(3,5,c);
          add_edge(4,8,c);
          add_edge(5,6,c); add_edge(5,7,c);
          add_edge(6,7,c);
          add_edge(7,8,c);
          BA::StructAnal<GraphC> sC = BA::StructAnal<GraphC>();
          BA::StructAnal<GraphC>::StructureTree stC = sC(c);
          cout << sC.printSTree(stC, "test_streeC.dot") << endl;

     //Return success...
       return 0;
}
