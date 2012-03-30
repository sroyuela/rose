#include <iostream>
#include <map>
#include <vector>

#include "rose.h"
#include "OmpAttribute.h"

#include "TI.h"
#include "Transform/ft_transform.h"

//Fault handling policies
SgStatement *FT::Transform::FailurePolicy::createBasicIf(SgStatement *body, 
                           SgStatement *faultHandler,
                           map<SgExpression *, set<SgExpression *> *> &ftVars, 
                           SgExpression *condSuccess,
                           SgStatement *elseCase,
                           bool alwaysFail) {
     //Initialization
       SgBasicBlock *sgBB = SageBuilder::buildBasicBlock();
               
       if(body != NULL) {
          if(body->variantT() == V_SgBasicBlock)
             SageInterface::appendStatementList(static_cast<SgBasicBlock *>(body)->get_statements(), sgBB);
          else
              SageInterface::appendStatement(body, sgBB);
       }
     //Add if stm
       if(alwaysFail)
          SageInterface::appendStatement(faultHandler, sgBB);
       else {
            SgIfStmt *ifstm = SageBuilder::buildIfStmt(
                                  SageBuilder::buildExprStatement(
                                      SageBuilder::buildUnaryExpression<SgNotOp>(condSuccess)),
                                  faultHandler,
                                  elseCase);
            SageInterface::appendStatement(ifstm, sgBB);
       }

       return sgBB;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// N-order fault handlers ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
 
//Fault policy "Final wish" - Invokes a given statement or error before (optionally) calling a (N-1)-level handler
SgStatement *FT::Transform::FailurePolicy_FinalWish::getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail) {
     SgBasicBlock *bb = SageBuilder::buildBasicBlock();

     //Inject statement
       SageInterface::appendStatement(stm, bb);
     //Perform second level handler...
       if(sLv != NULL)
          SageInterface::appendStatement(sLv->getHandler(body, ftVars, condSuccess, true, alwaysFail), bb);

     //Is someone else taking care of the cleanup?
       if(cascaded)
          return bb;
       else
           return createBasicIf(body, bb, ftVars, condSuccess, (alwaysExecStm ? stm : NULL), alwaysFail);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// 2-order fault handlers ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//Fault policy "Second chance" - Perform the computation upto N times while fault occurs, before calling second level handler
SgStatement *FT::Transform::FailurePolicy_SecondChance::getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail) {
     ROSE_ASSERT(body != NULL);
     ROSE_ASSERT(alwaysFail);
     SgBasicBlock *bb = SageBuilder::buildBasicBlock(), 
                  *bbContent = SageBuilder::buildBasicBlock();
     //Create for loop...
       SgForStatement *forStmExec = SageBuilder::buildForStatement(
          SageBuilder::buildVariableDeclaration(
               SgName("rI"),
               SageBuilder::buildIntType(),
               SageBuilder::buildAssignInitializer(SageBuilder::buildIntVal(0), SageBuilder::buildIntType()),
               bb),
          SageBuilder::buildExprStatement(SageBuilder::buildBoolValExp(true)),
               SageBuilder::buildBinaryExpression<SgPlusAssignOp>(
                    SageBuilder::buildVarRefExp(SgName("rI"), bb),
                    SageBuilder::buildIntVal(1)),
               bbContent);
          SageInterface::appendStatement(forStmExec, bb);
     //Create condition checker...
       if(body->variantT() == V_SgBasicBlock)
          SageInterface::appendStatementList(static_cast<SgBasicBlock *>(body)->get_statements(), bbContent);
       else
           SageInterface::appendStatement(body, bbContent);
       SageInterface::appendStatement(
          SageBuilder::buildIfStmt(
               SageBuilder::buildExprStatement(condSuccess),
               SageBuilder::buildBreakStmt(),
               SageBuilder::buildIfStmt(
                    SageBuilder::buildExprStatement(
                         SageBuilder::buildUnaryExpression<SgNotOp>(
                              SageBuilder::buildBinaryExpression<SgLessThanOp>(
                                   SageBuilder::buildVarRefExp(SgName("rI"), bb),
                                   SageBuilder::buildIntVal(N)))),
                    SageBuilder::buildBasicBlock(
                         sFP.getHandler(NULL, ftVars, SageBuilder::buildBoolValExp(true), true, alwaysFail),
                         SageBuilder::buildBreakStmt()),
                    NULL)),
               bbContent);

     return bb;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// 1-order fault handlers ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

//Fault policy "Die on error" - Print a messages that error has occured and call assertion to die.
SgStatement *FT::Transform::FailurePolicy_DieOnError::getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail) {
     SgBasicBlock *bb = SageBuilder::buildBasicBlock();
     ROSE_ASSERT(globalScope != NULL);
     //Create call to printf to print status message...
       SgExprListExp* paramPrintf = SageBuilder::buildExprListExp(
               SageBuilder::buildStringVal(
                    "Result is not consistent across executions, transient failure occurred!\\n"));
       SgFunctionSymbol *fSym = globalScope->lookup_function_symbol( SgName("printf") );
       if(fSym == NULL) {
          ROSE_ASSERT( TInterface::loadHeaderFile( this->globalScope, "stdio.h", true) == true);
          //Add assert function to current source...
            SageInterface::insertHeader("stdio.h", PreprocessingInfo::after, true, globalScope);
          fSym = globalScope->lookup_function_symbol( SgName("printf") );
          ROSE_ASSERT(fSym != NULL);
       }
       SgExprStatement* printfCall = SageBuilder::buildFunctionCallStmt("printf", SageBuilder::buildVoidType(), paramPrintf, globalScope);
       SageInterface::appendStatement(printfCall, bb);
     //Kill the application using assert (that way we know where in the program the error occured)
       fSym = globalScope->lookup_function_symbol( SgName("__assert") );
       if(fSym == NULL) {
          //Just include file and use macro...
          //Add assert function to current source...
            SageInterface::insertHeader("assert.h", PreprocessingInfo::after, true, globalScope);
       }
       SgExprStatement *stmAssert = SageBuilder::buildExprStatement(
          SageBuilder::buildFunctionCallExp(
               SgName("assert"),
               SageBuilder::buildVoidType(),
               SageBuilder::buildExprListExp(
                    SageBuilder::buildBoolValExp(false)),
               globalScope));
       SageInterface::appendStatement(stmAssert, bb);

     //Is someone else taking care of the cleanup?
       if(cascaded)
          return bb;
       else
          return createBasicIf(body, bb, ftVars, condSuccess, NULL, alwaysFail);
}

//Fault policy "Adjucator" - Decide which result to choose from
SgStatement *FT::Transform::FailurePolicy_Adjudicator::getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail) {
     SgBasicBlock *bb = SageBuilder::buildBasicBlock();

     if(this->adj == NULL)
          throw FT::Common::FTException("No adjudicator set");

     //Create voting for each pair
       for(map<SgExpression *, set<SgExpression *> *>::iterator it = ftVars.begin();
           it != ftVars.end();
           ++it) {   
                    //Initialization...
                      set<SgExpression *> *updatedValues = it->second;
                      SgExpression *initalVar = it->first;
                      SgExpression *lhs = *(updatedValues->begin());
                      SgType *baseType = lhs->get_type();

                    //Select correct voting mechanism...
                      SgStatement *sg = adj->getHandler(initalVar, updatedValues, bb, baseType);
                      if(sg != NULL)
                         SageInterface::appendStatement(sg, bb);
       }
     //Is someone else taking care of the cleanup?
       if(cascaded)
          return bb;
       else
           return createBasicIf(body, bb, ftVars, condSuccess, NULL, alwaysFail);
}

//Fault policy "Voting" - Vote on the results
SgStatement *FT::Transform::Adjucator_Voting::getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
     if(votingMechPerType == NULL)
          throw FT::Common::FTException("Voting chain not initialized.");
     else
          return votingMechPerType->getHandler(initialVar, updatedValues, bb, matchType);
}

SgStatement *FT::Transform::Adjucator_Voting_Mean::getHandler(SgExpression *initalVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
     //Initialize
       if(initalVar == NULL || updatedValues == NULL)
          return NULL;
       if(updatedValues->size() == 0)
          return NULL;
       SgExpression *value = *(updatedValues->begin()),
                    *lhs = (weights.size() > 0 ? SageBuilder::buildBinaryExpression<SgMultiplyOp>(
                                                  SageBuilder::buildDoubleVal(weights[0]), 
                                                  value) : value);
       SgType *baseType = lhs->get_type();
     //Create a sum of all the expressions...
       SgAddOp *addOp = SageBuilder::buildBinaryExpression<SgAddOp>(lhs, NULL),
               *firstAddOp = addOp;
       unsigned int i = 0;
       for(set<SgExpression *>::iterator it2 = ++updatedValues->begin();
           it2 != updatedValues->end();
           ++it2, i++) {
                   if(weights.size() <= i)
                      value = *it2;
                   else
                        value = SageBuilder::buildBinaryExpression<SgMultiplyOp>(
                                   SageBuilder::buildDoubleVal(weights[i]), 
                                   *it2);
                   addOp->set_rhs_operand_i( SageBuilder::buildBinaryExpression<SgAddOp>(value, NULL) );
                   addOp->get_rhs_operand_i()->set_parent(addOp);
                   addOp = isSgAddOp( addOp->get_rhs_operand_i() );
       }
     //Finalize the expression
       if(addOp->get_parent() != NULL) {
          SgAddOp *addOpParent = isSgAddOp(addOp->get_parent());
          addOpParent->set_rhs_operand_i(addOp->get_lhs_operand_i());
       } else
             addOp = isSgAddOp(addOp->get_lhs_operand_i());
       addOp->set_lhs_operand_i(NULL);
       SageInterface::deleteAST(addOp);

     return SageBuilder::buildExprStatement(
               SageBuilder::buildBinaryExpression<SgAssignOp>(
                              initalVar, 
                              SageBuilder::buildBinaryExpression<SgDivideOp>(
                                  firstAddOp, 
                                  BInterface::buildConstant(baseType, updatedValues->size()) )));
}

SgStatement *FT::Transform::Adjucator_Voting_Median::getHandler(SgExpression *initalVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
     //Create array variable...
       
     //Create sorting call...

     ROSE_ASSERT(false);
}

SgStatement *FT::Transform::Adjucator_Voting_Exact::getHandler(SgExpression *initalVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
     //Initialize
       bool useAddr = false;
       if(initalVar == NULL || updatedValues == NULL)
          return NULL;
       if(updatedValues->size() == 0)
          return NULL;
       SgExpression *lhs = *(updatedValues->begin());
       SgType *baseType = lhs->get_type();

     //Build variable declarations
       map<string, int>::iterator it2 = varCount.find(SageInterface::get_name(baseType));
       int varIndex = (it2 == varCount.end() ? 0 : it2->second);
       varCount[SageInterface::get_name(baseType)] = varIndex+1;
       stringstream ss;
       ss << varIndex;

       SgType *typeOfVar = (useAddr ? SageBuilder::buildPointerType(baseType) : baseType);
     //Decide which algorithm to use...
       if(assumeMajElemExist) {
         /*The following algorithm (from Robert S. Boyer, J Strother Moore, "MJRTY - A Fast Majority Vote Algorithm", 1982),
           finds the majority vote in O(n) if it exists, otherwise the output in undefined/invalid.
         */
            vector<SgExpression *> expValues(updatedValues->begin(), updatedValues->end());
            expValues.insert(expValues.begin(), SageBuilder::buildCastExp(SageBuilder::buildIntVal(0), typeOfVar, SgCastExp::e_C_style_cast) );
            SgVariableDeclaration *vX = SageBuilder::buildVariableDeclaration(
                         SgName("voting_mean_" + SageInterface::get_name(baseType) + "_" + ss.str()),
                         SageBuilder::buildArrayType( typeOfVar, SageBuilder::buildIntVal(updatedValues->size()+1) ), 
                         BInterface::buildArrayInitializer(&expValues, useAddr),
                         bb);
            SageInterface::appendStatement( vX, bb );

            if(iDecl == NULL) {
               iDecl = SageBuilder::buildVariableDeclaration(SgName("i"), SageBuilder::buildIntType(), NULL, bb);
               SageInterface::appendStatement( iDecl, bb );
               cntVoteDecl = SageBuilder::buildVariableDeclaration(SgName("cntVote"), SageBuilder::buildIntType(), NULL, bb);
               SageInterface::appendStatement( cntVoteDecl, bb );
            }
          //Include for loop...
            return SageBuilder::buildForStatement(
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgCommaOpExp>(
                         SageBuilder::buildBinaryExpression<SgAssignOp>(SageBuilder::buildVarRefExp(iDecl), SageBuilder::buildIntVal(1)),
                         SageBuilder::buildBinaryExpression<SgAssignOp>(SageBuilder::buildVarRefExp(cntVoteDecl), SageBuilder::buildIntVal(0)))),
               SageBuilder::buildExprStatement(
                    SageBuilder::buildBinaryExpression<SgLessThanOp>(
                         SageBuilder::buildVarRefExp(iDecl), 
                         SageBuilder::buildIntVal(updatedValues->size()+1) )),
               SageBuilder::buildPlusPlusOp(SageBuilder::buildVarRefExp(iDecl), SgUnaryOp::postfix),
               SageBuilder::buildIfStmt(
                    SageBuilder::buildBinaryExpression<SgEqualityOp>(
                         SageBuilder::buildVarRefExp(cntVoteDecl),
                         SageBuilder::buildIntVal(0)),
                    SageBuilder::buildBasicBlock(
                         SageBuilder::buildAssignStatement(
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(vX),
                                   SageBuilder::buildIntVal(0)),
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(vX),
                                   SageBuilder::buildVarRefExp(iDecl))),
                         SageBuilder::buildAssignStatement(
                              SageBuilder::buildVarRefExp(cntVoteDecl),
                              SageBuilder::buildIntVal(1))),
                    SageBuilder::buildIfStmt(
                         SageBuilder::buildBinaryExpression<SgEqualityOp>(
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(vX),
                                   SageBuilder::buildIntVal(0)),
                              SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(
                                   SageBuilder::buildVarRefExp(vX),
                                   SageBuilder::buildVarRefExp(iDecl))),
                         SageBuilder::buildExprStatement(SageBuilder::buildPlusPlusOp(SageBuilder::buildVarRefExp(cntVoteDecl), SgUnaryOp::postfix)),
                         SageBuilder::buildExprStatement(SageBuilder::buildMinusMinusOp(SageBuilder::buildVarRefExp(cntVoteDecl), SgUnaryOp::postfix)) )));  
       } else
          throw FT::Common::FTException("No exact voting algorithm fits the given constrains.");
}

SgStatement *FT::Transform::Adjucator_Voting_Fuzzy::getHandler(SgExpression *initalVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
     return NULL;
}
