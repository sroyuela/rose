#include <iostream>
#include <map>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <boost/regex.hpp>

#include "rose.h"
#include "OmpAttribute.h"

#include "TI.h"
#include "ft.h"

using namespace std;

void FT::Common::FTVisitor::visit(SgNode* node) {
     //Make sure node is OK
       ROSE_ASSERT(node != NULL);
     //Should node be enhanced...
       if(targetNode(node) == true)
          targetNodes.push_back(node);
}

void FT::Common::FTVisitor::addTarget(SgNode *node) {
     //Make sure a child of node hasn't been added
       std::vector<int> childNodes;
       int index = 0;
       for(std::vector<SgNode *>::iterator it = targetNodes.begin();
           it != targetNodes.end();
           ++it)
               if(TInterface::isParentOf(*it, node)) {
                    SgLocatedNode *ln;
                    if((ln = isSgLocatedNode(*it)) != NULL)
                         cout << ln->get_file_info()->get_filenameString() << ":" << ln->get_file_info()->get_line();
                    else
                         cout << "???:?";
                    cout << ") Ignoring node '" << (*it)->class_name() << "' (" << *it << ") as parent node '" 
                         << node->class_name() << "' (" << node << ") is scheduled for transformation." << endl;
                    childNodes.push_back(index);
               } else
                    index++;
     //Make sure node is not the child of another node
       for(std::vector<SgNode *>::iterator it = targetNodes.begin();
           it != targetNodes.end();
           ++it)
               if(TInterface::isParentOf(node, *it)) {
                    SgLocatedNode *ln;
                    if((ln = isSgLocatedNode(*it)) != NULL)
                         cout << ln->get_file_info()->get_filenameString() << ":" << ln->get_file_info()->get_line();
                    else
                         cout << "???:?";
                    cout << ") Ignoring node '" << node->class_name() << "' (" << node << ") as parent node '" 
                         << (*it)->class_name() << "' (" << *it << ") is scheduled for transformation." << endl;
                    return;
               }
     //Remove childs if any...
       for(std::vector<int>::iterator it = childNodes.begin();
           it != childNodes.end();
           ++it)
               targetNodes.erase( targetNodes.begin() + *it );
     //Add node
       targetNodes.push_back(node);
}
void FT::Common::FTVisitor::addRemove(SgNode *node) {
     //Make sure a child of node hasn't been added
       std::vector<int> childNodes;
       int index = 0;
       for(std::vector<SgNode *>::iterator it = removeNodes.begin();
           it != removeNodes.end();
           ++it)
               if(TInterface::isParentOf(*it, node))
                    childNodes.push_back(index);
               else
                    index++;
     //Make sure node is not the child of another node
       for(std::vector<SgNode *>::iterator it = removeNodes.begin();
           it != removeNodes.end();
           ++it)
               if(TInterface::isParentOf(node, *it))
                    return;
     //Remove childs if any...
       for(std::vector<int>::iterator it = childNodes.begin();
           it != childNodes.end();
           ++it)
               removeNodes.erase( removeNodes.begin() + *it );
     //Add node
       removeNodes.push_back(node);
} 

bool FT::Transform::FTPragmaVisitor::targetNode(SgNode *node) {
     //Handle differently depending on language...
     if(langHasC || langHasCxx) {
          //Is this our pragma?
            if(node->variantT() == V_SgPragmaDeclaration) {
               boost::regex commentMatcher("[[:space:]]*resiliency.*", boost::regex::icase);
               SgPragmaDeclaration *pragmaDecl = isSgPragmaDeclaration(node);
               SgPragma *pragma = pragmaDecl->get_pragma();
               //Make sure the pragma is correct..
                 const char *s = pragma->get_pragma().c_str();
                 if(!boost::regex_match(s, commentMatcher))
                    return false;              
               //Make sure there is a next statement...
                 SgStatement *stm = SageInterface::getNextStatement(pragmaDecl);
                 if(stm == NULL)
                    return false;
               //Print debug message if appropriate...
                 if(SgProject::get_verbose()>0)
                   cout << "Pragma: '" << pragma->get_pragma() << "' -> " << stm->class_name() << endl;
               //Remove pragma declaration from AST
                 addRemove(pragmaDecl);
               //Add node for transformation...
                 addTarget(stm);
            } 
     } 

     if(langHasFortran) {
          //Try to get comment
            SgStatement *stm;
            if((stm = isSgStatement(node)) == NULL)
               return false;
          //Extract PreProcInfo
            Rose_STL_Container< PreprocessingInfo* > *preproc = stm->getAttachedPreprocessingInfo();
            if(preproc == NULL)
               return false;
            boost::regex commentMatcherF90("[[:space:]]*!\\$[[:space:]]*resiliency.*", boost::regex::icase),
                         commentMatcherF("C\\$[[:space:]]*resiliency.*", boost::regex::icase);
            for(Rose_STL_Container< PreprocessingInfo* >::iterator it = preproc->begin();
                it != preproc->end();
                ++it)
                    //cout << "Comment: '" << (*it)->getString() << "'" << endl;
                    if( ((*it)->getTypeOfDirective() == PreprocessingInfo::FortranStyleComment) ||
                        ((*it)->getTypeOfDirective() == PreprocessingInfo::F90StyleComment) )
                         if(boost::regex_match((*it)->getString().c_str(), commentMatcherF90) ||
                            boost::regex_match((*it)->getString().c_str(), commentMatcherF)) {
                              SgStatement *nstm = SageInterface::getNextStatement(stm);
                              //Print debug message if appropriate...
                                if(SgProject::get_verbose()>0)
                                   cout << "Pragma: '" << (*it)->getString().substr(2) << "' -> " << nstm->class_name() << endl;
                              //Add node for transformation...
                                   addTarget(nstm);
                         }
     }

     return false;
}

SgExpression *FT::Transform::FaultHandling_Closure::generateComparisonExp(map<SgExpression *, set<SgExpression *> *> &ftVars) {
     stack<SgExpression *> operandStack[2];
     set<SgExpression *> *wVars;
     SgExpression *lhs, *rhs, *initalVar, *finalExp = NULL;
     SgAndOp *lastExp = NULL;

     //Create comp. exp for each variable
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = ftVars.begin();
           it != ftVars.end();
           ++it) {
                 //Setup initial value for this pair
                   initalVar = it->first;
                   wVars = it->second;
                   if(wVars->size() == 0)
                      continue;

                 //Put all SgVarRefExp (enteries in wVars) on stack...
                   for(set<SgExpression *>::iterator it = wVars->begin();
                       it != wVars->end();
                       ++it)
                            operandStack[0].push( *it );

                 //Group stack elements linearly and create SgEqualityOp of each pair...
                 //(ex. {1,2,3,4,5,...} -> {<1,2>,<2,3>,<3,4>,...}
                   if(operandStack[0].size() > 1) {
                      lhs = operandStack[0].top(); operandStack[0].pop();
                      while(operandStack[0].size() > 0) {
                            rhs = operandStack[0].top(); operandStack[0].pop();
                            operandStack[1].push( SageBuilder::buildBinaryExpression<SgEqualityOp>( lhs, rhs ) );
                            lhs = rhs;
                      }
                   } else {
                              operandStack[1].push( operandStack[0].top() ); 
                              operandStack[0].pop();
                   }
                 //Create a conjunction clause/chain with each ==
                   if(operandStack[1].size() > 1) {
                      if(lastExp == NULL) {
                         rhs = operandStack[1].top(); operandStack[1].pop();
                         finalExp = lastExp = SageBuilder::buildBinaryExpression<SgAndOp>(NULL, rhs);
                      }
                      while(operandStack[1].size() > 0) {
                            rhs = operandStack[1].top(); operandStack[1].pop();
                            lastExp->set_lhs_operand_i(SageBuilder::buildBinaryExpression<SgAndOp>(NULL, rhs));
                            lastExp->get_lhs_operand_i()->set_parent(lastExp);
                            lastExp = isSgAndOp( lastExp->get_lhs_operand_i() );
                      }
                   } else {
                           if(lastExp == NULL)
                              finalExp = lastExp = SageBuilder::buildBinaryExpression<SgAndOp>(NULL, operandStack[1].top() );
                           else {
                                 lastExp->set_lhs_operand_i( SageBuilder::buildBinaryExpression<SgAndOp>(NULL, operandStack[1].top()) );
                                 lastExp->get_lhs_operand_i()->set_parent(lastExp);
                                 lastExp = isSgAndOp( lastExp->get_lhs_operand_i() );
                           }
                           operandStack[1].pop();
                   }
                 }
     //Handle the end of the conj. chain
       if(lastExp->get_parent() != NULL) {
          SgAndOp *opAndRhs = isSgAndOp(lastExp->get_parent());
          opAndRhs->set_lhs_operand_i(lastExp->get_rhs_operand_i());
       } else
             finalExp = lastExp->get_rhs_operand_i();
       lastExp->set_rhs_operand_i(NULL);
       SageInterface::deleteAST(lastExp);
     //Return expression...
       return finalExp;
}

bool FT::Transform::FaultHandling_Closure::close() {
     //Build intra comparison
       SgStatement *intraHandler;
       if((redundancyIntraCore > 0) && (intraFtVars.size() > 0)) {
          SgExpression *intraCond = generateComparisonExp(this->intraFtVars);
          if(redundancyIntraOuter > 0) {
             //Create top comparison exp and add assign statement...
               map<SgExpression *, set<SgExpression *> *> outer;
               for(map<SgExpression *, set<SgExpression *> *>::iterator it = intraFtVars.begin(); it != intraFtVars.end(); ++it) {
                   set<SgExpression *> *s = new set<SgExpression *>();
                   unsigned int innerCounter = 0;
                   for(set<SgExpression *>::iterator it2 = it->second->begin(); innerCounter < redundancyIntraOuter; ++it2, innerCounter++)
                       s->insert(*it2);
                   outer[it->first] = s;
               }
             //Add declaration statements...
               for(vector<SgVariableDeclaration *>::iterator it = intraVarDecl.begin();
                   it != intraVarDecl.end();
                   ++it)
                         SageInterface::prependStatement(*it, getExecIntraOuterScope());
             //Create IF-stmt to separate inner and outer...
               SageInterface::appendStatement(
                              SageBuilder::buildIfStmt(
                                        SageBuilder::buildUnaryExpression<SgNotOp>(generateComparisonExp(outer)),
                                        fPIntra.getHandler(getExecIntraScope(), this->intraFtVars, intraCond, false, true), 
                                        NULL),
                              getExecIntraOuterScope());
               intraHandler = getExecIntraOuterScope();
          } else  {
               for(vector<SgVariableDeclaration *>::iterator it = intraVarDecl.begin();
                   it != intraVarDecl.end();
                   ++it)
                        SageInterface::prependStatement(*it, getExecIntraScope());
               intraHandler = fPIntra.getHandler(getExecIntraScope(), this->intraFtVars, intraCond, false, false);
          }
       } else if(redundancyIntraOuter > 0)
             intraHandler = getExecIntraOuterScope();
       else
             intraHandler = getExecIntraScope();

     //Build inter comparison
       if((redundancyInterCore > 0) && (interFtVars.size() > 0)) {
          SgBasicBlock *bbInter = SageBuilder::buildBasicBlock();
       //Create exec. section
         //Declare inter counter variable...
           SgVariableDeclaration *itCnt = SageBuilder::buildVariableDeclaration( 
               SgName("ii"), 
               SageBuilder::buildIntType(), 
               NULL,
               getDeclScope());
           SgVariableDeclaration *correctCnt = SageBuilder::buildVariableDeclaration( 
               SgName("correctCnt"), 
               SageBuilder::buildIntType(), 
               SageBuilder::buildAssignInitializer(
                    SageBuilder::buildIntVal(0),
                    SageBuilder::buildIntType()),
               getDeclScope());
         //Add declarations to Decl. BB
           SageInterface::appendStatement(itCnt, getDeclScope());
           SageInterface::appendStatement(correctCnt, getDeclScope());
           for(vector<SgVariableDeclaration *>::iterator it = interVarDecl.begin();
               it != interVarDecl.end();
               ++it)
                  SageInterface::appendStatement(*it, getDeclScope());
         //Create for loop
           SgForStatement *forStmExec = SageBuilder::buildForStatement(
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgAssignOp>(
                         SageBuilder::buildVarRefExp(itCnt),
                         SageBuilder::buildIntVal(0))),
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgLessThanOp>(
                         SageBuilder::buildVarRefExp(itCnt),
                         SageBuilder::buildIntVal(redundancyInterCore+1))),
               SageBuilder::buildBinaryExpression<SgPlusAssignOp>(
                    SageBuilder::buildVarRefExp(itCnt),
                    SageBuilder::buildIntVal(1)),
               intraHandler);
           SageInterface::appendStatement(forStmExec, bbInter);
       //Create comp. section
         //Create for loop
           SgForStatement *forStmCmp = SageBuilder::buildForStatement(
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgAssignOp>(
                         SageBuilder::buildVarRefExp(itCnt),
                         SageBuilder::buildAssignInitializer(SageBuilder::buildIntVal(0), SageBuilder::buildIntType()) )),
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgLessThanOp>(
                         SageBuilder::buildVarRefExp(itCnt),
                         SageBuilder::buildIntVal(redundancyInterCore+1-1))),
               SageBuilder::buildBinaryExpression<SgPlusAssignOp>(
                    SageBuilder::buildVarRefExp(itCnt),
                    SageBuilder::buildIntVal(1)),
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgPlusAssignOp>(
                        SageBuilder::buildVarRefExp(correctCnt),
                        generateComparisonExp(this->interCompExps) )));
               SageInterface::appendStatement(forStmCmp, bbInter);
       //Finalize inter
         //Use bbDecl as top level...
           SageInterface::appendStatement(
               fPInter.getHandler(bbInter,
                                  interFtVars,
                                  SageBuilder::buildBinaryExpression<SgEqualityOp>(
                                        SageBuilder::buildVarRefExp(correctCnt),
                                        SageBuilder::buildIntVal(redundancyInterCore+1-1)),
                                  false, false),
               bbDecl);
         //Make forStmExec parallel
           OmpSupport::OmpAttribute *omp_attribute = OmpSupport::buildOmpAttribute(OmpSupport::e_unknown, NULL, false);
           omp_attribute->setOmpDirectiveType(OmpSupport::e_parallel_for);
           OmpSupport::addOmpAttribute(omp_attribute, forStmExec);                                                               
           OmpSupport::generatePragmaFromOmpAttribute(forStmExec);
         //Make forStmCmp parallel
           omp_attribute = OmpSupport::buildOmpAttribute(OmpSupport::e_parallel_for, NULL, false);
           SgInitializedName *iname = SageInterface::convertRefToInitializedName(SageBuilder::buildVarRefExp(SgName("correctCnt"), bbDecl));
           omp_attribute->addVariable(OmpSupport::e_reduction_plus, iname->get_name().getString(), iname);
           OmpSupport::addOmpAttribute(omp_attribute, forStmCmp);                                                               
           OmpSupport::generatePragmaFromOmpAttribute(forStmCmp);
       //Make sure openmp is included...
         if((globalScope != NULL) && (globalScope->lookup_function_symbol( SgName("omp_set_num_threads") ) == NULL))
            SageInterface::insertHeader("omp.h", PreprocessingInfo::after, true, globalScope);
       } else
             SageInterface::appendStatement(intraHandler, bbDecl);  

     //Append exit statement if encountered...
       if(hasExitSmt())
          SageInterface::appendStatement(getExitStm(), bbDecl);
                              
     //Set results
       *bbResult = bbDecl;

     //Cleanup
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = interFtVars.begin();
           it != interFtVars.end();
           ++it)
                delete it->second;
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = intraFtVars.begin();
           it != intraFtVars.end();
           ++it)
                delete it->second;

     return true;
}

bool FT::Transform::FaultHandling_Closure::addInter(map<SgExpression *, set<SgExpression *> *> &ftVarsMap, map<SgExpression *, SgExpression *> &interCompare) {
     //No new instructions after exit...
       if(hasExitSmt())
          return false;
     //Add to large map and look for duplicates...
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = ftVarsMap.begin(), it2;
           it != ftVarsMap.end();
           ++it) {
                  for(map<SgExpression *, set<SgExpression *> *>::iterator it2 = interFtVars.begin();
                      it2 != interFtVars.end();
                      ++it2) 
                            if( TInterface::equals(it->first, it2->first, true) ) {
                                //Delete allocated structures...
                                  delete it2->second;
                                  delete interCompExps.find(it2->first)->second;
                                //Delete entries...
                                  interCompExps.erase( interCompExps.find(it2->first) );
                                  interFtVars.erase(it2);
                               break;
                            }
                  set<SgExpression *> *interCompSet = new set<SgExpression *>();
                  interCompSet->insert( interCompare.find(it->first)->second );
                  interCompExps[it->first] = interCompSet; 
                  interFtVars[it->first] = it->second;
       }

     return true;
}

SgVariableDeclaration *FT::Transform::FaultHandling_Closure::getInterVar(string varName, SgType *baseType, SgExpression *initExp) {
     //Has variable already been declared...
       for(vector<SgVariableDeclaration *>::iterator it = interVarDecl.begin();
           it != interVarDecl.end();
           ++it)
                if( SageInterface::getFirstInitializedName( *it )->get_name().getString() == varName) 
                    return *it;
     //No new instructions after exit...
       if(hasExitSmt())
          return NULL;
     //Variable was not found, declare it...
       SgVariableDeclaration *varDecl = SageBuilder::buildVariableDeclaration(
          SgName(varName),
          SageBuilder::buildArrayType(baseType, SageBuilder::buildIntVal(redundancyInterCore+1)),
          BInterface::buildArrayInitializer(initExp, baseType, redundancyInterCore+1),
          getDeclScope());
       interVarDecl.push_back( varDecl );

   return varDecl;                                   
}

bool FT::Transform::FaultHandling_Closure::addIntra(map<SgExpression *, set<SgExpression *> *> *ftVarsMap, vector<SgStatement *> &intraStm) {
     //No new instructions after exit...
       if(hasExitSmt())
          return false;
     //Add to large map and look for duplicates...
       unsigned int innerCounter = 0;
       for(vector<SgStatement *>::iterator it = intraStm.begin();
           it != intraStm.end();
           ++it, innerCounter++)
                if(innerCounter >= redundancyIntraOuter)
                    SageInterface::appendStatement(*it, bbExecIntra);
                else
                    SageInterface::appendStatement(*it, bbExecIntraOuter);
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = ftVarsMap->begin(), it2;
           it != ftVarsMap->end();
           ++it) {
                  for(map<SgExpression *, set<SgExpression *> *>::iterator it2 = intraFtVars.begin();
                      it2 != intraFtVars.end();
                      ++it2)
                            if( TInterface::equals(it->first, it2->first, true) ) {
                                delete it2->second;
                                intraFtVars.erase(it2);
                                break;
                            }
                  intraFtVars[it->first] = it->second;

                  if(redundancyIntraOuter > 0)
                      SageInterface::appendStatement(
                         SageBuilder::buildExprStatement(
                              SageBuilder::buildBinaryExpression<SgAssignOp>( it->first, *(it->second->begin()) )),
                              bbExecIntraOuter);
       }       
     return true;
}

SgVariableDeclaration *FT::Transform::FaultHandling_Closure::getIntraVar(string varName, SgType *baseType, SgExpression *initExp, bool declareIfNotFound) {
     //Has variable already been declared...
       for(vector<SgVariableDeclaration *>::iterator it = intraVarDecl.begin();
           it != intraVarDecl.end();
           ++it)
                if( SageInterface::getFirstInitializedName( *it )->get_name().getString() == varName) 
                    return *it;
     //Check parents...
       SgVariableDeclaration *varDecl = (this->parent_closure != NULL ? this->parent_closure->getIntraVar(varName, baseType, initExp, false) : NULL);
       if(varDecl != NULL)
          return varDecl;          
       if(!declareIfNotFound)
          return NULL;
     //No new instructions after exit...
       if(hasExitSmt())
          return NULL;         
     //Variable was not found, declare it...
       varDecl = SageBuilder::buildVariableDeclaration(
          SgName(varName),
          SageBuilder::buildArrayType(baseType, SageBuilder::buildIntVal(redundancyIntraCore+1)),
               BInterface::buildArrayInitializer(initExp, baseType, redundancyIntraCore+1),
               getDeclScope());
       intraVarDecl.push_back( varDecl );

     return varDecl;                                   
}

bool FT::Transform::FaultHandling_Closure::addIntraStatement(SgStatement *stm) {
     //No new instructions after exit...
       if(hasExitSmt())
          return false;
     //Add statement...
       if(TInterface::IsExitBBStatement(stm))
          setExitStm(stm);
       else {
          SgStatement *stmNew = SageInterface::deepCopy<SgStatement>(stm);
          if(!TInterface::hasValueAttribute(stm, FT_TRANFORMATION_NODE_ATTR))
               SageInterface::attachComment(stmNew, "Unmodified statement: " + stm->unparseToString());
          if(redundancyIntraOuter > 0)
               SageInterface::appendStatement(stmNew, bbExecIntraOuter);
          else
               SageInterface::appendStatement(stmNew, bbExecIntra);
       }

     return true;
}
     
//Functor functions for adding fault tolerance

//Initialize closure...
#define _FT_CLOSURE_BEGIN() \
            {if(closure == NULL) \
               bClosure = new FaultHandling_Closure(globalScope, closure, &bbTopLevel, redundancyInter, fPInter, redundancyIntra, redundancyIntraOuter, fPIntra);}
#define _FT_CLOSURE_END() \
            {if(closure == NULL) { \
               delete bClosure; \
               return bbTopLevel; \
            } else \
                  return NULL;}
     
SgNode *FT::Transform::transformSingle(SgNode *inputNode, FaultHandling_Closure *closure, SgGlobal *globalScope) {
     //Initialization
       FaultHandling_Closure *bClosure = closure;  
       SgBasicBlock *bbTopLevel = NULL;     
       SgNode *nodePtrA, *nodePtrB, *nodePtrC, *nodePtrD;
       SgStatementPtrList *stmBodyList;
       SgStatement *stmBody;
       SgScopeStatement *scope;
       if(inputNode == NULL)
          return NULL;
       performInitialization();

     //Mark node as having been transformed...
       ROSE_ASSERT(TInterface::hasValueAttribute(inputNode, FT_TRANFORMATION_NODE_ATTR) == false);
       inputNode->addNewAttribute(FT_TRANFORMATION_NODE_ATTR, new AstTextAttribute("TRUE"));

     //Recursivly handle statement(s)
       if(TInterface::IsLoop(inputNode, &nodePtrA, &nodePtrB, &nodePtrC, &nodePtrD)) {
          TInterface::SetLoop(inputNode, nodePtrA, nodePtrB, nodePtrC, transformSingle(nodePtrD, NULL, globalScope));
          return inputNode;
       } else if(TInterface::IsIf(inputNode, &nodePtrA, &nodePtrB, &nodePtrC)) {
          TInterface::SetIf(inputNode, nodePtrA, transformSingle(nodePtrB, NULL, globalScope), transformSingle(nodePtrC, NULL, globalScope));                                  
          return inputNode;
       } else if(TInterface::IsDeclaration(inputNode)) {
          return inputNode;
       } else if(TInterface::IsBlock(inputNode, &stmBody, &stmBodyList, &scope)) {
          if(inputNode->variantT() == V_SgSwitchStatement)
              bClosure = NULL;                
          else
               _FT_CLOSURE_BEGIN()

          //Handle stm list (if any)...
            if(stmBodyList != NULL)
               for(Rose_STL_Container<SgStatement*>::iterator it = stmBodyList->begin();
                   it != stmBodyList->end();
                   ++it) {
                          SgStatement *stm = isSgStatement( transformSingle(*it, bClosure, globalScope) );
                          if( stm != NULL ) {
                              if(bClosure == NULL && stm != *it)
                                   SageInterface::replaceStatement(*it, stm);
                              else
                                   bClosure->addIntraStatement(*it);
                          }
               }
          //... handle statement (if any).
            if( stmBody != NULL ) {
               SgStatement *stm = isSgStatement( transformSingle(stmBody, bClosure, globalScope) );
               if( stm != NULL ) {
                 if(bClosure == NULL && stm != stmBody)
                    SageInterface::replaceStatement(stmBody, stm);
                 else
                     bClosure->addIntraStatement(stmBody);
               }
            }
          //Special handle for the switch stm...
            if(bClosure == NULL)
               return inputNode;
            else 
               _FT_CLOSURE_END()          
       } else if(TInterface::IsStatement(inputNode)) {
          _FT_CLOSURE_BEGIN()              
          //Initialization
            SgStatement *stm = isSgStatement(inputNode);
            if(!applyFT(stm, redundancyInter, redundancyIntra, bClosure, globalScope)) {
               bClosure->addIntraStatement(stm);
            }
            _FT_CLOSURE_END()
       } else if(isSgExpression(inputNode) != NULL)
               return inputNode;
       else {
             stringstream ss;
             ss << "Unhandled node '" << inputNode->class_name() << "'.";
             throw FT::Common::FTException(ss.str());
       }                              
}

SgNode *FT::Transform::transformMulti(SgNode *inputNode, FT::Common::FTVisitor *decisionFunctor, SgGlobal *globalScope) {
     //Make sure initialization has been performed.
       performInitialization();
     //Determine global scope...
       SgGlobal *gScope = globalScope;
       if(isSgStatement(inputNode) != NULL) 
          gScope = (gScope == NULL ?
                    (this->globalScope == NULL ? SageInterface::getGlobalScope(static_cast<SgStatement *>(inputNode)) : this->globalScope) :
                    gScope);
     //Create default collector...
       if(decisionFunctor == NULL)
          decisionFunctor = new FTPragmaVisitor();
     //Collect nodes...
       if(inputNode == NULL)
          decisionFunctor->traverseMemoryPool();
       else
          decisionFunctor->traverse(inputNode);
     //Transform selected nodes...
       SgNode *outputNode = inputNode, *tmpNode;
       for(std::vector<SgNode *>::iterator it = decisionFunctor->getTargetNodes().begin();
           it != decisionFunctor->getTargetNodes().end();
           ++it) {
                    //Transform node
                      tmpNode = transformSingle( *it, NULL, gScope );
                      if(*it == inputNode)
                         outputNode = tmpNode;
                    //Replace stm...
                      ROSE_ASSERT(tmpNode != NULL); /*node couldn't have been added to closure
                                                      since we didn't create one.*/                         
                      if(*it != tmpNode) {
                         SgStatement *oldStm = isSgStatement(*it), *newStm = isSgStatement(tmpNode);
                         ROSE_ASSERT(oldStm != NULL && newStm != NULL);
                         SageInterface::insertStatement(oldStm, newStm);
                         SageInterface::removeStatement(oldStm);
                      }
       }
     //Remove selected nodes...
       for(std::vector<SgNode *>::iterator it = decisionFunctor->getRemoveNodes().begin();
           it != decisionFunctor->getRemoveNodes().end();
           ++it)
               if(isSgStatement( *it ) != NULL)
                    SageInterface::removeStatement( isSgStatement( *it ) );
               else
                    SageInterface::deleteAST( *it );
     return outputNode;
}

//Misc. functions
void FT::Transform::performInitialization() {
     //This should only be performed once.
       if(this->initPerformed)
          return;
     //Global init for inter-core computations...
       if(this->redundancyInter > 0)
          if((this->globalScope == NULL) || (this->project == NULL)) {
               stringstream ss;
               ss << "Global scope ('" << this->globalScope << "') or project ('" << this->project << "') was found invalid.";
               throw new FT::Common::FTException(ss.str());
          } else {
               //Get func symbol for "omp_set_num_threads"...
                  SgFunctionSymbol *fSym = this->globalScope->lookup_function_symbol( SgName("omp_set_num_threads") );
                  if(fSym == NULL) {
                         ROSE_ASSERT( TInterface::loadHeaderFile( this->globalScope, "omp.h", true) == true);
                         SageInterface::insertHeader("omp.h", PreprocessingInfo::after, true, this->globalScope);
                         fSym = this->globalScope->lookup_function_symbol( SgName("omp_set_num_threads") );
                  }
               //Get basic block for main function
                 SgFunctionDeclaration *fDecl = SageInterface::findMain(this->project);
                 if(fDecl == NULL)
                    throw new FT::Common::FTException("Couldn't find function declaration for 'main'.");
                 SgFunctionDefinition *fDef = fDecl->get_definition();
                 if(fDef == NULL)
                    throw new FT::Common::FTException("Invalid definition for function 'main'.");
                  SageInterface::prependStatement(
                    SageBuilder::buildExprStatement(
                         SageBuilder::buildFunctionCallExp(
                              fSym,
                              SageBuilder::buildExprListExp(SageBuilder::buildIntVal(this->redundancyInter+1)) )),
                    fDef->get_body());
          }
     initPerformed = true;
}

//Functions to apply fault tolerance
bool FT::Transform::applyIntraFT(map<SgExpression *, SgExpression *> &sideEffects, 
                        set<SgExpression *> &requireInit,
                        unsigned int redundancyIntraCore, 
                        FaultHandling_Closure *closure, SgGlobal *globalScope) {
     //Initialization
       ROSE_ASSERT(closure != NULL);

       set<SgExpression *> *createdVarsIntra;
       map<SgExpression *, set<SgExpression *> *> *ftVarsMap = new map<SgExpression *, set<SgExpression *> *>();
       vector<SgStatement *> intraStm;
     //Create exec. and comp. parts for each sideeffect
       for(map<SgExpression *, SgExpression *>::iterator it = sideEffects.begin();
           it != sideEffects.end();
           ++it) {
                  SgExpression *ref = it->first, *sideEffect = it->second;

                  SgInitializedName *initName = SageInterface::convertRefToInitializedName( ref );
                  SgType *baseType = sideEffect->get_type(); //TInterface::getPrimitiveType( initName->get_type() );

                  string redunVarNameIntra = initName->get_name().getString() + "_intra";
                  SgVariableDeclaration *intraResults = closure->getIntraVar(redunVarNameIntra, baseType, ref);

                  //Add N redundant computations
                    createdVarsIntra = new set<SgExpression *>();
                    for(unsigned int i = 0; i < redundancyIntraCore+1; i++) {
                        SgPntrArrRefExp *intraRef =
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(intraResults),
                                   SageBuilder::buildIntVal(i));
                        createdVarsIntra->insert( intraRef );

                        SgExpression *expCopy = SageInterface::copyExpression(sideEffect);
                        TInterface::changeAllRefs(expCopy, ref, intraRef, false);
                        SgStatement *stmNew = SageBuilder::buildExprStatement(expCopy);
                        stmNew->set_parent( closure->getDeclScope() );
                        intraStm.push_back(stmNew);
                        if(createdVarsIntra->size() == 1)
                           SageInterface::attachComment(stmNew, "Original statement: " + sideEffect->unparseToString());
                    }
                  //Add assignment
                    //Fast-forward random steps...
                      set<SgExpression *>::iterator it2 = createdVarsIntra->begin();
                      for(int nr = rand() % createdVarsIntra->size(); nr > 0; nr--) 
                          ++it2;
                    //Build assignment...
                      SgStatement *stmAssign = SageBuilder::buildExprStatement(
                              SageBuilder::buildBinaryExpression<SgAssignOp>( ref, *it2 ));
                      stmAssign->set_parent( closure->getDeclScope() );
                      intraStm.push_back(stmAssign);
                  //Add results for later unification
                    ftVarsMap->insert( pair<SgExpression *, set<SgExpression *> *>(ref, createdVarsIntra) );
       }
     //Add intra to closure
       closure->addIntra(ftVarsMap, intraStm);
     //Exit
       return true;
}

bool FT::Transform::applyInterFT(map<SgExpression *, SgExpression *> &sideEffects, 
                        set<SgExpression *> &requireInit,
                        unsigned int redundancyInterCore,
                        unsigned int redundancyIntraCore,
                        FaultHandling_Closure *closure, SgGlobal *globalScope) {
     ROSE_ASSERT(closure != NULL);
     //Create exec. section
       //Prepare...
         map<SgExpression *, SgExpression *> sideEffectsMap, interCompare;
         set<SgExpression *> *createdVarsInter;
         map<SgExpression *, set<SgExpression *> *> ftVarsMap;

       for(map<SgExpression *, SgExpression *>::iterator it = sideEffects.begin();
           it != sideEffects.end();
           ++it) {
                  //Initialization
                    SgExpression *expRef = it->first, *sideEffect = it->second;
                    SgInitializedName *initName = SageInterface::convertRefToInitializedName( expRef );
                    SgType *baseType = sideEffect->get_type(); //TInterface::getPrimitiveType( initName->get_type() );
                  //Create declarations
                    string redunVarNameInter = initName->get_name().getString() + "_inter";
                    SgVariableDeclaration *interResultsDecl = closure->getInterVar(redunVarNameInter, baseType, expRef);
                    SgPntrArrRefExp *interResRef = SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                         SageBuilder::buildVarRefExp(interResultsDecl),
                         SageBuilder::buildVarRefExp("ii", closure->getDeclScope()) );
                  //SageInterface::prependStatement(interResultsDecl, closure->getDeclScope());
                    //Create comparisons
                      interCompare[expRef] = SageBuilder::buildBinaryExpression<SgEqualityOp>(
                              interResRef,
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(interResultsDecl),
                                   SageBuilder::buildBinaryExpression<SgAddOp>(
                                        SageBuilder::buildVarRefExp("ii", closure->getDeclScope()),
                                        SageBuilder::buildIntVal(1) )));
                    //Add to list of side effects for intra
                      SgExpression *expCopy = SageInterface::copyExpression( sideEffect );
                      TInterface::changeAllRefs(expCopy, expRef, interResRef, false);
                      sideEffectsMap[interResRef] = expCopy;
                      createdVarsInter = new set<SgExpression *>();
                      for(unsigned int i = 0; i < redundancyInterCore+1; i++)
                          createdVarsInter->insert(SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                      SageBuilder::buildVarRefExp(interResultsDecl),
                      SageBuilder::buildIntVal(i)));

                      ftVarsMap[expRef] = createdVarsInter;
       }

     //Handle locally if no intra redundancy is requested
       if(redundancyIntraCore > 0)
          applyIntraFT(sideEffectsMap, requireInit, redundancyIntraCore, closure, globalScope);
       else {
             SgStatement *stmNew = NULL, *firstStm = NULL;
             for(map<SgExpression *, SgExpression *>::iterator it = sideEffectsMap.begin();
                 it != sideEffectsMap.end();
                 ++it) {
                        stmNew = SageBuilder::buildExprStatement(it->second);
                        SageInterface::appendStatement(stmNew , closure->getExecIntraScope());
                        if(firstStm == NULL) {
                           firstStm = stmNew;
                           SageInterface::attachComment(stmNew, "Original statement: " + it->second->unparseToString());
                        }
             }
       }

     //Return results
       closure->addInter(ftVarsMap, interCompare);

     return true;
}

bool FT::Transform::applyFT(SgStatement *stm, unsigned int redundancyInterCore, unsigned int redundancyIntraCore, FaultHandling_Closure *closure, SgGlobal *globalScope) {
     //Return original stm if no redundancy should be introduced...
       if(redundancyInterCore+redundancyIntraCore == 0)
          return stm;
     //Initialize and check input
       vector< SgNode * > rVars, wVars;
       if(stm == NULL || closure == NULL)
          return false;
     //Collect R/W list...
       if(!SageInterface::collectReadWriteRefs(stm, rVars, wVars)) {
          #ifdef FT_POST_ERROR_MSG_AS_SRC_COMMENT
               SageInterface::attachComment(stm, "Unmodified statement: " + stm->unparseToString() + " (Couldn't decide side-effects)");
          #endif
          return false;
       }
       if(wVars.size() != 1) {
          #ifdef FT_POST_ERROR_MSG_AS_SRC_COMMENT
               string str("Invalid number of write references {");
               for(unsigned int i = 0; i < wVars.size(); i++) {
                    str.append("'" + wVars[i]->unparseToString() + "'");
                    if(i < wVars.size()-1)
                         str.append(", ");
               }                    
               SageInterface::attachComment(stm, "Unmodified statement: " + stm->unparseToString() + " (" + str + "})");
          #endif
          return false;
       }

       map<SgExpression *, SgExpression *> sideEffects;
       SgExpression *exp;
       /*This wouldn't be a problem if the side-effect analysis returned a pair <e,s> for
         each side-effect. Where 'e' is the destination of the side-effect and 's' is the side-effect
         itself */
       switch(stm->variantT()) {
        case V_SgExprStatement: exp = static_cast<SgExprStatement *>(stm)->get_expression();   break;
        default:
               #ifdef FT_POST_ERROR_MSG_AS_SRC_COMMENT
                    SageInterface::attachComment(stm, "(Unhandled type '" + stm->class_name() + "')");
               #endif
               return false;
       }
       for(vector< SgNode * >::iterator it = wVars.begin();
           it != wVars.end();
           ++it) {
                  SgExpression *ref = isSgExpression( *it );
                  ROSE_ASSERT(ref != NULL);
                  sideEffects[ ref ] = exp;
       }
       set<SgExpression *> requireInit;
       for(vector< SgNode * >::iterator it = rVars.begin();
           it != rVars.end();
           ++it) {
               SgExpression *ref = isSgExpression( *it );
               ROSE_ASSERT(ref != NULL);
               if(sideEffects.find(ref) != sideEffects.end())
                  requireInit.insert(ref);
       }
     //Apply redundancy
       if(redundancyInterCore > 0)
          return applyInterFT(sideEffects, requireInit, redundancyInterCore, redundancyIntraCore, closure, globalScope);
       else
           return applyIntraFT(sideEffects, requireInit, redundancyIntraCore, closure, globalScope);
}
