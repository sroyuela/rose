#ifndef TI_HEADER
#define TI_HEADER

#include <iostream>
#include <set>
#include <boost/algorithm/string.hpp>

#include "rose.h"

using namespace std;

template<class T>
class AstValueAttribute : public AstAttribute {
     private:
          T value;
     public:
          AstValueAttribute(T value) {this->value = value;}
          T getValue() {return this->value;}
          virtual std::string toString() {
                    stringstream ss;
                    ss << "Attribute(Value=" << value << ")";
                    return ss.str();
          }
};

namespace BInterface {
     SgClassType* buildClassType(const SgName &className, SgScopeStatement *currentScope = NULL, SgExpression* optional_fortran_type_kind = NULL);
     SgClassType* buildClassType(SgClassDeclaration *classDecl, SgExpression* optional_fortran_type_kind = NULL);
     SgNode *replaceInitializedName(SgNode *node, SgInitializedName *nameNode, SgScopeStatement *scope);
     SgType *replaceElementType(SgType *type, SgType *newElementType);
     SgAggregateInitializer *buildArrayInitializer(SgExpression *exp, SgType *type, int arraySize);
     SgAggregateInitializer *buildArrayInitializer(vector<SgExpression *> *values, bool useReference);
     SgAggregateInitializer *buildArrayInitializer(set<SgExpression *> *values, bool useReference = false);
     SgExpression *buildConstant(SgType *baseType, long double c);
};

namespace TInterface {

     bool closeProject(SgProject *p);
     bool isParentOf(SgNode *node, SgNode *parent);
     bool isCompilerGenerated(SgLocatedNode *loc);
     bool loadHeaderFile(SgGlobal *globalTarget, string fileName, bool isSystemHeader = false);

     template <class T>
     T GetValueAttribute(SgNode *parentNode, const char *Attribute_ID) {
       AstValueAttribute<T> *attr = static_cast< AstValueAttribute<T> * >(parentNode->getAttribute(Attribute_ID));
       return attr->getValue();          
     }
     bool hasValueAttribute(SgNode *parentNode, const char *Attribute_ID);
     template <class T>
     SgNode *AddValueAttribute(SgNode *parentNode, const char *Attribute_ID, T value) {
          if(TInterface::hasValueAttribute(parentNode, Attribute_ID))
               parentNode->updateAttribute(Attribute_ID, new AstValueAttribute<T>(value));
          else
               parentNode->addNewAttribute(Attribute_ID, new AstValueAttribute<T>(value));
          return parentNode;
     }

     SgMemberFunctionDeclaration *getClassFunction(const SgName &funcName, SgClassDeclaration *classDeclaration, bool caseSensitiveCompare = true);
     SgType *getArrayHighestDimension(SgType *type);
     SgType* getPrimitiveType(SgType* t);
     SgClassType *getTemplateSpecialization(const SgName &className, Rose_STL_Container<SgType *> &templateArguments, SgScopeStatement *currentScope = NULL);
     bool equals(SgNode *n1, SgNode *n2, bool ignoreArrRhs);
     bool changeAllRefs(SgNode *searchTree, SgExpression *orgExp, SgExpression *newExp, bool keepOldExp);

     bool IsConstantDouble(SgExpression *exp, double *d);
     bool IsTerminalVariable(SgExpression *exp);
     bool IsTerminalConstant(SgExpression *exp);
     bool IsUnaryOp(SgExpression *exp, SgExpression **op);
     bool SetUnaryOp(SgExpression *exp, SgExpression *op);
     bool IsBinaryOp(SgExpression *exp, SgExpression **lhs, SgExpression **rhs);
     bool SetBinaryOp(SgExpression *exp, SgExpression *lhs, SgExpression *rhs);
     bool IsNaryOp(SgExpression *exp, SgExprListExp **list);
     bool SetNaryOp(SgExpression *exp, SgExprListExp *list);
     bool IsDeclaration(SgNode *node);
     bool IsIf(SgNode *node, SgNode **cond, SgNode **truebody, SgNode **falsebody);
     bool SetIf(SgNode *node, SgNode *cond, SgNode *truebody, SgNode *falsebody);
     bool IsExitBBStatement(SgNode *node);
     bool IsStatement(SgNode *node);
     bool IsLoop(SgNode *node, SgNode** init, SgNode** cond, SgNode** incr, SgNode** body);
     bool SetLoop(SgNode *node, SgNode* init, SgNode* cond, SgNode* incr, SgNode* body);
     bool IsBlock(SgNode *node, SgStatement **stmBody, SgStatementPtrList **stmBodyList, SgScopeStatement **scope);
     bool SetBlock(SgNode *node, SgStatement *stmBody, SgStatementPtrList *stmBodyList, SgScopeStatement *scope);
};

#endif
