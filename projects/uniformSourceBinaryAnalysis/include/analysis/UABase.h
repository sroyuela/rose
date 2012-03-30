#ifndef UABASE_HDR
#define UABASE_HDR

#include <analysis/binaryAnalysis.h>

namespace BA {

     class AnalysisBase : public std::unary_function<SgNode *, bool> {
          private:
               
          protected:
               virtual bool _analyzePre(CFG cfg) {return true;};          
               virtual bool _analyzePre(SgNode *node) {return true;};
               virtual bool _analyzePost(CFG cfg) {return true;}
               virtual bool _analyzePost(SgNode *node) {return true;};
               virtual void _print(ostream &o) = 0;
          public:
               //(De)Constructor
               AnalysisBase() {}
               virtual ~AnalysisBase() {}

               //Functions
               bool operator()(CFG cfg) {
                    if(!_analyzePre(cfg))
                         return true;
                    //Build web for each function and variable...
                      for(pair<CFGVIter, CFGVIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first)
                          if( !(*this)(get(boost::vertex_name, cfg, *vP.first)) )
                              return false;
                    _analyzePost(cfg);
                    return true;
               }
               bool operator()(SgNode *n) {
                    if(n == NULL)
                         throw BAException("Invalid entry node: NULL");
                    switch(n->variantT()) {
                     case V_SgProject: {
                       if(!_analyzePre(n))
                          return true;
                       SgProject *p = static_cast<SgProject *>(n);
                       //Query AST nodes for interpretations...
                         vector<SgNode*> interps = NodeQuery::querySubTree(p, V_SgAsmInterpretation);
                         if(interps.size() == 0)
                            throw BAException("Unable to compute CFG (SgAsmInterpretation not found in project!)");
                       //Perform CFG analysis...
                         CFG cfg = BinaryAnalysis::ControlFlow().build_cfg_from_ast<CFG>(interps.back());
                       //... and callgraph analysis...
                         BinaryAnalysis::FunctionCall cg_analyzer;
                         struct ExcludeLeftovers: public BinaryAnalysis::FunctionCall::VertexFilter {
                             bool operator()(BinaryAnalysis::FunctionCall *analyzer, SgAsmFunction *func) {
                                  return func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS);
                   	         }
                         } exclude_leftovers;
                         cg_analyzer.set_vertex_filter(&exclude_leftovers);
                         CG cg = cg_analyzer.build_cg_from_cfg<CG>(cfg);
                       //Build web for each function and variable...
                         for(pair<CG_VIter, CG_VIter> vP = vertices(cg); vP.first != vP.second; ++vP.first)
                             if( !(*this)(get(boost::vertex_name, cg, *vP.first)) )
                                 return false;
                       _analyzePost(n);
                      } return true;
                     case V_SgAsmFunction: {
                       if(!_analyzePre(n))
                          return true;
                       SgAsmFunction *f = static_cast<SgAsmFunction *>(n);
                       for(Rose_STL_Container<SgAsmStatement*>::iterator it = f->get_statementList().begin(); it != f->get_statementList().end(); ++it)
                           if( !(*this)( *it ) )
                               return false;
                       _analyzePost(n);
                      } return true;
                     case V_SgAsmBlock: {
                       if(!_analyzePre(n))
                          return true;
                       SgAsmBlock *bb = static_cast<SgAsmBlock *>(n);
                       for(Rose_STL_Container<SgAsmStatement*>::iterator it = bb->get_statementList().begin(); it != bb->get_statementList().end(); ++it)
                           if( !(*this)( *it ) )
                               return false;
                       _analyzePost(n);
                      } return true;
                     case V_SgAsmInstruction:
                     case V_SgAsmArmInstruction:
                     case V_SgAsmPowerpcInstruction:
                     case V_SgAsmx86Instruction:
                     case V_SgAsmStaticData:
                      _analyzePre(n);
                      _analyzePost(n);
                      return true;                  
                     default:
                        throw BAException("Unhandled type '" + n->class_name() + "'");
                    }
               }
               friend ostream &operator<<(ostream &o, AnalysisBase &r) {
                    r._print(o);
                    return o;
               }
     };

};

#endif

