
#include "BinaryControlFlow.h"
#include "BinaryLoader.h"
#include "SymbolicSemantics.h"
#include "YicesSolver.h"

#include "rose.h"

#include "TI.h"
#include "ft_common.h"

namespace FT {
   class RedundancyAnalysis {
     public:
           //Visitors
             /** 
             * @class RedundancyAnalysis
             * @brief Default implmentation of FTVisitor. Analyze the need for redundancy of each statement
             */
             struct FTAnalyzeVisitor : public Common::FTVisitor {
                    FTAnalyzeVisitor() {}
                    bool targetNode(SgNode *node);
             };

           //Constructor... 
             RedundancyAnalysis(SgProject *project = NULL) {
               this->project = project;

               if(this->project == NULL)
                    this->project = SageInterface::getProject();
             }

             ~RedundancyAnalysis() {}
           //Functions for analysing neccessity of fault tolerance
             /**
              * Analyze neccessity of redundant computations
              * @param inputNode AST input node.
              * @param closure Container for all side effects and results, needed in order for multiple statements (in a BB) to share a init. / unification stage.
              * @param globalScope Global scope of inputNode, overrides global scope given to class constructor.
              * @returns NULL if "inputNode" was added to closure or a transformed SgNode (possibly "inputNode" itself)
              **/
             SgNode *analyzeSingle(SgNode *inputNode, SymbolicSemantics::Policy *semPolicy = NULL);
             /**
              * Analyze all specified IR nodes (by visitor).
              * @param startNode Top-most AST input node. If equal to NULL, transformMulti will perform a MemoryPool traversal.
              * @param decisionFunctor Visitor functor. Decides which IR nodes that will be transformed.
              * @param globalScope Global scope of inputNode, overrides global scope given to class constructor.
              **/
             SgNode *analyzeMulti(SgNode *startNode = NULL);
     private:
             SgProject *project;
             SgGlobal *globalScope;

             struct BinAnalVist : public boost::default_dfs_visitor {
                    typedef boost::graph_traits<BinaryAnalysis::ControlFlow::Graph>::vertex_descriptor BVertex;
                    typedef boost::graph_traits<BinaryAnalysis::ControlFlow::Graph>::edge_descriptor BEdge;
                    RedundancyAnalysis &o;

                    BinAnalVist(RedundancyAnalysis &owner) : o(owner) {}
                    void discover_vertex(BVertex v, const BinaryAnalysis::ControlFlow::Graph &g) const {
                         o.analyzeSingle( get(boost::vertex_name, g, v) );
                    }
             };
   };
};
