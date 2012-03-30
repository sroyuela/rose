#include<queue>
#include<set>
#include<map>
#include<vector>
#include<string>
#include<limits.h>
#include<iostream>
#include<stdexcept>
#include<assert.h>

using namespace std;

typedef struct _QE {
     string node;
     unsigned int dist;

     _QE(string &s, unsigned int d) {node = s; dist = d;}
     unsigned int getDist() const {return dist;}

     //#pragma resiliency
     bool operator>(const struct _QE& x) const {
          return getDist() > x.getDist();
     }
} qEntry;

template<typename nodeT>
class Graph {
     private:
          bool undirected;
          set<nodeT> V_;
          map<nodeT, std::map<nodeT, unsigned int> *> E;
     public:
          Graph(bool undirected = true) {this->undirected = undirected;};
          ~Graph() {};

          void addNode(nodeT node) {V_.insert(node);}

          //#pragma resiliency
          void setDistance(nodeT startNode, nodeT endNode, unsigned int d) {
               map<nodeT, unsigned int> *sMap;
               //Checks...
                 if(startNode == endNode)
                    return;
                 if(V_.find(startNode) == V_.end())
                    throw runtime_error("Invalid node '" + startNode + "'");
                 if(V_.find(endNode) == V_.end())
                    throw runtime_error("Invalid node '" + endNode + "'");                 
               //Add node...
                 if(E.find(startNode) == E.end()) {
                    sMap = new std::map<nodeT, unsigned int>();
                    E[startNode] = sMap;
                 } else
                    sMap = E.find(startNode)->second;
                 sMap->insert( pair<nodeT, unsigned int>(endNode, d) );
               //Add node in other direction if undirected
                 if(undirected) {
                      if(E.find(endNode) == E.end()) {
                         sMap = new std::map<nodeT, unsigned int>();
                         E[endNode] = sMap;
                      } else
                         sMap = E.find(endNode)->second;
                      sMap->insert( pair<nodeT, unsigned int>(startNode, d) );
                 }
          }

          unsigned int getDistance(nodeT startNode, nodeT endNode) {
               if(startNode == endNode)
                    return 0;
               if(E.find(startNode) == E.end())
                    return UINT_MAX;
               map<nodeT, unsigned int> *sMap = E.find(startNode)->second;
               if(sMap->find(endNode) == sMap->end())
                    return UINT_MAX;
               return sMap->find(endNode)->second;
          }
          
          std::map<nodeT, unsigned int> *getNeighbors(nodeT node) {
               //#pragma resiliency
               if(E.find(node) == E.end())
                    return NULL;
               else
                    return E.find(node)->second;
          }

          set<nodeT> &V() {return V_;}        
};

map<string, unsigned int> *dijkstra(Graph<string> &g, string source) {
     map<string, unsigned int> *dist = new map<string, unsigned int>;
     priority_queue<qEntry ,vector<qEntry>, std::greater<qEntry> > Q;
     //Initialization...
       for(std::set<string>::iterator it = g.V().begin();
           it != g.V().end();
           ++it) {
            (*dist)[*it] = UINT_MAX;
            Q.push( qEntry( (string &) *it, UINT_MAX) );
       }
       (*dist)[source] = 0;
       Q.push( qEntry( source, 0) );
     //Compute min path...
       //#pragma resiliency
       for (; !Q.empty(); Q.pop()) {
          qEntry qE = Q.top();
          //Make sure remaining V's are accessible...
            if(dist->find(qE.node)->second == UINT_MAX)
               break;
          //Explore
            std::map<string, unsigned int> *nE = g.getNeighbors(qE.node);
            if(nE == NULL)
               break; //qE has no neighbors!
            for(std::map<string, unsigned int>::iterator it = nE->begin();
                it != nE->end();
                ++it) {
                    unsigned int distTot = dist->find(qE.node)->second + it->second;
                    if(distTot < dist->find(it->first)->second) {
                         (*dist)[it->first] = distTot;
                         Q.push( qEntry( (string &) it->first, distTot) );
                    }        
            }
       }
     //Return results
       return dist;       
}


int main(int argc, char *argv[]) {
     //Build test graph...
       Graph<string> gTest(true);
       gTest.addNode("Livermore");
       gTest.addNode("Dublin");
       gTest.addNode("Pleasanton");
       gTest.addNode("Hayward");
       gTest.addNode("SanFrancisco");
       gTest.addNode("Oakland");
       gTest.setDistance("Livermore", "Dublin", 11);
       gTest.setDistance("Livermore", "Pleasanton", 6);  
       gTest.setDistance("Dublin", "Hayward", 10);
       gTest.setDistance("Pleasanton", "Hayward", 16);
       gTest.setDistance("Oakland", "SanFrancisco", 12);
       gTest.setDistance("Hayward", "SanFrancisco", 27);
       gTest.setDistance("Hayward", "Oakland", 15);
       gTest.setDistance("Dublin", "Oakland", 23);
     //Compute and output shortest-path from Livermore
       map<string, unsigned int> *dPath;
       try { 
               dPath = dijkstra(gTest, "Livermore");
       } catch(runtime_error &e) {
               dPath = new map<string, unsigned int>();
               cout << "ERROR: Exception ('" << e.what() << "') occured!" << endl;
       }
       for(map<string, unsigned int>::iterator it = dPath->begin();
           it != dPath->end();
           ++it)
               if(it->first != "Livermore")
                    cout << "Livermore -> " << it->first << ": " << it->second << " miles." << endl;
     //Compare output to golden model
       map<string, unsigned int>::iterator it;
       assert( (it = dPath->find("Dublin")) != dPath->end());
       assert(it->second == 11);
       assert( (it = dPath->find("Hayward")) != dPath->end());
       assert(it->second == 21);
       assert( (it = dPath->find("Oakland")) != dPath->end());
       assert(it->second == 34);
       assert( (it = dPath->find("Pleasanton")) != dPath->end());
       assert(it->second == 6);
       assert( (it = dPath->find("SanFrancisco")) != dPath->end());
       assert(it->second == 46);
       assert( (it = dPath->find("Livermore")) != dPath->end());
       assert(it->second == 0);

     return 0;
}
