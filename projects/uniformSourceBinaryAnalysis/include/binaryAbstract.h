#ifndef BIN_ABSTRACT_CLASS
#define BIN_ABSTRACT_CLASS

#include <boost/graph/vector_as_graph.hpp>
#include <boost/graph/adjacency_list.hpp>

        //Abstract graph...
               typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, boost::property<boost::vertex_name_t, SgAsmBlock*> > FlowGraph;
               typedef boost::graph_traits<FlowGraph>::vertex_iterator FGVIter;
               typedef boost::graph_traits<FlowGraph>::vertex_descriptor FGVertex;

        //Abstract classes
          //Scheduler interface
                 template<class Policy> class SchInterface {
                    private:

                    public:
                         //Schedule gI as gO
                         virtual bool operator()(FlowGraph gI, FlowGraph &gO) = 0;
                 };
                 template<class Policy> class RegAllocInterface {
                    private:

                    public:
                         virtual bool operator()(FlowGraph gI, FlowGraph &gO, set<BA::RegisterRep> &availableReg) = 0;
                 };

#endif
