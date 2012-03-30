#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "rose.h"
#include "OmpAttribute.h"

#include "TI.h"
#include "uq.h"

using namespace std;

namespace AttributeIDs {
     static const char *ATTRIBUTE_ID_TEMP_REF = "ROSE_UQ_TRANSFORM_IS_TEMPORARY_REGISTER";
     static const char *ATTRIBUTE_ID_PC_VALUE = "ROSE_UQ_TRANSFORM_IS_PC_VALUE";
     static const char *ATTRIBUTE_ID_PROCESS_VARS = "ROSE_UQ_TRANSFORM_PROCESS_VARIABLES";
}; 

#define PC_TYPE_NAME_PREFIX "__"

#define UQ_DEBUG

struct UQOp {
     private:
          bool recursivlyApply;
          SgGlobal *globalScope;
          string nameOfPCVar;
          typedef enum {ADD, MUL, SUB, DIV} PC_OPERATIONS;         
          

          //Handling of temporary variables...
          int lastTempVar;
          map<string, SgMemberFunctionRefExp *> pcLibNameToFuncMap;
          vector<SgStatement *> tempDeclBlock, varDeclBlock;
          vector<SgVarRefExp *> currentAvailableTempVars;

          SgVarRefExp *getTempReg(SgType *typeOfTempReg) {
               //Are there any free variables?
                 if(currentAvailableTempVars.size() > 0)
                    return *(currentAvailableTempVars.begin());
               //Create new temp register...
                 stringstream ss;
                 ss << "tmpReg" << lastTempVar++;

                 SgType *typeOfVar = SageInterface::removeConst(typeOfTempReg);
                 Rose_STL_Container<SgType *> templateArguments;
                 if(SageInterface::getElementType( typeOfVar ) == NULL) {
                         templateArguments.push_back( typeOfVar );
                         typeOfVar = TInterface::getTemplateSpecialization(SgName("UQTKArray1D"), templateArguments, globalScope);
                 } else {
                         templateArguments.push_back( SageInterface::getElementType( typeOfVar ) );
                         typeOfVar = BInterface::replaceElementType(
                                        typeOfVar, 
                                        TInterface::getTemplateSpecialization(SgName("UQTKArray1D"), templateArguments, globalScope));
                 }
                 SgClassType *classType = isSgClassType( typeOfVar );
                 ROSE_ASSERT(classType != NULL);
                 SgClassDeclaration *cDecl = isSgClassDeclaration(classType->get_declaration());
                 SgClassDeclaration *pcDecl = SageInterface::lookupClassSymbolInParentScopes(SgName("PCSet"), globalScope)->get_declaration();
                 ROSE_ASSERT((cDecl != NULL) && (pcDecl != NULL));
                 SgMemberFunctionSymbol *mFuncSym = isSgMemberFunctionSymbol(
                         TInterface::getClassFunction(SgName("GetNumberPCTerms"), pcDecl)->search_for_symbol_from_symbol_table());
                 ROSE_ASSERT(mFuncSym != NULL);
                 SgVariableDeclaration *decl = SageBuilder::buildVariableDeclaration(
                                                  SgName( ss.str() ),
                                                  typeOfVar,
                                                  SageBuilder::buildConstructorInitializer(
                                                       SageInterface::getDefaultConstructor(cDecl),
                                                       SageBuilder::buildExprListExp(
                                                            SageBuilder::buildFunctionCallExp(
                                                                 SageBuilder::buildBinaryExpression<SgDotExp>(
                                                                      SageBuilder::buildVarRefExp(
                                                                           SageInterface::lookupVariableSymbolInParentScopes(SgName(nameOfPCVar), globalScope)),
                                                                      SageBuilder::buildMemberFunctionRefExp(mFuncSym, true, false)),
                                                                 SageBuilder::buildExprListExp())),
                                                       classType, true, true, true, false),
                                                  globalScope);
                 tempDeclBlock.push_back(decl);
               //Return with newly created node with attribute set
                 return static_cast<SgVarRefExp *>(
                         TInterface::AddValueAttribute<bool>(
                              TInterface::AddValueAttribute<string>(
                                   SageBuilder::buildVarRefExp(decl),
                                   AttributeIDs::ATTRIBUTE_ID_TEMP_REF,
                                   "{}"),
                              AttributeIDs::ATTRIBUTE_ID_PC_VALUE,
                              true));
          }
          void returnTempReg(SgVarRefExp *reg) {
               ROSE_ASSERT(reg != NULL);
               currentAvailableTempVars.push_back(reg);
          }

          bool IsScalar(SgExpression *exp) {
               return TInterface::hasValueAttribute(exp, AttributeIDs::ATTRIBUTE_ID_PC_VALUE);
          }
         
          SgFunctionCallExp *createFunctionCall(PC_OPERATIONS op, SgType *baseType, SgExpression *lhs, SgExpression *rhs, SgExpression *dest, bool inPlace) {
               string funcName;
               SgExprListExp *expCallList;
               //Decide on which function to use
                 switch(op) {
                  case ADD:   funcName = "Add";        break;                         
                  case SUB:   funcName = "Subtract";   break;
                  case DIV:   funcName = "Div";        break;
                  case MUL:   funcName = "Multiply";   break;
                 } 

                 if(!TInterface::hasValueAttribute(lhs, AttributeIDs::ATTRIBUTE_ID_PC_VALUE)) {
                    ROSE_ASSERT( TInterface::hasValueAttribute(rhs, AttributeIDs::ATTRIBUTE_ID_PC_VALUE) ); //createFunctionCall() should never been called...
                    switch(op) {
                     case ADD: /* Commuting ops: Use "OpScalar(X, Const)" instead! */
                     case MUL: return createFunctionCall(op, baseType, rhs, lhs, dest, inPlace);
                     case DIV: /* Non-commutative ops */
                     case SUB: funcName += "Scalar";
                    }
                 } else if( !TInterface::hasValueAttribute(rhs, AttributeIDs::ATTRIBUTE_ID_PC_VALUE) ) {
                         funcName += "Scalar";
                 }  

                 SgExpression *destReg = (dest == NULL ? getTempReg(baseType) : dest);
                 if(inPlace) {
                    funcName += "InPlace";
                    expCallList = SageBuilder::buildExprListExp(lhs, rhs);
                 } else
                    expCallList = SageBuilder::buildExprListExp(lhs, rhs, destReg);

               //Create call
                 SgMemberFunctionRefExp *refExp = NULL;
                 map<string, SgMemberFunctionRefExp *>::iterator it;
                 if((it = pcLibNameToFuncMap.find(funcName)) != pcLibNameToFuncMap.end())
                    refExp = it->second;

                 SgFunctionCallExp *fCall = SageBuilder::buildFunctionCallExp( 
                         SageBuilder::buildBinaryExpression<SgDotExp>(
                              SageBuilder::buildVarRefExp(nameOfPCVar),
                              refExp),
                         expCallList);
               //Remove either of LHS & RHS if their temporaries
                 if(TInterface::hasValueAttribute(lhs, AttributeIDs::ATTRIBUTE_ID_TEMP_REF))    returnTempReg(static_cast<SgVarRefExp *>(lhs));
                 if(TInterface::hasValueAttribute(rhs, AttributeIDs::ATTRIBUTE_ID_TEMP_REF))    returnTempReg(static_cast<SgVarRefExp *>(rhs)); 

               return static_cast<SgFunctionCallExp *>(
                         TInterface::AddValueAttribute<bool>(
                              fCall,
                              AttributeIDs::ATTRIBUTE_ID_PC_VALUE,
                              true));               
          }

          SgExpression *UQ(SgExpression *expInput, SgExpression *dest, map<string, bool> *v, SgScopeStatement *scope) {
               SgExprListExp *expList;
               SgExpression *expLHS, *expRHS, *expNewLHS = NULL, *expNewRHS = NULL;
               SgBinaryOp *binOp;
               //Make sure there are some variables that could be interesting...
                 if(v == NULL)
                    return expInput;
               //Handle differently depending on type
                 if(TInterface::IsTerminalVariable(expInput)) {
                         switch(expInput->variantT()) {
                          case V_SgPntrArrRefExp:
                          case V_SgVarRefExp:
                          case V_SgDotExp:
                          case V_SgArrowExp:
                          case V_SgPointerDerefExp:
                          case V_SgCastExp: {
                            //Get name of reference...
                              SgInitializedName *iName;
                              if((iName = SageInterface::convertRefToInitializedName(expInput)) == NULL)
                                   return expInput;
                            //See if any matches the variables of the process
                              for(map<string, bool>::iterator it = v->begin();
                                  it != v->end();
                                  ++it)
                                   if(boost::iequals( iName->get_name().getString(), it->first )) {
                                        //Update name...
                                          SgInitializedName *newiName = SageBuilder::buildInitializedName(
                                                  SgName(PC_TYPE_NAME_PREFIX + it->first),
                                                  iName->get_typeptr(),
                                                  NULL);
                                        //Return results
                                        return static_cast<SgExpression *>(
                                                  TInterface::AddValueAttribute<bool>(
                                                       BInterface::replaceInitializedName(expInput, newiName, scope),
                                                       AttributeIDs::ATTRIBUTE_ID_PC_VALUE,
                                                       true));
                                   }
                              return expInput; }
                          default:
                              return expInput;
                         }
                 } else if(TInterface::IsTerminalConstant(expInput)) {
                    return expInput;
                 } else if(TInterface::IsUnaryOp(expInput, &expLHS)){
                    expNewLHS = UQ(expLHS, NULL, v, scope);
                    if(!TInterface::hasValueAttribute(expNewLHS, AttributeIDs::ATTRIBUTE_ID_PC_VALUE))
                         return expInput;
                    switch(expInput->variantT()) {
                     case V_SgMinusMinusOp:  return createFunctionCall(SUB, expLHS->get_type(), expNewLHS, SageBuilder::buildDoubleVal(1.0), dest, false); // Sub(LHS, LHS, 1)
                     case V_SgPlusPlusOp:    return createFunctionCall(ADD, expLHS->get_type(), expNewLHS, SageBuilder::buildDoubleVal(1.0), dest, false); // Add(LHS, LHS, 1)
                     /*default: TInterface::AddValueAttribute<bool>(expInput, AttributeIDs::ATTRIBUTE_ID_PC_VALUE, true);
                              TInterface::SetUnaryOp(expInput, expNewLHS);
                              return expInput;*/
                     default:
                         cout << "Unsupport unary expression type '" << expInput->class_name() << "'." << endl;
                         ROSE_ASSERT(false);
                    }
                 } else if(TInterface::IsBinaryOp(expInput, &expLHS, &expRHS)){
                    binOp = isSgBinaryOp(expInput);

                    switch(binOp->variantT()) {
                     case V_SgAssignOp: {
                         //Is this the initialization?
                           map<string, bool>::iterator it;
                           SgInitializedName *iName = SageInterface::convertRefToInitializedName(expLHS);
                           if((it = v->find( iName->get_name().getString() )) == v->end()) {
                              //Print debug message?
                              if(SgProject::get_verbose() > 0)
                                   cout << "Var '" << iName->get_name().getString() << "' not found" << endl;
                              
                              if((expNewRHS = UQ(expRHS, NULL, v, scope)) != expRHS)
                                   TInterface::SetBinaryOp(expInput, expLHS, expNewRHS);
                              return expInput; 
                           } else {
                              //Has the variable been initiated?
                              if(it->second == false) {
                                   //Print debug message?
                                   if(SgProject::get_verbose() > 0)
                                        cout << "Var '" << iName->get_name().getString() << "' not initialized" << endl;

                                   SgVariableSymbol *varSym = SageInterface::lookupVariableSymbolInParentScopes(PC_TYPE_NAME_PREFIX + iName->get_name(), scope);
                                   //Handle differently if its a element various composite type
                                     SgClassType *classType = isSgClassType( varSym->get_type() );
                                     if(classType == NULL) 
                                        if((classType = isSgClassType(SageInterface::getElementType(varSym->get_type())) ) == NULL) {
                                            cout << "Element type of '" << iName->get_name().getString() << "' is not a class type, hasn't declaration occured?" << endl;
                                            ROSE_ASSERT(false);
                                        }
                                   SgClassDeclaration *cDecl = isSgClassDeclaration(classType->get_declaration());
                                   SgClassDeclaration *pcDecl = SageInterface::lookupClassSymbolInParentScopes(SgName("PCSet"), globalScope)->get_declaration();
                                   ROSE_ASSERT((cDecl != NULL) && (pcDecl != NULL));
                                   SgMemberFunctionSymbol *mFuncSym = isSgMemberFunctionSymbol(
                                             TInterface::getClassFunction(SgName("GetNumberPCTerms"), pcDecl)->search_for_symbol_from_symbol_table());
                                   ROSE_ASSERT(mFuncSym != NULL);
                                   //Update all references...
                                     if(TInterface::changeAllRefs(expLHS,
                                                  SageBuilder::buildVarRefExp(iName,scope),
                                                  SageBuilder::buildVarRefExp(varSym),
                                                  false))
                                        	expNewLHS = expLHS;
                                     ROSE_ASSERT(expNewLHS != NULL);
                                   //Create assignment object
                                     SgAssignOp *assignExp = SageBuilder::buildBinaryExpression<SgAssignOp>(
                                             expLHS,
                                             SageBuilder::buildConstructorInitializer(
                                                  SageInterface::getDefaultConstructor(cDecl),
                                                  SageBuilder::buildExprListExp(
                                                       SageBuilder::buildFunctionCallExp(
                                                            SageBuilder::buildBinaryExpression<SgDotExp>(
                                                                 SageBuilder::buildVarRefExp(
                                                                      SageInterface::lookupVariableSymbolInParentScopes(SgName(nameOfPCVar), globalScope)),
                                                                 SageBuilder::buildMemberFunctionRefExp(mFuncSym, true, false)),
                                                            SageBuilder::buildExprListExp()),
                                                       UQ(expRHS, NULL, v, scope)),
                                                  classType, true, true, true, false) );
                                   //Update map and return result, annotated with attribute
                                     (*v)[iName->get_name().getString()] = true;
                                     return static_cast<SgExpression *>(
                                             TInterface::AddValueAttribute<bool>(
                                                  assignExp,
                                                  AttributeIDs::ATTRIBUTE_ID_PC_VALUE,
                                                  true));                            
                              } else {
                                   expNewLHS = UQ(expLHS, NULL, v, scope);

                                   return static_cast<SgExpression *>(
                                             TInterface::AddValueAttribute<bool>(
                                                  UQ(expRHS, expNewLHS, v, scope),
                                                  AttributeIDs::ATTRIBUTE_ID_PC_VALUE,
                                                  true));
                              }
                           }
                       } break;
                     default:
                         expNewLHS = UQ(expLHS, NULL, v, scope);
                         expNewRHS = UQ(expRHS, NULL, v, scope);
                         if( !TInterface::hasValueAttribute(expNewLHS, AttributeIDs::ATTRIBUTE_ID_PC_VALUE) &&
                             !TInterface::hasValueAttribute(expNewRHS, AttributeIDs::ATTRIBUTE_ID_PC_VALUE) )
                                   return expInput;

                         switch(binOp->variantT()) {
                          case V_SgAddOp:      return createFunctionCall(ADD, binOp->get_type(), expNewLHS, expNewRHS, dest, false); // Add(dest, LHS, RHS)
                          case V_SgDivideOp:   return createFunctionCall(DIV, binOp->get_type(), expNewLHS, expNewRHS, dest, false); // Div(dest, LHS, RHS)
                          case V_SgMultiplyOp: return createFunctionCall(MUL, binOp->get_type(), expNewLHS, expNewRHS, dest, false); // Mult(dest, LHS, RHS)
                          case V_SgSubtractOp: return createFunctionCall(SUB, binOp->get_type(), expNewLHS, expNewRHS, dest, false); // Sub(dest, LHS, RHS)

                          case V_SgPlusAssignOp:  return createFunctionCall(ADD, binOp->get_type(), expNewLHS, expNewRHS, NULL, true); // AddInPlace(LHS, RHS)
                          case V_SgDivAssignOp:   return createFunctionCall(DIV, binOp->get_type(), expNewLHS, expNewRHS, NULL, true); // Div(LHS, LHS, RHS)
                          case V_SgMultAssignOp:  return createFunctionCall(MUL, binOp->get_type(), expNewLHS, expNewRHS, NULL, true); // MultiplyInPlace(LHS, RHS)
                          case V_SgMinusAssignOp: return createFunctionCall(SUB, binOp->get_type(), expNewLHS, expNewRHS, NULL, true); // SubInPlace(LHS, LHS, RHS)
                          /*default: TInterface::AddValueAttribute<bool>(expInput, AttributeIDs::ATTRIBUTE_ID_PC_VALUE, true);
                                   TInterface::SetBinaryOp(expInput, expNewLHS, expNewRHS);
                                   return expInput;*/
                          default:
                              cout << "Unsupport binary expression type '" << expInput->class_name() << "'." << endl;
                              ROSE_ASSERT(false);
                         } break;
                    }
				return expInput;
                 } else if(TInterface::IsNaryOp(expInput, &expList)){
                       //Re-compose a vector of updated expressions...
                         Rose_STL_Container<SgExpression*> expContainer;
                         bool hasOperatorBeenUpdated = false;
                         for(Rose_STL_Container<SgExpression*>::iterator it = expList->get_expressions().begin();
                             it != expList->get_expressions().end();
                             ++it) {
                              expNewLHS = UQ(*it, NULL, v, scope);
                              hasOperatorBeenUpdated |= TInterface::hasValueAttribute(expNewLHS, AttributeIDs::ATTRIBUTE_ID_PC_VALUE);
                              expContainer.push_back(expNewLHS);
                         }
                       //See if anything changed..
                         if( hasOperatorBeenUpdated )
                              TInterface::SetNaryOp(expInput, SageBuilder::buildExprListExp(expContainer) );
                       return expInput;
                 } else {
                    cout << "Unsupport expression '" << expInput->class_name() << "'." << endl;
                    ROSE_ASSERT(false);
                 }
               return expInput;
          }

     public:                                            
          UQOp(SgGlobal *scope, int n_dim, int order, string pc_type) {
              this->globalScope = scope;
              this->lastTempVar = 0;
              this->nameOfPCVar = "pc"; 
              //Add code to initiate PC lib
                SgVariableDeclaration *dimDecl =
                                        SageBuilder::buildVariableDeclaration(
                                             SgName("pcDimension"),
                                             SageBuilder::buildIntType(),
                                             SageBuilder::buildAssignInitializer(
                                                  SageBuilder::buildIntVal( n_dim ),
                                                  SageBuilder::buildIntType()),
                                             globalScope);
                SageInterface::attachComment(dimDecl, "Initialization of PC-based UQTK... ");
                tempDeclBlock.push_back(dimDecl);
                SgVariableDeclaration *ordDecl =
                                        SageBuilder::buildVariableDeclaration(
                                             SgName("pcOrder"),
                                             SageBuilder::buildIntType(),
                                             SageBuilder::buildAssignInitializer(
                                                  SageBuilder::buildIntVal( order ),
                                                  SageBuilder::buildIntType()),
                                             globalScope);
                tempDeclBlock.push_back(ordDecl);

                SgClassSymbol *classSym = SageInterface::lookupClassSymbolInParentScopes(SgName("PCSet"), globalScope);
                if(classSym == NULL) {
                    ROSE_ASSERT( TInterface::loadHeaderFile( this->globalScope, "PCSet.h", false) );
                    SageInterface::insertHeader("PCSet.h", PreprocessingInfo::after, false, globalScope);
                    classSym = SageInterface::lookupClassSymbolInParentScopes(SgName("PCSet"), globalScope);
                    ROSE_ASSERT(classSym != NULL);
                }
                SgClassDeclaration *classDecl = classSym->get_declaration();
                SgMemberFunctionDeclaration* classConstructor = SageInterface::getDefaultConstructor(classDecl);
                SgVariableDeclaration *pcDecl =
                                        SageBuilder::buildVariableDeclaration(
                                             SgName("pc"),
                                             BInterface::buildClassType(classDecl),
                                             SageBuilder::buildConstructorInitializer(
                                                  classConstructor,
                                                  SageBuilder::buildExprListExp(
                                                       SageBuilder::buildVarRefExp(ordDecl),
                                                       SageBuilder::buildVarRefExp(dimDecl),
                                                       SageBuilder::buildStringVal(pc_type)),
                                                  BInterface::buildClassType(classDecl),
                                                  false, false, true, false),
                                             globalScope);
                tempDeclBlock.push_back(pcDecl);
              //Create function map for PC lib...
                SgMemberFunctionSymbol *mFuncSym;

                const int PC_LIB_FUNCTIONS = 9;
                const char *pcMemberFuncs[PC_LIB_FUNCTIONS] = {
                         "Add", /*"AddScalar", "AddScalarInPlace"*/ "AddInPlace", 
                         "Subtract", /*"SubtractScalar", "SubtractScalarInPlace"*/ "SubtractInPlace", 
                         "Div", //"DivScalar", "DivScalarInPlace", "DivInPlace"
                         "Multiply", "MultiplyScalar", "MultiplyScalarInPlace", /*"MultiplyInPlace"*/
                    };
                for(int i = 0; i < PC_LIB_FUNCTIONS-1; i++) {
                    //cout << "Checking '" << pcMemberFuncs[i] << "' ..." << endl;
                    SgMemberFunctionDeclaration *newPtr = TInterface::getClassFunction(SgName(pcMemberFuncs[i]), classDecl);
                    if(newPtr == false) {
                         cout << "Unable to find function declaration '" << pcMemberFuncs[i] << "' in object 'PCSet', quitting!" << endl;
                         ROSE_ASSERT(false);
                    }
                    mFuncSym = isSgMemberFunctionSymbol(newPtr->search_for_symbol_from_symbol_table() );
                    ROSE_ASSERT(mFuncSym != NULL);
                    pcLibNameToFuncMap[ pcMemberFuncs[i] ] = SageBuilder::buildMemberFunctionRefExp(mFuncSym, true, false);
                }                    
          }
      
          SgNode *operator()(SgNode *inputNode, SgScopeStatement *scope, map<string, bool> *variables = NULL) {
                 SgNode *nodePtrA, *nodePtrB, *nodePtrC, *nodePtrD, *nodeResults = NULL;
                 SgStatementPtrList *stmBodyList;
                 SgStatement *stmBody;
                 SgScopeStatement *nextScope = NULL;

                 if(inputNode == NULL)
                    return NULL;
                 //See if node contains any attributes that needs to be propagated
                   map<string, bool> *curVariables = variables;
                   bool curWasCreatedHere = false;
                   if(TInterface::hasValueAttribute(inputNode, AttributeIDs::ATTRIBUTE_ID_PROCESS_VARS)) {
                         if(curVariables == NULL) {
                              curVariables = new map<string, bool>();
                              curWasCreatedHere = true;
                         }

                         map<string, bool> *vars = TInterface::GetValueAttribute<map<string, bool> *>(inputNode, AttributeIDs::ATTRIBUTE_ID_PROCESS_VARS);
                         for(map<string, bool>::iterator it = vars->begin(); it != vars->end(); ++it)
                              (*curVariables)[it->first] = it->second;
                   }

                 //Recursivly handle statement(s) if asked
                   if(TInterface::IsLoop(inputNode, &nodePtrA, &nodePtrB, &nodePtrC, &nodePtrD)) {
                      SgStatement *stmNodeD = isSgStatement( nodePtrD );
                      nextScope = isSgScopeStatement(inputNode);
                      nextScope = (nextScope != NULL ? nextScope : scope);

                      SgStatement *stmBody = isSgStatement( (*this)(stmNodeD, nextScope, curVariables) );
                      if(!TInterface::IsBlock(stmBody, NULL, NULL, NULL) && (stmBody != stmNodeD)) {
                              SageInterface::removeStatement(stmNodeD);
                              stmBody = SageBuilder::buildBasicBlock(stmBody, stmNodeD);
                      }
                      SageInterface::prependStatementList(varDeclBlock, isSgScopeStatement(stmBody));
                      varDeclBlock.clear();

                      if(TInterface::SetLoop(inputNode, nodePtrA, nodePtrB, nodePtrC, stmBody))                                   
                         nodeResults = inputNode;
                   } else if(TInterface::IsIf(inputNode, &nodePtrA, &nodePtrB, &nodePtrC)) {
                          SgStatement *stmNodeB = isSgStatement( nodePtrB ),
                                      *stmNodeC = isSgStatement( nodePtrC );
                          SgStatement *stmIfBody = isSgStatement( (*this)(stmNodeB, scope, curVariables) );
                          if(!TInterface::IsBlock(stmNodeB, NULL, NULL, NULL) && (stmIfBody != stmNodeB)) {
                              SageInterface::removeStatement(stmNodeB);
                              stmIfBody = SageBuilder::buildBasicBlock(stmIfBody, stmNodeB);
                          }
                          SageInterface::prependStatementList(varDeclBlock, isSgScopeStatement(stmIfBody));
                          varDeclBlock.clear();

                          SgStatement *stmElseBody = isSgStatement( (*this)(stmNodeC, scope, curVariables) );
                          if(!TInterface::IsBlock(stmNodeC, NULL, NULL, NULL) && (stmElseBody != stmNodeC)) {
                              SageInterface::removeStatement(stmNodeC);
                              stmElseBody = SageBuilder::buildBasicBlock(stmElseBody, stmNodeC);
                          }                              
                          SageInterface::prependStatementList(varDeclBlock, isSgScopeStatement(stmElseBody));
                          varDeclBlock.clear();

                          if(TInterface::SetIf(inputNode, nodePtrA, stmIfBody, stmElseBody))
                             nodeResults = inputNode;
                   } else if(TInterface::IsBlock(inputNode, &stmBody, &stmBodyList, &nextScope)) {
                          nextScope = (nextScope != NULL ? nextScope : scope);
                          SgStatement *stm;
                          SgStatementPtrList *stmBodyListNew = stmBodyList;
                          //Handle stm list (if any)...
                            if(stmBodyList != NULL) {
                                stmBodyListNew = new SgStatementPtrList();
                                for(Rose_STL_Container<SgStatement*>::iterator it = stmBodyList->begin();
                                    it != stmBodyList->end();
                                    ++it) {
                                         if((stm = isSgStatement( (*this)(*it, nextScope, curVariables)) ) != *it) {
                                             //Add new statement...
                                             stmBodyListNew->push_back(stm);
                                             stm->set_parent(inputNode);
                                         }
                                         stmBodyListNew->push_back( *it );
                                }
                                stmBodyList->insert(stmBodyList->begin(), varDeclBlock.begin(), varDeclBlock.end());
                                varDeclBlock.clear();
                             }
                          //... andle handle statement (if any).
                            if(stmBody != NULL) {
                              SgBasicBlock *bb = SageBuilder::buildBasicBlock();
                              SageInterface::appendStatement(stmBody, bb);
                              if((stm = isSgStatement( (*this)(stmBody, nextScope, curVariables)) ) != stmBody)
                                //Add new statement...
                                  SageInterface::prependStatement(stm, bb);
                              SageInterface::appendStatementList(varDeclBlock, bb);
                              stmBody = bb;
                              varDeclBlock.clear();
                            }
                          //Update node...
                            TInterface::SetBlock(inputNode, stmBody, stmBodyListNew, nextScope);

                          //Print debug message?
                            if(SgProject::get_verbose() > 0)
                              cout << "Block output: " << inputNode->unparseToString() << endl << endl;                         
                          nodeResults = inputNode;
                   } else if(TInterface::IsDeclaration(inputNode)) {
                         //No point in doing anything if no variables has been defined
                           if(curVariables == NULL)
                              nodeResults = inputNode;
                         //Handle different declarations differently
                           switch(inputNode->variantT()) {
                            case V_SgVariableDeclaration: {
                              SgVariableDeclaration *varDecl = isSgVariableDeclaration( inputNode );
                              ROSE_ASSERT(varDecl->get_variables().size() == 1);
                              SgInitializedName *iName = *(varDecl->get_variables().begin());
                              //See if any matches the variables of the process
                                for(map<string, bool>::iterator it = curVariables->begin();
                                    it != curVariables->end();
                                    ++it)
                                        if(boost::iequals( iName->get_name().getString(), it->first )) {
                                           //Change type of variables...
                                             SgType *typeOfVar;
                                             Rose_STL_Container<SgType *> templateArguments;
                                             if(SageInterface::getElementType( iName->get_type() ) == NULL) {
                                                templateArguments.push_back( iName->get_type() );
                                                typeOfVar = TInterface::getTemplateSpecialization(SgName("UQTKArray1D"), templateArguments, globalScope);
                                             } else {
                                                  templateArguments.push_back( SageInterface::getElementType( iName->get_type() ) );
                                                  typeOfVar = BInterface::replaceElementType(
                                                                 iName->get_type(), 
                                                                 TInterface::getTemplateSpecialization(SgName("UQTKArray1D"), templateArguments, globalScope));
                                     	     }
                                           //Do we have a initializer?
                                             SgInitializer *initPtr = iName->get_initptr();
                                             if(initPtr != NULL) {
                                                  initPtr = isSgInitializer( UQ(initPtr, NULL, curVariables, scope) );
                                                  (*variables)[iName->get_name().getString()] = true;
                                             }
                                           //Alter variable declaration...
                                             SgVariableDeclaration *newVarDecl = SageBuilder::buildVariableDeclaration(
                                                  	PC_TYPE_NAME_PREFIX + iName->get_name(),
                                                       typeOfVar,
                                                       initPtr,
                                                       scope);
                                           //Enter results
                                             nodeResults = newVarDecl;
                                        }
                              //Was variable supposed to have a PC type?     
                                if(nodeResults == NULL)
                                   nodeResults = inputNode;
                             } break;
                            default:
                              cout << "Unhandled declaration '" << inputNode->class_name() << "'" << endl;
                              return inputNode;
                           }
                   } else if(TInterface::IsStatement(inputNode)) { 
                          SgStatement *stm = isSgStatement(inputNode);
                          /*//Check to see if there are any side-effects to the given variables
                            set< SgInitializedName * > rVars, wVars;
                            if((stm != NULL) && SageInterface::collectReadWriteVariables(stm, rVars, wVars)) {
                                  bool sideEffectsOnVarFound = false;
                                  for(set< SgInitializedName * >::iterator it = wVars.begin();
                                      !sideEffectsOnVarFound && (it != wVars.end());
                                      ++it)
                                       for(map<string, bool>::iterator it2 = curVariables->begin();
                                           !sideEffectsOnVarFound && (it2 != curVariables->end());
                                           ++it2) 
                                            if(boost::iequals( (*it)->get_name().getString(), it2->first ))
                                                 sideEffectsOnVarFound = true;
                                  if(!sideEffectsOnVarFound)
                                       return stm;
                            }*/
                          //Process statement
                            SgExpression *exp = NULL;
                            switch(inputNode->variantT()) {
                              case V_SgExprStatement:  exp = static_cast<SgExprStatement *>(stm)->get_expression();    break;
                              case V_SgReturnStmt:     exp = static_cast<SgExprStatement *>(stm)->get_expression();    break;
                              default:
                                   cout << "Unhandled statement '" << stm->class_name() << "'" << endl;
                                   return stm;
                            }
                            SgExpression *expCopy = SageInterface::deepCopy<SgExpression>(exp);
                            SgExpression *newExp = UQ(expCopy, NULL, curVariables, scope);
                          //Add comment about transformation...
                            if( TInterface::hasValueAttribute(newExp, AttributeIDs::ATTRIBUTE_ID_PC_VALUE) ) {
                              //Print debug message?
                                if(SgProject::get_verbose() > 0)
                                   SageInterface::attachComment(stm, "Original statement: " + stm->unparseToString());
                              switch(inputNode->variantT()) {
                                   case V_SgExprStatement:  nodeResults = SageBuilder::buildExprStatement(newExp);      break;
                                   case V_SgReturnStmt:     nodeResults = SageBuilder::buildReturnStmt(newExp);         break;
                                   default:
                                        ROSE_ASSERT(false); //Something is missing, the switch 
                                                            //stm above contains a stm type not
                                                            //mentioned in this one.
                              }
                            } else {
                              SageInterface::deleteAST(expCopy);
                              nodeResults = stm;
                            }
                   } else
                         cout << "Unhandled node type '" << inputNode->class_name() << "'" << endl;
                 //Make sure parents variable map is updated...
                   if(curWasCreatedHere) {
                         delete curVariables;
                   }
                 //Return results...
                   return nodeResults;               
          }

          const vector<SgStatement *> &getDeclBlock() {return tempDeclBlock;}
};

int main(int argc, char *argv[]) {
     SgProject *project = frontend(argc, argv);
     ROSE_ASSERT(project != NULL);

     //Initialize toolkit     
     UQOp f(SageInterface::getFirstGlobalScope(project), 3, 1, "HG");

     vector<SgPragmaDeclaration* > pragmas = SageInterface::querySubTree<SgPragmaDeclaration>(project, V_SgPragmaDeclaration);
     vector< pair<SgStatement *, string> > vectProcessBB;
     int bbsProcessed = 0;
     for(std::vector<SgPragmaDeclaration* >::iterator it = pragmas.begin();
         it != pragmas.end();
         ++it) {
          SgPragma *pragma = (*it)->get_pragma();

          if( boost::iequals(pragma->get_pragma().substr(0, 10), "UQ_PROCESS") ) {
               map<string, bool> *vars = NULL;
               //Decode parameters...
                 vars = new map<string, bool>();
                 int startIndex = -1, endIndex = -1;
                 for(unsigned int i = 10; i < pragma->get_pragma().length(); i++)
                     if(pragma->get_pragma().at(i) == '(')
                        startIndex = i+1;
                     else if(pragma->get_pragma().at(i) == ')')
                        endIndex = i-1;
                 if(startIndex == -1 || endIndex == -1) {
                    cout << "Invalid parameter list given to UQ_PROCESS" << endl;
                    continue;
                 }
                 vector<std::string> strs;
                 string s = pragma->get_pragma().substr(startIndex, endIndex-startIndex);
                 boost::split(strs, s, boost::is_any_of(","));
                 for(vector<std::string>::iterator itF = strs.begin(); itF != strs.end(); itF++) {
                     boost::erase_all(*itF, " ");
                     vars->insert( pair<string, bool>(*itF, false) );
                     //cout << "Process variable '" << *itF << "'..." << endl;
                 }
               //Annotate next statement following the pragma...
                 SgStatement *nextStm = SageInterface::getNextStatement(*it), *nextStmReal = NULL;
                 if(nextStm != NULL) {
                         //Decide the real statement...
                         switch(nextStm->variantT()) {
                          case V_SgFunctionDeclaration: {
                                   SgFunctionDefinition *funcDef = isSgFunctionDeclaration(nextStm)->get_definition();
                                   if(funcDef != NULL)
                                        nextStmReal = funcDef->get_body();
                                   else
                                        cout << "Unable to find definition for function '" 
                                             << isSgFunctionDeclaration(nextStm)->get_name().getString() 
                                             << "', ingoring UQ_PROCESS" << endl;
                              } break;
                          default:
                              cout << "UQ_PROCESS cannot be placed before a statement of type '" << nextStm->class_name() << "'" << endl;
                              break;
                         }
                         //Add attributes
                         if(vars != NULL)
                              TInterface::AddValueAttribute(nextStmReal, AttributeIDs::ATTRIBUTE_ID_PROCESS_VARS, vars);
                         //Push statment to queue
                         if(nextStmReal != NULL)
                              vectProcessBB.push_back( pair<SgStatement *, string>(nextStmReal, SageInterface::get_name(nextStm)) );
                 }
               //Remove pragma from output
                 SageInterface::removeStatement( *it );
          } else if( boost::iequals(pragma->get_pragma().substr(0, 10), "UQ_DECLARE") ) {
               

               //Remove pragma from output
                 SageInterface::removeStatement( *it );
          }
     }
     for(vector<pair<SgStatement *, string> >::iterator it = vectProcessBB.begin(); it != vectProcessBB.end(); ++it) {
          cout << "Transforming '" << (*it).second
               << "' [" << (*it).first->get_file_info()->get_filenameString() << ":" << (*it).first->get_file_info()->get_line() << "]... " << endl;
          SgStatement *newStatement = isSgStatement( f((*it).first, SageInterface::getScope((*it).first)) );
          if(newStatement != (*it).first) {
               SageInterface::replaceStatement((*it).first, newStatement);
          }
          bbsProcessed++; 
     }
     if(bbsProcessed > 0) {
          SgFunctionDeclaration *funcMain = SageInterface::findMain(project);
          ROSE_ASSERT(funcMain);
          SageInterface::prependStatementList( f.getDeclBlock(), funcMain->get_definition()->get_body() );
     } else {
          cout << "No changes to input" << endl;
          return 1;
     }

     //generatePDF(*project);
     //generateDOT(*project);

     return backend(project);
}
