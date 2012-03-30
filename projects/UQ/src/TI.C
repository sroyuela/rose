#include <iostream>
#include <boost/algorithm/string.hpp>

#include "rose.h"
#include "TI.h"

using namespace std;

namespace BInterface {
     SgClassType* buildClassType(const SgName &className, SgScopeStatement *currentScope, SgExpression* optional_fortran_type_kind) { 
       SgClassSymbol *classSym = SageInterface::lookupClassSymbolInParentScopes(className, currentScope);
       ROSE_ASSERT(classSym);
       SgClassType *result = SgClassType::createType(classSym->get_declaration(), optional_fortran_type_kind); 
       ROSE_ASSERT(result); 
       return result;
     }
     SgClassType* buildClassType(SgClassDeclaration *classDecl, SgExpression* optional_fortran_type_kind) { 
       SgClassType *result = SgClassType::createType(classDecl, optional_fortran_type_kind); 
       ROSE_ASSERT(result); 
       return result;
     }

     SgNode *replaceInitializedName(SgNode *node, SgInitializedName *nameNode, SgScopeStatement *scope) {
          SgPntrArrRefExp *arrRef;
          SgDotExp *dotExp;
          SgArrowExp *arrExp;
          SgPointerDerefExp *ptrExp;
          SgCastExp *castExp;

          if(node == NULL)
               return nameNode;

          if (isSgInitializedName(node)) {
               return nameNode;
          } else if ((arrRef = isSgPntrArrRefExp(node)) != NULL) {
               SgExpression *lhsExp = isSgExpression(replaceInitializedName(arrRef->get_lhs_operand(), nameNode, scope));
               ROSE_ASSERT(lhsExp != NULL);
               return SageBuilder::buildBinaryExpression<SgPntrArrRefExp>(lhsExp, arrRef->get_rhs_operand());
          } else if (isSgVarRefExp(node) != NULL) {
               SgNode* parent = node->get_parent();
               if (isSgDotExp(parent) && (isSgDotExp(parent)->get_rhs_operand() == node))
                    return replaceInitializedName(parent, nameNode, scope);
               else if(isSgArrowExp(parent) && (isSgArrowExp(parent)->get_rhs_operand() == node))
                    return replaceInitializedName(parent, nameNode, scope);

               return SageBuilder::buildVarRefExp(nameNode, scope);
          } else if ((dotExp = isSgDotExp(node)) != NULL) {
               SgExpression *lhsExp = isSgExpression(replaceInitializedName(dotExp->get_lhs_operand(), nameNode, scope));
               ROSE_ASSERT(lhsExp != NULL);
               return SageBuilder::buildBinaryExpression<SgDotExp>(lhsExp, dotExp->get_rhs_operand());
          } else if ((arrExp = isSgArrowExp(node)) != NULL) {
               SgExpression *lhsExp = isSgExpression(replaceInitializedName(arrExp->get_lhs_operand(), nameNode, scope));
               ROSE_ASSERT(lhsExp != NULL);
               return SageBuilder::buildBinaryExpression<SgDotExp>(lhsExp, arrExp->get_rhs_operand());
          } else if ((ptrExp = isSgPointerDerefExp(node)) != NULL) {
               SgExpression *exp = isSgExpression(replaceInitializedName(ptrExp->get_operand(), nameNode, scope));
               ROSE_ASSERT(exp != NULL);
               return SageBuilder::buildUnaryExpression<SgPointerDerefExp>(exp);
          } else if ((castExp = isSgCastExp(node)) != NULL) {
               SgExpression *exp = isSgExpression(replaceInitializedName(castExp->get_operand(), nameNode, scope));
               ROSE_ASSERT(exp != NULL);
               return SageBuilder::buildCastExp(exp, exp->get_type(), castExp->get_cast_type());
          } else {
               cerr<<"In BInterface::replaceInitializedName(): unhandled reference type: " << node->class_name() << endl;
               ROSE_ASSERT(false);
          }
     }

     SgType *replaceElementType(SgType *type, SgType *newElementType) {
          if(type == NULL)
               return NULL;

          switch(type->variantT()) {
           //Composite types...
           case V_SgArrayType: {
               SgArrayType *arrType = isSgArrayType(type);
               return SageBuilder::buildArrayType(replaceElementType(arrType->get_base_type(), newElementType), arrType->get_index());
             } break;                                        
           case V_SgPointerType: {
               SgPointerType *ptrType = isSgPointerType(type);
               return SageBuilder::buildPointerType(replaceElementType(ptrType->get_base_type(), newElementType));
             } break;
           case V_SgModifierType: {
               SgModifierType *modType = isSgModifierType(type);
               return SageBuilder::buildModifierType(replaceElementType(modType->get_base_type(), newElementType));
             } break;
           case V_SgReferenceType: {
               SgReferenceType *refType = isSgReferenceType(type);
               return SageBuilder::buildReferenceType(replaceElementType(refType->get_base_type(), newElementType));
             } break;
           case V_SgTypeComplex: {
               SgTypeComplex *complexType = isSgTypeComplex(type);
               return SageBuilder::buildComplexType(replaceElementType(complexType->get_base_type(), newElementType));
             } break;
           case V_SgTypeImaginary: {
               SgTypeImaginary *imType = isSgTypeImaginary(type);
               return SageBuilder::buildImaginaryType(replaceElementType(imType->get_base_type(), newElementType));
             } break;
           //Element types...
           case V_SgTypeBool:
           case V_SgTypeChar:
 	      case V_SgTypeDouble:
           case V_SgTypeFloat:
           case V_SgTypeInt:
           case V_SgTypeLong:
           case V_SgTypeLongDouble:
           case V_SgTypeLongLong:
           case V_SgTypeShort:
           case V_SgTypeString:
           case V_SgTypeVoid:
           case V_SgTypeWchar:
           case V_SgTypeSignedChar:
           case V_SgTypeSignedInt:
           case V_SgTypeSignedLong:
           case V_SgTypeSignedLongLong:
           case V_SgTypeSignedShort:
           case V_SgTypeUnsignedChar:
           case V_SgTypeUnsignedInt:
           case V_SgTypeUnsignedLong:
           case V_SgTypeUnsignedLongLong:
           case V_SgTypeUnsignedShort:
           case V_SgTypeUnknown:
               return newElementType;
           default:
               cout << "Unhandled type '" << type->class_name() << "' in replaceElementType()." << endl;
               ROSE_ASSERT(false);
          }
     }

     SgAggregateInitializer *buildArrayInitializer(vector<SgExpression *> *values, bool useReference) {
          if(values == NULL)
               return NULL;
          if(values->size() == 0)
               return NULL;

          SgType *type = (*(values->begin()))->get_type();
          if(useReference == false)
               return SageBuilder::buildAggregateInitializer(SageBuilder::buildExprListExp(*values), type);
          else {
               std::vector< SgExpression * > exprVec;
               for(vector<SgExpression *>::iterator it = values->begin();
                   it != values->end();
                   ++it)
                    exprVec.push_back( SageBuilder::buildAddressOfOp( *it ) );
               return SageBuilder::buildAggregateInitializer( SageBuilder::buildExprListExp(exprVec), type);
          }  
     }

     SgAggregateInitializer *buildArrayInitializer(set<SgExpression *> *values, bool useReference) {
          if(values == NULL)
               return NULL;
          if(values->size() == 0)
               return NULL;

          SgType *type = (*(values->begin()))->get_type();
          if(useReference == false) {
               std::vector< SgExpression * > expList = std::vector< SgExpression * >(values->begin(), values->end());
               return SageBuilder::buildAggregateInitializer(SageBuilder::buildExprListExp(expList), type);
          } else {
               std::vector< SgExpression * > exprVec;
               for(set<SgExpression *>::iterator it = values->begin();
                   it != values->end();
                   ++it)
                    exprVec.push_back( SageBuilder::buildAddressOfOp( *it ) );
               return SageBuilder::buildAggregateInitializer( SageBuilder::buildExprListExp(exprVec), type);
          }               
     }

     SgAggregateInitializer *buildArrayInitializer(SgExpression *exp, SgType *type, int arraySize) {
          return SageBuilder::buildAggregateInitializer( SageBuilder::buildExprListExp(std::vector< SgExpression * >(arraySize, exp)),type );
     }

     SgExpression *buildConstant(SgType *baseType, long double c) {
          if(baseType == NULL)
               return NULL;
          switch(baseType->variantT()) {
           case V_SgTypeUnsignedShort:    return SageBuilder::buildUnsignedShortVal( (unsigned short) round(c) );
           //case V_SgTypeUnsignedLongLong: return SageBuilder::buildUnsignedLongLongVal( (unsigned long long) round(c) );
           case V_SgTypeUnsignedInt:      return SageBuilder::buildUnsignedIntVal( (unsigned int) round(c) );
           case V_SgTypeUnsignedChar:     return SageBuilder::buildUnsignedCharVal( (c < -128.0 ? (unsigned char) -128 : (c>127.0 ? (unsigned char) 127 : (unsigned char) round(c)) ) );
           //case V_SgTypeSignedShort:    return SageBuilder::buildSignedShortVal( (signed short) round(c) );
           //case V_SgTypeSignedLongLong: return SageBuilder::buildSignedLongLongVal( (signed long long) round(c) );
           //case V_SgTypeSignedInt:      return SageBuilder::buildSignedIntVal( (signed int) round(c) );
           //case V_SgTypeSignedChar:     return SageBuilder::buildSignedCharVal( (c < -128.0 ? (signed char) -128 : (c>127.0 ? (signed char) 127 : (signed char) round(c)) ) );
           case V_SgTypeShort:          return SageBuilder::buildShortVal( (short) round(c) );
           case V_SgTypeLongLong:       return SageBuilder::buildLongLongIntVal( (long long) round(c) );
           case V_SgTypeLongDouble:     return SageBuilder::buildLongDoubleVal(c);
           case V_SgTypeLong:           return SageBuilder::buildLongIntVal( (long) round(c) );
           case V_SgTypeInt:            return SageBuilder::buildIntVal( (int) round(c) );
           case V_SgTypeImaginary:      return SageBuilder::buildImaginaryVal(c);
           case V_SgTypeFloat:          return SageBuilder::buildFloatVal( (float) c);
           case V_SgTypeDouble:         return SageBuilder::buildDoubleVal( (double) c);
           case V_SgTypeComplex:        return SageBuilder::buildComplexVal(SageBuilder::buildDoubleVal(c), SageBuilder::buildDoubleVal(c));
           case V_SgTypeChar:           return SageBuilder::buildCharVal( (c > 255.0 ? (char) 255 : (char) round(c)) );
           case V_SgTypeBool:           return SageBuilder::buildBoolValExp( (c > 0 ? true : false) );
           case V_SgTypeCrayPointer:  
           case V_SgPointerType:   return NULL; //TODO!
           default:
               //Only for primitives
               assert(false);
               return NULL;
          }
     }
};

namespace TInterface {

     bool closeProject(SgProject *p) {
          //Anonymous visitor definition
            vector<SgNode *> collectedNodes;
            struct Collector : ROSE_VisitTraversal {
                        vector<SgNode *> &colNodes;
                        Collector(vector<SgNode *> &colN) : colNodes(colN) {}
                        void visit (SgNode* node) {colNodes.push_back(node);}
            };
            Collector c(collectedNodes);
            c.traverseMemoryPool();
          //Delete all nodes...
            for(vector<SgNode *>::iterator it = collectedNodes.begin();
                it != collectedNodes.end();
                ++it)
                    delete *it;

          return true;
     }

     bool isParentOf(SgNode *node, SgNode *parent) {
          if(node == NULL || parent == NULL)
               return false;
          for(SgNode *par = node->get_parent(); par; par = par->get_parent())
               if(par == parent)
                    return true;
          return false;
     }


     bool isCompilerGenerated(SgLocatedNode *loc) {
          if(loc->get_file_info() != NULL)
             if( (loc->get_file_info()->get_line() == 0) &&
                 (loc->get_file_info()->get_col() == 0) &&
                 (loc->get_file_info()->get_file_id() < 0) )
                  return true;
          return false;
     }

     bool loadHeaderFile(SgGlobal *globalTarget, string fileName, bool isSystemHeader) {
          //Build temporary file =)
            char *tmpFilename = "/tmp/tmpfileROSE.C";
            ofstream tmpF( tmpFilename );
            if(isSystemHeader)
               tmpF << "#include <" << fileName << ">" << endl;
            else
               tmpF << "#include \"" << fileName << "\"" << endl;   
            tmpF.close();
          //Open frontend...
            vector<string> vCmd;
            vCmd.push_back("");
            SgProject *proj = SageInterface::getProject();
            if(proj != NULL) {
               //Add include directories...
                 for(SgStringList::iterator it = proj->get_includeDirectorySpecifierList().begin(); it != proj->get_includeDirectorySpecifierList().end(); ++it)
                    vCmd.push_back(*it);
            }
            vCmd.push_back(tmpFilename);
            //for(vector<string>::iterator it = vCmd.begin(); it != vCmd.end(); ++it)
            //    cout << "CMD: " << *it << endl;
            SgProject *incProject = frontend(vCmd);
            if(incProject == NULL)
               return false;
          //Put contents of global symbol table over to 
            SgGlobal *globalInc = SageInterface::getFirstGlobalScope(incProject);

            //Copy type table...
              std::set<SgNode *> symTable = globalInc->get_type_table()->get_type_table()->get_symbols();
              for(std::set<SgNode *>::iterator it = symTable.begin(); it != symTable.end(); ++it)
                    switch( (*it)->variantT() ) {
                     case V_SgFunctionTypeSymbol: {
                         SgFunctionTypeSymbol *funcSym = isSgFunctionTypeSymbol( *it );
                         globalTarget->get_type_table()->insert_type(funcSym->get_name(),funcSym->get_type());
                       } break;
                     default:
                         return false;
                    }
            //Copy symbol table...
            for(SgSymbol *sym = globalInc->first_any_symbol(); sym; sym = globalInc->next_any_symbol()) {
               switch(sym->variantT()) {
                case V_SgAliasSymbol:
                case V_SgAsmBinaryAddressSymbol:
                case V_SgAsmBinaryDataSymbol:
                case V_SgDefaultSymbol:
                case V_SgFunctionTypeSymbol:
                    break;
                case V_SgJavaLabelSymbol:
                case V_SgLabelSymbol:
                case V_SgNamespaceSymbol: 
                    break;
                    //ROSE_ASSERT(false);

                case V_SgClassSymbol: {
                    SgClassSymbol *classSym = isSgClassSymbol(sym);
                    if( isCompilerGenerated(classSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgCommonSymbol: {
                    SgCommonSymbol *commonSym = isSgCommonSymbol(sym);
                    if( isCompilerGenerated(commonSym->get_declaration()) )
                         continue;                  
                 } break;
                
                case V_SgEnumFieldSymbol: {
                    SgEnumFieldSymbol *enumFSym = isSgEnumFieldSymbol(sym);
                    if( isCompilerGenerated(enumFSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgEnumSymbol: {
                    SgEnumSymbol *enumSym = isSgEnumSymbol(sym);
                    if( isCompilerGenerated(enumSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgFunctionSymbol: {
                    SgFunctionSymbol *funcSym = isSgFunctionSymbol(sym);
                    if( isCompilerGenerated(funcSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgMemberFunctionSymbol: {
                    SgMemberFunctionSymbol *memberFSym = isSgMemberFunctionSymbol(sym);
                    if( isCompilerGenerated(memberFSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgRenameSymbol: {
                    SgRenameSymbol *renSym = isSgRenameSymbol(sym);
                    if( isCompilerGenerated(renSym->get_declaration()) )
                         continue;                  
                 } break;

                case V_SgInterfaceSymbol: {
                    SgInterfaceSymbol *intSym = isSgInterfaceSymbol(sym);
                    if( isCompilerGenerated(intSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgIntrinsicSymbol: {
                    SgIntrinsicSymbol *intSym = isSgIntrinsicSymbol(sym);
                    if( isCompilerGenerated(intSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgModuleSymbol: {
                    SgModuleSymbol *modSym = isSgModuleSymbol(sym);
                    if( isCompilerGenerated(modSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgTemplateSymbol: {
                    SgTemplateSymbol *templSym = isSgTemplateSymbol(sym);
                    if( isCompilerGenerated(templSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgTypedefSymbol: {
                    SgTypedefSymbol *typeSym = isSgTypedefSymbol(sym);
                    if( isCompilerGenerated(typeSym->get_declaration()) )
                         continue;                  
                 } break;
                case V_SgVariableSymbol: {
                    SgVariableSymbol *varSym = isSgVariableSymbol(sym);
                    if( isCompilerGenerated(varSym->get_declaration()) )
                         continue;                  
                 } break;
                default:
                    cout << "Unhandled symbol '" << sym->class_name() << "'" << endl;
                    ROSE_ASSERT(false);
               } 
               globalTarget->insert_symbol(sym->get_name(), sym);
            }
          //Kill new frontend...
            //SageInterface::deleteAST(incProject);
          //Kill file...

          return true;
     }

     bool hasValueAttribute(SgNode *parentNode, const char *Attribute_ID) {
          return parentNode->attributeExists(Attribute_ID);
     }

     SgMemberFunctionDeclaration *getClassFunction(const SgName &funcName, SgClassDeclaration *classDeclaration, bool caseSensitiveCompare) {
          ROSE_ASSERT(classDeclaration != NULL);
          SgDeclarationStatement* definingDeclaration = classDeclaration->get_definingDeclaration();
          ROSE_ASSERT(definingDeclaration != NULL);
          SgClassDeclaration* definingClassDeclaration = isSgClassDeclaration(definingDeclaration);

          if(definingClassDeclaration == NULL)
               return NULL;
          SgClassDefinition* classDefinition = definingClassDeclaration->get_definition();
          ROSE_ASSERT(classDefinition != NULL);

          for(Rose_STL_Container<SgDeclarationStatement*>::iterator it = classDefinition->get_members().begin();
              it != classDefinition->get_members().end();
              ++it) {
               // Check the parent pointer to make sure it is properly set
                  ROSE_ASSERT( (*it)->get_parent() != NULL);
                  ROSE_ASSERT( (*it)->get_parent() == classDefinition);

                  SgMemberFunctionDeclaration* memberFunction = isSgMemberFunctionDeclaration(*it);
                  if (memberFunction != NULL) {
                         if(caseSensitiveCompare &&
                            (memberFunction->get_name().getString() == funcName.getString()) )
                              return memberFunction;
                         else if(!caseSensitiveCompare &&
                                 boost::iequals(memberFunction->get_name().getString(), funcName.getString()) )
                              return memberFunction;
                  }
             }
          return NULL;
     }

     SgType *getArrayHighestDimension(SgType *type) {
          if(type == NULL)
               return NULL;

          SgType *result;
          switch(type->variantT()) {
           case V_SgArrayType:
               if((result = getArrayHighestDimension(isSgArrayType(type)->get_base_type()) ) == NULL)
                    return type;
               return result;
           default:
               return NULL;
          }
     }
     SgType* getPrimitiveType(SgType* type) {
          if(type == NULL)
               return NULL;
          switch(type->variantT()) {
           case V_SgArrayType:                    return getPrimitiveType(static_cast<SgArrayType *>(type)->get_base_type());
           case V_SgFunctionType:                 return type;
           case V_SgMemberFunctionType:           return type;
           case V_SgPartialFunctionType:          return type;
           case V_SgPartialFunctionModifierType:  return type;
           case V_SgModifierType:                 return getPrimitiveType(static_cast<SgModifierType *>(type)->get_base_type());
           case V_SgNamedType:                    return type;
           case V_SgClassType:                    return type;
           case V_SgEnumType:                     return type;
           case V_SgJavaParameterizedType:        return getPrimitiveType(static_cast<SgJavaParameterizedType *>(type)->get_raw_type());
           case V_SgTypedefType:                  return getPrimitiveType(static_cast<SgTypedefType *>(type)->get_base_type());
           case V_SgPointerType:                  return getPrimitiveType(static_cast<SgPointerType *>(type)->get_base_type());
           case V_SgPointerMemberType:            return getPrimitiveType(static_cast<SgPointerMemberType *>(type)->get_class_type());
           case V_SgQualifiedNameType:            return getPrimitiveType(static_cast<SgQualifiedNameType *>(type)->get_base_type());
           case V_SgReferenceType:                return getPrimitiveType(static_cast<SgReferenceType *>(type)->get_base_type());
           case V_SgTemplateType:                 return type;
           case V_SgTypeBool:                     return type;
           case V_SgTypeCAFTeam:                  return type;
           case V_SgTypeChar:                     return type;
           case V_SgTypeComplex:                  return getPrimitiveType(static_cast<SgTypeComplex *>(type)->get_base_type());
           case V_SgTypeCrayPointer:              return type;
           case V_SgTypeDefault:                  return type;
           case V_SgTypeDouble:                   return type;
           case V_SgTypeEllipse:                  return type;
           case V_SgTypeFloat:                    return type;
           case V_SgTypeGlobalVoid:               return type;
           case V_SgTypeImaginary:                return getPrimitiveType(static_cast<SgTypeImaginary *>(type)->get_base_type());
           case V_SgTypeInt:                      return type;
           case V_SgTypeLabel:                    return type;
           case V_SgTypeLong:                     return type;
           case V_SgTypeLongDouble:               return type;
           case V_SgTypeLongLong:                 return type;
           case V_SgTypeShort:                    return type;
           case V_SgTypeSignedChar:               return type;
           case V_SgTypeSignedInt:                return type;
           case V_SgTypeSignedLong:               return type;
           case V_SgTypeSignedLongLong:           return type;
           case V_SgTypeSignedShort:              return type;
           case V_SgTypeString:                   return type;
           case V_SgTypeUnknown:                  return type;
           case V_SgTypeUnsignedChar:             return type;
           case V_SgTypeUnsignedInt:              return type;
           case V_SgTypeUnsignedLong:             return type;
           case V_SgTypeUnsignedLongLong:         return type;
           case V_SgTypeUnsignedShort:            return type;
           case V_SgTypeVoid:                     return type;
           case V_SgTypeWchar:                    return type;
           default:
               cout << "Unhandled type in getPrimitiveType(): '" << type->class_name() << "'" << endl;
               ROSE_ASSERT(false);
          }
     }

     SgClassType *getTemplateSpecialization(const SgName &className, Rose_STL_Container<SgType *> &templateArguments, SgScopeStatement *currentScope) {
          //Create template argument list
            string classDeclName = className.getString() + " < ";
            unsigned int typeIndex = 0;
            for(Rose_STL_Container<SgType *>::iterator it = templateArguments.begin();
                it != templateArguments.end();
                ++it, typeIndex++) {
                    classDeclName.append( (*it)->unparseToString() );
                    if(typeIndex != templateArguments.size()-1)
                         classDeclName.append(" , ");
            }
            classDeclName.append(" > ");
            //cout << "Type resolved to '" << classDeclName << "'" << endl;
          //Search for class...
            for(SgScopeStatement *curScope = (currentScope == NULL ? SageBuilder::topScopeStack() : currentScope);
                curScope != NULL;
                ) {
                    SgClassSymbol *classSym;
                    for(classSym = curScope->first_class_symbol(); classSym; classSym = curScope->next_class_symbol())
                         if(classSym->get_name().getString() == classDeclName) {
                              //cout << "[" << index << " - '" << classSym->get_name().getString() << "'] " << endl;
                              SgClassDeclaration *classDecl = isSgClassDeclaration(classSym->get_declaration());
                              ROSE_ASSERT(classDecl);
                              return SgClassType::createType( classDecl );
                         }

            if (curScope->get_parent() != NULL) // avoid calling get_scope when parent is not set
               curScope = isSgGlobal(curScope) ? NULL : curScope->get_scope();
            else
               curScope = NULL;
          }
            
          //Create object
            return NULL;              
     }           

     bool equals(SgNode *n1, SgNode *n2, bool ignoreArrRhs) {
          if(n1 == n2)
               return true;
          if(n1 == NULL)
               return false;
          if(n2 == NULL)
               return false;
          if(n1->variantT() != n2->variantT())
               return false;

          switch(n1->variantT()) {
           case V_SgVarRefExp: {
              SgVarRefExp *vrA = isSgVarRefExp(n1), *vrB = isSgVarRefExp(n2);
              return ( equals(vrA->get_type(), vrB->get_type(), ignoreArrRhs) &&
                       (vrA->get_symbol()->get_name().getString() == vrB->get_symbol()->get_name().getString()) );
            } break;
           case V_SgPntrArrRefExp: {
               //Only care about the base of the expression!
               SgPntrArrRefExp *arrA = isSgPntrArrRefExp(n1), *arrB = isSgPntrArrRefExp(n2);
               return ( equals(arrA->get_lhs_operand_i(), arrB->get_lhs_operand_i(), ignoreArrRhs) &&
                        (ignoreArrRhs || equals(arrA->get_rhs_operand_i(), arrB->get_rhs_operand_i(), ignoreArrRhs)) );
            } break;
           case V_SgSubtractOp: {
              SgBinaryOp *bopA = isSgBinaryOp(n1), *bopB = isSgBinaryOp(n2);
              return ( equals(bopA->get_type(), bopB->get_type(), ignoreArrRhs) &&
                       equals(bopA->get_lhs_operand_i(), bopB->get_lhs_operand_i(), ignoreArrRhs) &&
                       equals(bopA->get_rhs_operand_i(), bopB->get_rhs_operand_i(), ignoreArrRhs) );
            } break;
           case V_SgTypeUnknown:
               return true;
           case V_SgPointerType: {
               SgPointerType *ptrA = isSgPointerType(n1), *ptrB = isSgPointerType(n2);
               return ( equals(ptrA->get_base_type(), ptrB->get_base_type(), ignoreArrRhs) );
            } break;               
           case V_SgArrayType: {
               SgArrayType *arrA = isSgArrayType(n1), *arrB = isSgArrayType(n2);
               return ( equals(arrA->get_base_type(), arrB->get_base_type(), ignoreArrRhs) &&
                        equals(arrA->get_index(), arrB->get_index(), ignoreArrRhs) &&
                        equals(arrA->get_dim_info(), arrB->get_dim_info(), ignoreArrRhs) );
            } break;
           case V_SgTypeInt:
           case V_SgTypeFloat: {
              SgType *tA = isSgType(n1), *tB = isSgType(n2);
              return ( !(tA->isUnsignedType() ^ tB->isUnsignedType()) &&
                       !(tA->isIntegerType() ^ tB->isIntegerType()) &&
                       !(tA->isFloatType() ^ tB->isFloatType()) );
            } break;
           case V_SgCastExp: {
               SgCastExp *e1 = isSgCastExp(n1), *e2 = isSgCastExp(n2);
               return ( equals(e1->get_type(), e2->get_type(), ignoreArrRhs) &&
                        (e1->get_cast_type() == e2->get_cast_type()) &&
                        equals(e1->get_originalExpressionTree(), e2->get_originalExpressionTree(), ignoreArrRhs) );
                        
            } break;
           case V_SgIntVal: {
               SgIntVal *e1 = isSgIntVal(n1), *e2 = isSgIntVal(n2);
               return (equals(e1->get_type(), e2->get_type(), ignoreArrRhs) &&
                       e1->get_value() == e2->get_value());
            } break;
           default:
               cout << "Unhandle case in equals for '" << n1->class_name() << "'" << endl;
               ROSE_ASSERT(false);
               return false;
          }
     }

     bool changeAllRefs(SgNode *searchTree, SgExpression *orgExp, SgExpression *newExp, bool keepOldExp) {
          if((searchTree == NULL) || (orgExp == NULL) || (newExp == NULL))
                    return false;
          
          NodeQuerySynthesizedAttributeType vars = NodeQuery::querySubTree(searchTree, orgExp->variantT());
          for(NodeQuerySynthesizedAttributeType::const_iterator it = vars.begin();
              it != vars.end();
              ++it) {
               //Make sure the element is correct...
                 switch(orgExp->variantT()) {
                  case V_SgVarRefExp:        if( !equals(isSgVarRefExp(orgExp), isSgVarRefExp(*it), false) )          continue;     break;
                  case V_SgPntrArrRefExp:    if( !equals(isSgPntrArrRefExp(orgExp), isSgPntrArrRefExp(*it), false) )  continue;     break;
                  default:
                    cout << "Unrecognized type '" << orgExp->class_name() << "'." << endl;
                    continue;
                 }
               //Replace expressions (*it) & newExp 
                 SgExpression *expLHS = isSgExpression(*it);
                 //cout << "Changing '" << expLHS->unparseToString() << "' to '" << newExp->unparseToString() << "'" << endl;
                 SageInterface::replaceExpression( expLHS, newExp, keepOldExp );
          }
          return true;
     }

     bool IsConstantDouble(SgExpression *exp, double *d) {
          if(exp == NULL)
               return false;
          switch(exp->variantT()) {
           case V_SgFloatVal: 
               if(d)    *d = (double) static_cast<SgFloatVal *>(exp)->get_value();
               break;
           case V_SgDoubleVal:
               if(d)    *d = static_cast<SgDoubleVal *>(exp)->get_value();
               break;
           default:
               return false;
          }
          return true;
     }

     bool IsTerminalVariable(SgExpression *exp) {
          if(exp == NULL)
               return false;
          switch(exp->variantT()) {
               case V_SgDotExp:
               case V_SgArrowExp:
               case V_SgPointerDerefExp:
               case V_SgCastExp:
               case V_SgPntrArrRefExp:
               case V_SgClassNameRefExp:
               case V_SgFunctionRefExp:
               case V_SgLabelRefExp:
               case V_SgMemberFunctionRefExp:
               case V_SgThisExp:
               case V_SgVarRefExp:
                    return true;
               default:
                    return false;              
          }
     }
     bool IsTerminalConstant(SgExpression *exp) {
          if(exp == NULL)
               return false;
          switch(exp->variantT()) {
               case V_SgBoolValExp:
               case V_SgCharVal:
               case V_SgComplexVal:
               case V_SgDoubleVal:
               case V_SgEnumVal:
               case V_SgFloatVal:
               case V_SgIntVal:
               case V_SgLongDoubleVal:
               case V_SgLongLongIntVal:
               case V_SgShortVal:
               case V_SgStringVal:
               case V_SgUnsignedCharVal:
               case V_SgUnsignedIntVal:
               case V_SgUnsignedLongLongIntVal:
               case V_SgUnsignedLongVal:
               case V_SgUnsignedShortVal:
               case V_SgUpcMythread:
               case V_SgUpcThreads:
               case V_SgWcharVal:
                    return true;
               default:
                    return false;              
          }
     }
     bool IsUnaryOp(SgExpression *exp, SgExpression **op) {
          SgUnaryOp *uExp = isSgUnaryOp(exp);
          if(uExp != NULL) {
               if(op)   *op = uExp->get_operand_i();
               return true;
          }
          return false;
     }
     bool SetUnaryOp(SgExpression *exp, SgExpression *op) {
          SgUnaryOp *uExp = isSgUnaryOp(exp);
          if(uExp != NULL) {
               if(op)   uExp->set_operand_i(op);
               return true;
          }
          return false;
     }

     bool IsBinaryOp(SgExpression *exp, SgExpression **lhs, SgExpression **rhs) {
          SgBinaryOp *binExp = isSgBinaryOp(exp);
          if(binExp != NULL) {
               if(lhs)   *lhs = binExp->get_lhs_operand();
               if(rhs)   *rhs = binExp->get_rhs_operand();
               return true;
          }
          return false;
     }
     bool SetBinaryOp(SgExpression *exp, SgExpression *lhs, SgExpression *rhs) {
          SgBinaryOp *binExp = isSgBinaryOp(exp);
          if(binExp != NULL) {
               if(lhs)   binExp->set_lhs_operand(lhs);
               if(rhs)   binExp->set_rhs_operand(rhs);
               return true;
          }
          return false;
     }

     bool IsNaryOp(SgExpression *exp, SgExprListExp **list) {
          switch(exp->variantT()) {
           case V_SgFunctionCallExp: {
                    SgFunctionCallExp *e = isSgFunctionCallExp(exp);
                    if(list)  *list = e->get_args();
                    return true;
               } break;
           case V_SgConditionalExp: {
                    SgConditionalExp *e = isSgConditionalExp(exp);
                    if(list)  *list = SageBuilder::buildExprListExp(
                                        e->get_conditional_exp(),
                                        e->get_true_exp(),
                                        e->get_false_exp());
                    return true;
               } break; 
           default:
               break;
          }
          return false;
     }
     bool SetNaryOp(SgExpression *exp, SgExprListExp *list) {
          if(exp == NULL)
               return false;
          switch(exp->variantT()) {
           case V_SgFunctionCallExp: {
                    SgFunctionCallExp *e = isSgFunctionCallExp(exp);
                    if(list)  e->set_args(list);
                    return true;
               } break;
           case V_SgConditionalExp: {
                    SgConditionalExp *e = isSgConditionalExp(exp);
                    if(list && list->get_expressions().size() == 3) {
                         e->set_conditional_exp(list->get_expressions().at(0));
                         e->set_true_exp(list->get_expressions().at(1));
                         e->set_false_exp(list->get_expressions().at(2));
                    }
                    return true;
               } break; 
           default:
               break;
          }
          return false;
     }

     bool IsDeclaration(SgNode *node) {
          if(node == NULL)
               return false;
          /*SgExprListExp *expList = NULL;
          switch(node->variantT()) {
           case V_SgVariableDeclaration:
                    break;
           default:
               break;
          }*/
          if(isSgDeclarationStatement(node) != NULL) {
               return true;
          } else
               return false;
     }
     bool IsIf(SgNode *node, SgNode **cond, SgNode **truebody, SgNode **falsebody) { 
       switch (node->variantT()) {
        case V_SgIfStmt: {
            SgIfStmt *is = isSgIfStmt(node);
            if (cond != NULL)      *cond = is->get_conditional();
            if (truebody != NULL)  *truebody = is->get_true_body();
            if (falsebody != NULL) *falsebody = is->get_false_body();
          } break;
       case V_SgCaseOptionStmt: {
           SgCaseOptionStmt* cs = isSgCaseOptionStmt(node);
           if (cond != NULL)       *cond = cs->get_key();
           if (truebody != NULL)   *truebody = cs->get_body();
           if (falsebody != NULL)  *falsebody = NULL;
          } break;
       case V_SgDefaultOptionStmt: {
           SgDefaultOptionStmt *def = isSgDefaultOptionStmt(node);
           if (cond != NULL)       *cond = NULL;
           if (truebody != NULL)   *truebody = def->get_body();
           if (falsebody != NULL)  *falsebody = NULL;
          } break;
       default:
           return false;
       }
       return true;
     }
     bool SetIf(SgNode *node, SgNode *cond, SgNode *truebody, SgNode *falsebody) { 
       switch (node->variantT()) {
        case V_SgIfStmt: {
            SgIfStmt *is = isSgIfStmt(node);
            if (cond != NULL)      is->set_conditional( isSgStatement(cond) );
            if (truebody != NULL)  is->set_true_body( isSgStatement(truebody) );
            if (falsebody != NULL) is->set_false_body( isSgStatement(falsebody) );
          } break;
       case V_SgCaseOptionStmt: {
           SgCaseOptionStmt* cs = isSgCaseOptionStmt(node);
           if (cond != NULL)       cs->set_key( isSgExpression(cond) );
           if (truebody != NULL)   cs->set_body( isSgStatement(truebody) );
          } break;
       case V_SgDefaultOptionStmt: {
           SgDefaultOptionStmt *def = isSgDefaultOptionStmt(node);
           if (truebody != NULL)   def->set_body( isSgStatement(truebody) );
          } break;
       default:
           return false;
       }
       return true;
     }

     bool IsExitBBStatement(SgNode *node) {
          return ((isSgBreakStmt(node) != NULL) ||
                  (isSgGotoStatement(node) != NULL) ||
                  (isSgReturnStmt(node) != NULL) ||
                  (isSgContinueStmt(node) != NULL));
     }
     bool IsStatement(SgNode *node) {
         return isSgStatement(node) != 0;
     }
     bool IsLoop(SgNode *node, SgNode** init, SgNode** cond, SgNode** incr, SgNode** body) {
          if(node == NULL)
               return false;

          switch (node->variantT()) {
           case V_SgForStatement: {
               SgForStatement *f = isSgForStatement(node);
               if (init != NULL) *init = f->get_for_init_stmt();
               if (incr != NULL) *incr = f->get_increment();
               if (cond != NULL) *cond = f->get_test();
               if (body != NULL) *body = f->get_loop_body();
             } break;
           case V_SgWhileStmt: {
               SgWhileStmt* w = isSgWhileStmt(node);
               if (init != NULL) *init = NULL;
               if (incr != NULL) *incr = NULL;
               if (cond != NULL) *cond = w->get_condition();
               if (body != NULL) *body = w->get_body();
             } break;
           case V_SgDoWhileStmt: {
               SgDoWhileStmt *w = isSgDoWhileStmt(node);
               if (init != 0) *init = NULL;
               if (incr != 0) *incr = NULL;
               if (cond != 0) *cond = w->get_condition();
               if (body != 0) *body = w->get_body();
             } break;
           case V_SgFortranDo: {
               SgFortranDo *f = isSgFortranDo(node);
               if (init != 0) *init = f->get_initialization();
               if (incr != 0) *incr = f->get_increment();
               if (cond != 0) *cond = f->get_bound();
               if (body != 0) *body = f->get_body();
             } break;
           default:
               return false;
          }
          return true;
     }
     bool SetLoop(SgNode *node, SgNode* init, SgNode* cond, SgNode* incr, SgNode* body) {
          if(node == NULL)
               return false;

          switch (node->variantT()) {
           case V_SgForStatement: {
               SgForStatement *f = isSgForStatement(node);
               if (isSgForInitStatement(init) != NULL) f->set_for_init_stmt( isSgForInitStatement(init) );
               if (isSgExpression(incr) != NULL) f->set_increment( isSgExpression(incr) );
               if (isSgStatement(cond) != NULL) f->set_test( isSgStatement(cond) );
               if (isSgStatement(body) != NULL) {
                    f->set_loop_body( isSgStatement(body) );
                    body->set_parent(f);
               }
             } break;
           case V_SgWhileStmt: {
               SgWhileStmt* w = isSgWhileStmt(node);
               if (isSgStatement(cond) != NULL) w->set_condition( isSgStatement(cond) );
               if (isSgStatement(body) != NULL) w->set_body( isSgStatement(body) );
             } break;
           case V_SgDoWhileStmt: {
               SgDoWhileStmt *w = isSgDoWhileStmt(node);
               if (isSgStatement(cond) != 0) w->set_condition( isSgStatement(cond) );
               if (isSgStatement(body) != 0) w->set_body( isSgStatement(body) );
             } break;
           case V_SgFortranDo: {
               SgFortranDo *f = isSgFortranDo(node);
               if (isSgExpression(init) != 0) f->set_initialization( isSgExpression(init) );
               if (isSgExpression(incr) != 0) f->set_increment( isSgExpression(incr) );
               if (isSgExpression(cond) != 0) f->set_bound( isSgExpression(cond) );
               if (isSgBasicBlock(body) != 0) f->set_body( isSgBasicBlock(body) );
             } break;
           default:
               return false;
          }
          return true;
     }
     bool IsBlock(SgNode *node, SgStatement **stmBody, SgStatementPtrList **stmBodyList, SgScopeStatement **scope) {  
          switch (node->variantT()) {
           case V_SgBasicBlock: {
               SgBasicBlock *bb = isSgBasicBlock(node);
               if(stmBodyList) *stmBodyList = &bb->get_statements();
               if(stmBody) *stmBody = NULL;
               if(scope) *scope = bb;
             } break;
           case V_SgSwitchStatement: {
               SgSwitchStatement *switchStm = isSgSwitchStatement(node);
               if(stmBodyList) *stmBodyList = NULL;
               if(stmBody) *stmBody = switchStm->get_body();
               if(scope) *scope = switchStm;
             } break;
           case V_SgForInitStatement: {
               SgForInitStatement *forInitStm = isSgForInitStatement(node);
               if(stmBodyList) *stmBodyList = &forInitStm->get_init_stmt();
               if(stmBody) *stmBody = NULL;
               if(scope) *scope = NULL;
             } break;
           default:
               return false;
          };
          return true;
     }
     bool SetBlock(SgNode *node, SgStatement *stmBody, SgStatementPtrList *stmBodyList, SgScopeStatement *scope) {  
          switch (node->variantT()) {
           case V_SgBasicBlock: {
               SgBasicBlock *bb = isSgBasicBlock(node);
               if( (stmBodyList != NULL) && (stmBodyList != &bb->get_statements()) ) {
                    bb->get_statements().clear();
                    for(SgStatementPtrList::iterator it = stmBodyList->begin();
                        it != stmBodyList->end();
                        ++it) {
                              bb->get_statements().push_back( *it );
                              (*it)->set_parent( bb );
                         }
               }
             } break;
           case V_SgSwitchStatement: {
               SgSwitchStatement *switchStm = isSgSwitchStatement(node);
               if((stmBody != NULL) && (stmBody != switchStm->get_body())) {
                   switchStm->set_body(stmBody);
                   stmBody->set_parent(switchStm);
               }
             } break;
           case V_SgForInitStatement: {
               SgForInitStatement *forInitStm = isSgForInitStatement(node);
               if((stmBodyList != NULL) && (stmBodyList != &forInitStm->get_init_stmt())) {
                    forInitStm->get_init_stmt().clear();
                    for(SgStatementPtrList::iterator it = stmBodyList->begin();
                        it != stmBodyList->end();
                        ++it) {
                              forInitStm->get_init_stmt().push_back( *it );
                              (*it)->set_parent( forInitStm );
                         }
               }
             } break;
           default:
               return false;
          };
          return true;
     }
};
