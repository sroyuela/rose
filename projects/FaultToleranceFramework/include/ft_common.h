#include <iostream>
#include <limits>
#include <map>
#include <vector>
#include <stdexcept>
#include <boost/regex.hpp>

#include "rose.h"

namespace FT {
    /** 
     * @namespace FT::Common
     * @brief Fault-tolerant support namespace. Contains utility functions used by the FTTransform & FTAnalysis classes
     * @author Jacob Lidman
     * 
     */
   namespace Common {
           //FT Exception
             class FTException : public runtime_error {
                    public:
                         FTException(string msg) : runtime_error(msg) {};
             };

           //Visitors
             /** 
             * @class FTVisitor
             * @brief Base/abstract class for visitor used by transformMulti function in FTOp class. 
             * Supports memory pool or pre/post order traversal depending on which constructor and traversel function (traverseMemoryPool, traverse) that is used.
             */
             struct FTVisitor : public ROSE_VisitTraversal, public AstPrePostProcessing {
                    private:
                         bool postOrderTraversal;
                         std::vector<SgNode *> targetNodes;
                         std::vector<SgNode *> removeNodes;

                         void visit(SgNode* node);
                         virtual void preOrderVisit(SgNode *node) {
                              if(postOrderTraversal == false)
                                   visit(node);
                         }
                         virtual void postOrderVisit(SgNode *node) {
                              if(postOrderTraversal == true)
                                   visit(node);
                         }
                    protected:
                         void addTarget(SgNode *node);
                         void addRemove(SgNode *node);               
                    public:
                         FTVisitor() {
                              this->postOrderTraversal = true;
                         }
                         FTVisitor(bool postOrder) {
                              this->postOrderTraversal = postOrder;
                         }
                         virtual ~FTVisitor() {}

                         std::vector<SgNode *> &getTargetNodes() {return targetNodes;}
                         std::vector<SgNode *> &getRemoveNodes() {return removeNodes;}

                         virtual bool targetNode(SgNode *node) = 0;
             };
   };
};
