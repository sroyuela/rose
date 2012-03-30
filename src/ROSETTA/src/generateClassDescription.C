// ################################################################
// #                           Header Files                       #
// ################################################################

#include "ROSETTA_macros.h"
#include "grammar.h"
#include "terminal.h"
#include "grammarString.h"
#include <sstream>
#include <vector>
#include <iostream>
#include <string>
#include <exception>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/spirit/include/classic.hpp>

using namespace std;
using namespace boost::algorithm;
using namespace boost::spirit::classic;

struct Syntax : public grammar<Syntax> {
     private: 
          //Parsed content
            mutable string s;
            mutable Grammar::TYPE_CATEGORY tC;     
     public:
          //Functors
            struct FString {
               private:
                    string &strAccumelator;
               public:
                    FString(string &str) : strAccumelator(str) {};
                    void operator()(const char &ch) const {strAccumelator.append(1, ch);}
                    void operator()(const char *first, const char *last) const {
                         if((long int)last-(long int)first <= 0)
                              return;
                         if(strAccumelator.size() > 0)
                              strAccumelator.append(" ");
                         strAccumelator.append(first, (long int)last-(long int)first);
                    }
            };
            struct FTypeCat {
                    private:
                         Grammar::TYPE_CATEGORY &tC, TC;
                    public:
                         FTypeCat(Grammar::TYPE_CATEGORY &tcVar, Grammar::TYPE_CATEGORY t) : tC(tcVar) {TC = t;}
                         template <typename IteratorT> void operator()( IteratorT, IteratorT ) const {tC = TC;}
            };
       //PRE definitions
         template <typename ScannerT>
         struct definition {
            public:
               /* NOTE: This grammar accepts type names that isn't legal by the C/C++ standard.
                        (Such as: "unsigned double", "long char" etc.)
                        This has been intentionally added to simply the grammar substantially.

                        A compiler won't accept a file containing accessor-functions returning
                        any of these illegal type, so the problem is not of practical importance.*/
               #define SET_STR Syntax::FString(self.s)
               #define SET_TC(_x) Syntax::FTypeCat(self.tC, Grammar::_x)
               #define LD_(_x) as_lower_d[_x]
               definition( Syntax const &self ) {
                    //Literals def.
                      chlit<> PTR('*');
                      strlit<> TEMPLATE("::");
                      signmodifier = (LD_("unsigned") ^ LD_("signed"));
                      sizemodifier = (LD_("short") ^ LD_("long"));
                    //Types (special string handling)
                      ROSET = lexeme_d[(LD_("sg") >> +alpha_p)[SET_STR] >> *blank_p >> PTR][SET_TC(TCAT_SGNODE)];
                    //Types
                      boolT = LD_("bool")[SET_TC(TCAT_BOOL)];
                      floatT = LD_("float")[SET_TC(TCAT_FLOAT)];
                      stdT = lexeme_d[LD_("std") >> TEMPLATE >> +alpha_p][SET_TC(TCAT_STD_CONT)];
                      doubleT = LD_("double")[SET_TC(TCAT_DOUBLE)];
                      intT = LD_("int")[SET_TC(TCAT_INT)];
                      charT = (LD_("char") ^ LD_("wchar_t") ^ LD_("char16_t") ^ LD_("char32_t"))[SET_TC(TCAT_CHAR)];
                      //To handle the ambiguity in the grammar, longest_d[] has some unwanted side-effects for this PRE...
                        ambigSolve = (signmodifier || sizemodifier || sizemodifier)[SET_TC(TCAT_INT)] || (intT ^ doubleT ^ charT);

                    T = !LD_("static") >> (ROSET ^ (ambigSolve ^ boolT ^ floatT ^ stdT)[SET_STR]);
               }
               boost::spirit::classic::rule<ScannerT> LONG, ROSET, boolT, charT, floatT, doubleT, intT, stdT, ambigSolve,
                         signmodifier, sizemodifier, T;
               const boost::spirit::classic::rule<ScannerT> &start() const { return T; }
         };
       //Struct
         Syntax() {}
         ~Syntax() {}
         string getResults(Grammar::TYPE_CATEGORY &typeCat) {
               //Fix type name...
                 string s(this->s.c_str());
                 this->s.clear();
               //Fix type cat...
                 typeCat = this->tC;               

               return s;
         }
         void SetTypeCat(Grammar::TYPE_CATEGORY tC) {this->tC = tC;}    
};

//Global variables
Syntax *typeSyntaxParser;
bool Grammar::initializeClassDesc() {
     //Initialize type parser object
       if((typeSyntaxParser = new Syntax()) == NULL)
               return false;
     return true;
}
bool Grammar::finalizeClassDesc() {
     delete typeSyntaxParser;
     return true;
}

string isProperTypeName(string typeName, Grammar::TYPE_CATEGORY &t, string &realTypeName) {
     if(typeSyntaxParser == NULL)
               return "";

     //Run parser...
       parse_info<> info;
       Grammar::TYPE_CATEGORY typeCat = Grammar::TCAT_NONE;
       typeSyntaxParser->SetTypeCat(Grammar::TCAT_NONE);
       try {
            info = parse( typeName.c_str(), *typeSyntaxParser, space_p );
       } catch( exception &e ) {
                cout << "Exception!" << endl;
                return "";
       }
       realTypeName = typeSyntaxParser->getResults(typeCat);

       if( info.full ) {
           //Fix the typename...
             string typeNameProper = realTypeName;
             boost::replace_all(typeNameProper,"::","_");
             boost::replace_all(typeNameProper," ","_");
             boost::replace_all(typeNameProper,"\t","_");
           t = typeCat;
           //cout << typeName << ": OK! '" << realTypeName << "' (" << typeCat << ")" << endl;
           return typeNameProper;
       } else {
               t = Grammar::TCAT_NONE;
               realTypeName = "";
               //cout << typeName << ": Fail '" << realTypeName << "' (" << typeCat << ")" << endl;
               return "";
       }
}


bool Grammar::genClassDescriptionTerm(Terminal &node, 
                                      vector<Grammar::MFClassDescriptor *> &desc, 
                                      map<string, Grammar::TYPE_CATEGORY> &typeCategoryMap,
                                      map<string, string> &realTypeNameMap) {
        //Collect information regarding "node"
          //Functions conforming to "T Name(void) const"...
            vector<MFFunctionDescriptor *> *funcVec = new vector<MFFunctionDescriptor *>();
            vector<GrammarString *> localList = node.getMemberDataPrototypeList(Terminal::LOCAL_LIST,Terminal::INCLUDE_LIST);
            vector<GrammarString *> localExcludeList = node.getMemberDataPrototypeList(Terminal::LOCAL_LIST,Terminal::EXCLUDE_LIST);
            Grammar::editStringList ( localList, localExcludeList );

            for(vector<GrammarString *>::iterator dataMemberIterator = localList.begin();
                dataMemberIterator != localList.end();
                ++dataMemberIterator) {
                    GrammarString &data = **dataMemberIterator;

                    //Get return type...
                      string returnType = data.getTypeNameString(), realTypeName;
                      TYPE_CATEGORY returnTypeCat = TCAT_NONE;
                      if((returnType = isProperTypeName(returnType, returnTypeCat, realTypeName)) == "")
                         continue;
                      if(typeCategoryMap.find(returnType) == typeCategoryMap.end()) {
                         typeCategoryMap[returnType] = returnTypeCat;
                         realTypeNameMap[returnType] = realTypeName;
                      }

                    //Should we include this access method?
                      BuildAccessEnumX funcType = data.automaticGenerationOfDataAccessFunctions.getValue();
                      if((funcType == TAG_BUILD_ACCESS_FUNCTIONS) ||
                         (funcType == TAG_BUILD_FLAG_ACCESS_FUNCTIONS)) {
                          MFFunctionDescriptor *fDesc = 
                              new MFFunctionDescriptor(returnTypeCat, returnType, "get_" + data.getVariableNameString());
                          funcVec->push_back(fDesc);
                      } else if(funcType == TAG_BUILD_LIST_ACCESS_FUNCTIONS) {
                            //Should this really be ignored?
                      }
            }
        //Create CD object...
          string baseClassName = "INVALID_ID", realClassName = "INVALID_ID";
          TYPE_CATEGORY typeCatBase = TCAT_NONE;
          if(node.getBaseClass() != NULL) {
             string tmpBaseClassName = node.getBaseClass()->getName() + " *", tmpRealClassName;
             boost::replace_all(tmpBaseClassName,"$CLASSNAME", node.getName());
             //cout << node.getName() << ": Parent '" << tmpBaseClassName << "(" << node.getBaseClass()->getName() << ")" << endl;
             if((tmpBaseClassName = isProperTypeName(tmpBaseClassName, typeCatBase, tmpRealClassName)) != "") {
               baseClassName = tmpBaseClassName;
               realClassName = tmpRealClassName;
             }
          }
          typeCategoryMap[baseClassName] = typeCatBase;
          if(realClassName != "INVALID_ID") 
               realTypeNameMap[baseClassName] = realClassName + " *";

          MFClassDescriptor *cd = new MFClassDescriptor( typeCatBase, node.getName(), baseClassName, funcVec );

          if(maxNumberOfFunctions < funcVec->size())
               maxNumberOfFunctions = funcVec->size();
        //Push descriptor to collection...
          desc.push_back(cd);
        //Perform same process for all children...
          bool success = true;
          for(vector<Terminal *>::iterator treeNodeIterator = node.subclasses.begin();
              treeNodeIterator != node.subclasses.end();
              ++treeNodeIterator) {
                if( ((*treeNodeIterator) == NULL) ||
                    ((*treeNodeIterator)->getBaseClass() == NULL) )
                      success = false;
                else
                    success &= genClassDescriptionTerm(**treeNodeIterator, desc, typeCategoryMap, realTypeNameMap);
          }
        //Return results
          return success;
}

bool Grammar::genClassDescription(vector<Grammar::MFClassDescriptor *> &desc, 
                                  map<string, Grammar::TYPE_CATEGORY> &typeCategoryMap,
                                  map<string, string> &realTypeNameMap,
                                  ofstream &classMemberDescC,
                                  ofstream &classMemberDescH) {
     //Build up sorted sets of all types and access functions...
     set<string> types, accessFunctions;
     map<string, Grammar::MFClassDescriptor *> typeNameToDescMap; 
     for(vector<Grammar::MFClassDescriptor *>::iterator it = desc.begin();
         it != desc.end();
         ++it) {
          types.insert( (*it)->classType );
          typeNameToDescMap[(*it)->classType] = *it;

          for(vector<MFFunctionDescriptor *>::iterator it2 = (*it)->funcDesc->begin();
              it2 != (*it)->funcDesc->end();
              ++it2) {
               types.insert( (*it2)->returnType );
               accessFunctions.insert( (*it2)->funcName );
          }
     }
          
     //Build the C & H file...
     map<string, int> funcNameToIndexMap, typeNameToIndexMap;
     map<int, string> indexToTypeNameMap;
     classMemberDescC << "// ################################################################" << endl
                      << "// #          SgNode (and derived) class member functions         #" << endl
                      << "// ################################################################" << endl
                      << "//" << endl
                      << "// This file is auto-generated by src/ROSETTA/src/generateClassDescription.C " << endl
                      << "// and should as such not be manually edited." << endl
                      << "//" << endl
                      << "// The contents of this file reflects the access functions of each SgNode derived class." << endl
                      << "// Each function on the form \"T [Name](void) const\" (where T is the return type and" << endl
                      << "// 'Name' is the function name) will be assigned a index number."
                      << endl << endl << endl
                      << "#include <string>" << endl
                      << "#include <boost/algorithm/string/predicate.hpp>" << endl
                      << "#include \"" << string(getGrammarName()) + "ClassMemberFunc.h" << "\"" << endl
                      << endl
                      << "using namespace std;" << endl
                      << "using namespace boost::algorithm;" << endl
                      << "using namespace MemberFuncEnums::Types;" << endl
                      << "using namespace MemberFuncEnums::Function;" << endl
                      << "using namespace MemberFuncEnums::TypeCategory;" << endl
                      << endl << endl
                      << "// ---------- memberFuncName ----------" << endl
                      << "static const string memberFuncNameArray[] =" << endl
                      << "       {" << endl
                      << "              ";

                      // Insert all class access functions...
                         unsigned int i = 0, lenOfLine = string("              ").size();
                         for(set<string>::iterator it = accessFunctions.begin();
                             it != accessFunctions.end();
                             ++it, i++) {
                              funcNameToIndexMap[*it] = i;

                              if(i < accessFunctions.size() - 1) {
                                   lenOfLine += string("\"").size() + it->size() + string("\", ").size();
                                   if(lenOfLine >= 100) {
                                        //Pretty-print this array at least...
                                        lenOfLine = string("              ").size();
                                        classMemberDescC << endl << "              ";
                                   }
                                   classMemberDescC << "\"" << (*it) << "\", ";
                              } else
                                   classMemberDescC << "\"" << (*it) << "\"" << endl;
                         }

     classMemberDescC << "       };" << endl
                      << endl
                      << "// ---------- typesName ----------" << endl
                      << "static const string memberTypeNameArray[] =" << endl
                      << "       {" << endl
                      << "              ";

                      // Insert all class access types...
                         i = 0, lenOfLine = string("              ").size();
                         for(set<string>::iterator it = types.begin();
                             it != types.end();
                             ++it, i++) {
                              typeNameToIndexMap[*it] = i;
                              indexToTypeNameMap[i] = *it;

                              if(i < types.size() - 1) {
                                   lenOfLine += string("\"").size() + it->size() + string("\", ").size();
                                   if(lenOfLine >= 100) {
                                        //Pretty-print this array at least...
                                        lenOfLine = string("              ").size();
                                        classMemberDescC << endl << "              ";
                                   }
                                   classMemberDescC << "\"" << (*it) << "\", ";
                              } else
                                   classMemberDescC << "\"" << (*it) << "\"" << endl;
                         }

     classMemberDescC << "       };" << endl
                      << endl
                      << "// ---------- type description ----------" << endl
                      << "static const classDescriptor typeDescArray[] =" << endl
                      << "       {" << endl;

                      // An array describing all the classes and their access functions...
                         i = 0;
                         for(map<int, string>::iterator it = indexToTypeNameMap.begin();
                             it != indexToTypeNameMap.end();
                             ++it, i++) {
                              //Try to find class descriptor
                                Grammar::MFClassDescriptor *classDesc = NULL;
                                map<string, Grammar::MFClassDescriptor *>::iterator itCDesc;
                                if((itCDesc = typeNameToDescMap.find(it->second)) != typeNameToDescMap.end())
                                   classDesc = itCDesc->second;
                              //Insert descriptor...
                                if(classDesc != NULL) {
                                   string classType = "TID_" + classDesc->classType,
                                          baseClass = "TID_" + classDesc->baseClass;

                                   classMemberDescC << "              {" << classType << ", " << baseClass;
                                   switch(classDesc->classTypeCategory) {
                                        case TCAT_NONE:         classMemberDescC << ", TCAT_NONE";                break;
                                        case TCAT_SGNODE:       classMemberDescC << ", TCAT_SGNODE";              break;
                                        default:  ROSE_ASSERT(false); //Class can't have this type as parent!
                                   }
                                   classMemberDescC << ", " << classDesc->funcDesc->size();

                                   unsigned int j = 0;
                                   if( classDesc->funcDesc->size() > 0) {
                                        classMemberDescC << ", {" << endl;
                                        for(vector<MFFunctionDescriptor *>::iterator it2 = classDesc->funcDesc->begin();
                                            it2 != classDesc->funcDesc->end();
                                            ++it2, j++) {
                                             string returnType = "TID_" + (*it2)->returnType,
                                                    funcName = "FID_" + (*it2)->funcName;
                                             classMemberDescC << "                  {" << returnType << ", " << funcName;
                                             switch((*it2)->returnTypeCategory) {
                                                  case TCAT_NONE:         classMemberDescC << ", TCAT_NONE";                break;
                                                  case TCAT_SGNODE:       classMemberDescC << ", TCAT_SGNODE";              break;
                                                  case TCAT_BOOL:         classMemberDescC << ", TCAT_BOOL";                break;
                                                  case TCAT_CHAR:         classMemberDescC << ", TCAT_CHAR";                break;
                                                  case TCAT_FLOAT:        classMemberDescC << ", TCAT_FLOAT";               break;
                                                  case TCAT_DOUBLE:       classMemberDescC << ", TCAT_DOUBLE";              break;
                                                  case TCAT_INT:          classMemberDescC << ", TCAT_INT";                 break;
                                                  case TCAT_STD_CONT:     classMemberDescC << ", TCAT_STD_CONTAINER";       break;
                                             }
                                             classMemberDescC << "}";
                                             if(j < classDesc->funcDesc->size()-1)
                                                  classMemberDescC << ",";
                                             classMemberDescC << endl;
                                        }
                                        classMemberDescC << "              }}";
                                   } else
                                        classMemberDescC << ", {}}";
                                } else {
                                   //Handler for other type categories...
                                     map<string, Grammar::TYPE_CATEGORY>::iterator it2 = typeCategoryMap.find(it->second);
                                     ROSE_ASSERT(it2 != typeCategoryMap.end());
                                     classMemberDescC << "              {TID_" << it->second << ", TID_INVALID_ID, ";
                                     switch(it2->second) {
                                      case TCAT_NONE:         classMemberDescC << "TCAT_NONE, 0, {}}";                break;
                                      case TCAT_SGNODE:       classMemberDescC << "TCAT_SGNODE, 0, {}}";              break;
                                      case TCAT_BOOL:         classMemberDescC << "TCAT_BOOL, 0, {}}";                break;
                                      case TCAT_CHAR:         classMemberDescC << "TCAT_CHAR, 0, {}}";                break;
                                      case TCAT_FLOAT:        classMemberDescC << "TCAT_FLOAT, 0, {}}";               break;
                                      case TCAT_DOUBLE:       classMemberDescC << "TCAT_DOUBLE, 0, {}}";              break;
                                      case TCAT_INT:          classMemberDescC << "TCAT_INT, 0, {}}";                 break;
                                      case TCAT_STD_CONT:     classMemberDescC << "TCAT_STD_CONTAINER, 0, {}}";       break;
                                     }
                                }
                              //Are there more types?
                                if(i < indexToTypeNameMap.size()-1)
                                     classMemberDescC << ",";
                                classMemberDescC << endl;
                         }
             
     classMemberDescC << "       };" << endl
                      << endl
                      << "// ---------- Functions ----------" << endl;

                      // Insert all type converter functions...
                         for(map<string,int>::iterator it = typeNameToIndexMap.begin();
                             it != typeNameToIndexMap.end();
                             ++it) {
                                //Only add declarations for non-sgnode types...
                                  map<string, Grammar::TYPE_CATEGORY>::iterator it2 = typeCategoryMap.find(it->first);
                                  if((it2 == typeCategoryMap.end()) || (it2->second == TCAT_SGNODE))
                                     continue;
                                //Add declaration
                                  classMemberDescC << "bool toValue(boost::any &valAny, " << realTypeNameMap.find( it->first )->second << " &valProper) {" << endl
                                                   << "     try {" << endl
                                                   << "          valProper = boost::any_cast<" << realTypeNameMap.find( it->first )->second << ">(valAny);" << endl
                                                   << "          return true;" << endl
                                                   << "     } catch(boost::bad_any_cast &) {" << endl
                                                   << "          return false;" << endl
                                                   << "}" << endl;
                         }

     classMemberDescC << "bool getMemberFuncValue(SgNode *inputNode, MemberFuncEnums::Types::TID inputNodeType, MemberFuncEnums::Types::FID memberFunc, boost::any &val) {"<< endl
                      << "    if(inputNodes == NULL)" << endl
                      << "         return false;" << endl
                      << "    switch(inputNodeType) {" << endl;
                         // Create collector function                         
                              for(vector<Grammar::MFClassDescriptor *>::iterator it = desc.begin();
                                  it != desc.end();
                                  ++it) {
                                   //No point in adding classes without accessors...
                                   if((*it)->funcDesc->size() > 0) {
                                        classMemberDescC << "     case TID_" << (*it)->classType << ": {" << endl
                                                         << "         " << realTypeNameMap.find( (*it)->classType )->second << 
                                                                 " iNode_ = static_cast<" << realTypeNameMap.find( (*it)->classType )->second << ">(inputNode);" << endl
                                                         << "         if(iNode_ == NULL)" << endl
                                                         << "              return false;" << endl
                                                         << "         switch(memberFunc) {" << endl;
                                        for(vector<MFFunctionDescriptor *>::iterator it2 = (*it)->funcDesc->begin();
                                            it2 != (*it)->funcDesc->end();
                                            ++it2)
                                                  classMemberDescC << "              case FID_" << (*it2)->funcName << ": val = iNode_->" << (*it2)->funcName << "();     return true;" << endl;
                                        classMemberDescC << "              default: ";
                                        if((*it)->baseClass == "INVALID_ID")
                                             classMemberDescC << "return false;" << endl;
                                        else
                                             classMemberDescC << "return getMemberFuncValue(inputNode, TID_" << (*it)->baseClass << ", memberFunc, val);" << endl;
                                        classMemberDescC << "         } } break;" << endl;
                                   }
                              }
     classMemberDescC << "     default: return false;" << endl
                      << "    }" << endl
                      << "}" << endl
                      << endl
                      << "MemberFuncEnums::TypeCategory::TYPE_CAT getTypeCatFromType(MemberFuncEnums::Types::TID tID) {" << endl
                      << "    switch(tID) {" << endl;
                         for(map<string, Grammar::TYPE_CATEGORY>::iterator it = typeCategoryMap.begin(); it != typeCategoryMap.end(); ++it) {
                              classMemberDescC << "         case TID_" << it->first << ": return ";
                              switch(it->second) {
                                   case TCAT_NONE:         classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_NONE;" << endl;                       break;
                                   case TCAT_SGNODE:       classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_SGNODE;" << endl;                     break;
                                   case TCAT_BOOL:         classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_BOOL;" << endl;                       break;
                                   case TCAT_CHAR:         classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_CHAR;" << endl;                       break;
                                   case TCAT_FLOAT:        classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_FLOAT;" << endl;                      break;
                                   case TCAT_DOUBLE:       classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_DOUBLE;" << endl;                     break;
                                   case TCAT_INT:          classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_INT;" << endl;                        break;
                                   case TCAT_STD_CONT:     classMemberDescC << "MemberFuncEnums::TypeCategory::TCAT_STD_CONTAINER;" << endl;              break;
                              }
                         }

     classMemberDescC << "static const string getMemberNameById(int memberIndex, const string &array[], int maxIndex, int invalidIndex) const {" << endl
                      << "   //Retreive function name from index 'memberFuncIndex' in 'memberFuncName'" << endl
                      << "     if((memberFuncIndex >= 0) && (memberFuncIndex <= maxIndex))" << endl
                      << "       return array[memberIndex];" << endl
                      << "     else" << endl
                      << "       return invalidIndex;" << endl
                      << "}" << endl
                      << endl
                      << "static int getMemberIdByName(const string memberName, const string &array[], int maxIndex, int invalidIndex) const {" << endl
                      << "    for(int sI = 0, eI = maxIndex, mI = (sI+eI)/2; sI <= eI; mI = (sI+eI)/2)" << endl
                      << "       if(boost::iequals(memberName, array[mI]))" << endl
                      << "         return mI;" << endl
                      << "       else if(lexicographical_compare(memberName, array[mI], is_iless()))" << endl
                      << "           eI = mI - 1;" << endl
                      << "       else" << endl
                      << "           sI = mI + 1;" << endl
                      << "    return invalidIndex;" << endl
                      << "}" << endl
                      << endl
                      << "int getMemberFuncIdByName(const string memberFuncName) const {" << endl
                      << "    return getMemberIdByName(memberFuncName," << endl
                      << "                             memberFuncNameArray," << endl
                      << "                             MemberFuncEnums::Function.FID_MAXIMUM_ID," << endl
                      << "                             MemberFuncEnums::Function.FID_INVALID_ID);" << endl
                      << "}" << endl
                      << endl 
                      << "int getMemberTypeIdByName(const string memberTypeName) const {" << endl
                      << "    return getMemberIdByName(memberTypeName," << endl
                      << "                             memberTypeNameArray," << endl
                      << "                             MemberFuncEnums::Types.TID_MAXIMUM_ID," << endl
                      << "                             MemberFuncEnums::Types.TID_INVALID_ID);" << endl
                      << "}" << endl
                      << "const string getMemberFuncNameById(int memberFuncId) const {" << endl
                      << "    return getMemberNameById(memberFuncId," << endl
                      << "                             memberFuncNameArray," << endl
                      << "                             MemberFuncEnums::Function.FID_MAXIMUM_ID," << endl
                      << "                             MemberFuncEnums::Function.FID_INVALID_ID);" << endl
                      << "}" << endl
                      << "const string getMemberTypeNameById(int memberTypeId) const {" << endl
                      << "    return getMemberNameById(memberTypeId," << endl
                      << "                             memberTypeNameArray," << endl
                      << "                             MemberFuncEnums::Types.TID_MAXIMUM_ID," << endl
                      << "                             MemberFuncEnums::Types.TID_INVALID_ID);" << endl
                      << "}" << endl
                      << endl;

     classMemberDescH << "// ################################################################" << endl
                      << "// #          SgNode (and derived) class member functions         #" << endl
                      << "// ################################################################" << endl
                      << "//" << endl
                      << "// This file is auto-generated by src/ROSETTA/src/generateClassDescription.C " << endl
                      << "// and should as such not be manually edited." << endl
                      << "//" << endl
                      << "// The contents of this file reflects the access functions of each SgNode derived class." << endl
                      << "// Each function on the form \"T [Name](void) const\" (where T is the return type and" << endl
                      << "// 'Name' is the function name) will be assigned a index number." << endl
                      << endl
                      << "#ifndef CLASS_MEMBER_FUNC_HEADER" << endl
                      << "#define CLASS_MEMBER_FUNC_HEADER" << endl
                      << endl << endl << endl
                      << "#include <boost/any.hpp>" << endl
                      << endl << endl
                      << "#include <string>" << endl
                      << "#include <set>" << endl
                      << endl << endl
                      << "using namespace std;" << endl
                      << endl
                      << "//Definitions" << endl
                      << "#define MAXIMUM_NUM_MEMBER_FUNCTIONS_PER_CLASS " << maxNumberOfFunctions << endl
                      << "namespace MemberFuncEnums {" << endl
                      << "   namespace Types {" << endl
                      << "      enum TID {" << endl;

                      // Insert all class access types...
                         int maxId = 0, maxSgNode = 0;
                         for(map<string,int>::iterator it = typeNameToIndexMap.begin();
                             it != typeNameToIndexMap.end();
                             ++it) {
                              maxId = (maxId > it->second ? maxId : it->second);
                              classMemberDescH << "              TID_" << it->first << " = " << it->second << ", " << endl;

                              map<string, Grammar::TYPE_CATEGORY>::iterator it2 = typeCategoryMap.find(it->first);
                              if((it2 != typeCategoryMap.end()) && (it2->second == TCAT_SGNODE))
                                   maxSgNode = (maxSgNode > it->second ? maxSgNode : it->second);
                         }

     classMemberDescH << "              TID_MAXIMUM_ID_SGNODE = " << maxSgNode << "," << endl
                      << "              TID_MAXIMUM_ID = " << maxId << "," << endl
                      << "              TID_INVALID_ID = " << (maxId+1) << endl
                      << "      };" << endl
                      << "   };" << endl
                      << "   namespace TypeCategory {" << endl
                      << "         enum TYPE_CAT {" << endl
                      << "              TCAT_NONE = 0," << endl
                      << "              TCAT_SGNODE = 1," << endl
                      << "              TCAT_BOOL = 2," << endl
                      << "              TCAT_CHAR = 3," << endl
                      << "              TCAT_FLOAT = 4," << endl
                      << "              TCAT_DOUBLE = 5," << endl
                      << "              TCAT_INT = 6," << endl
                      << "              TCAT_STD_CONT = 7" << endl
                      << "         };" << endl
                      << "   };" << endl
                      << "   namespace Function {" << endl
                      << "      enum FID {" << endl;

                      // Insert all class member functions...
                         maxId = 0;
                         for(map<string,int>::iterator it = funcNameToIndexMap.begin();
                             it != funcNameToIndexMap.end();
                             ++it) {
                              maxId = (maxId > it->second ? maxId : it->second);
                              classMemberDescH << "              FID_" << it->first << " = " << it->second << ", " << endl;
                         }

     classMemberDescH << "              FID_MAXIMUM_ID = " << maxId << "," << endl
                      << "              FID_INVALID_ID = " << (maxId+1) << endl
                      << "      };" << endl
                      << "   };" << endl
                      << "};" << endl
                      << "typedef struct {" << endl
                      << "   MemberFuncEnums::Types::TID returnType;" << endl
                      << "   MemberFuncEnums::Function::FID funcName;" << endl
                      << "   MemberFuncEnums::TypeCategory::TYPE_CAT returnTypeCategory;" << endl
                      << "} funcDescriptor;" << endl
                      << endl
                      << "typedef struct {" << endl
                      << "   Types::TID classType;" << endl
                      << "   Types::TID baseClass;" << endl
                      << "   MemberFuncEnums::TypeCategory::TYPE_CAT classTypeCategory;" << endl
                      << "   unsigned int nrAccFuncs;" << endl
                      << "   funcDescriptor accFuncs[];" << endl //MAXIMUM_NUM_MEMBER_FUNCTIONS_PER_CLASS
                      << "} classDescriptor;" << endl
                      << endl

                      << endl << endl
                      << "external const string memberFuncNameArray[];" << endl
                      << "external const string memberTypeNameArray[];" << endl
                      << "external const classDescriptor typeDescArray[];" << endl
                      << endl << endl
                      << "//Std::less class for non-SgNode types..." << endl
                      << "struct lessMisc {" << endl
                      << "     template <class T, class S> bool operator()(T x, S y) {return (x < y);}" << endl
                      << "     bool operator()(MemberFuncEnums::Types::TID tID, boost::any &x, boost::any &y) {" << endl
                      << "         switch(tID) {" << endl;
                       
                         for(map<string,int>::iterator it = typeNameToIndexMap.begin();
                             it != typeNameToIndexMap.end();
                             ++it) {
                                //Only add cases for non-sgnode types...
                                  map<string, Grammar::TYPE_CATEGORY>::iterator it2 = typeCategoryMap.find(it->first);
                                  if((it2 == typeCategoryMap.end()) || (it2->second == TCAT_SGNODE))
                                     continue;
                                //Add case
                                  classMemberDescH << "              case TID_" << it->first << ": {" << endl
                                                   << "                    " << realTypeNameMap.find( it->first )->second << " x_, y_;" << endl
                                                   << "                    if(!toValue(x, x_) || !toValue(y, y_))" << endl
                                                   << "                         return false;" << endl
                                                   << "                    else" << endl
                                                   << "                         return (*this)(x_, y_);}" << endl;
                         }

     classMemberDescH << "              default: ROSE_ASSERT(false);" << endl
                      << "         }" << endl
                      << "     }" << endl
                      << "};" << endl
                      << "//Comparator class for non-SgNode types..." << endl
                      << "struct eqMisc {" << endl
                      << "    template <class T, class S> bool operator()(T x, S y) {return (x == y);}" << endl
                      << "    bool operator()(MemberFuncEnums::Types::TID tID, boost::any &x, boost::any &y) {" << endl
                      << "         lessMisc lS;" << endl
                      << "         return (!lS(tID,x,y) && !lS(tID,y,x));" << endl
                      << "    }" << endl
                      << "};" << endl << endl
                      << "// Functions to retrieve name and index of member function desc." << endl
                      << "MemberFuncEnums::TypeCategory::TYPE_CAT getTypeCatFromType(MemberFuncEnums::Types::TID tID);" << endl
                      << "int getMemberFuncIdByName(const string memberFuncName) const;" << endl
                      << "int getMemberTypeIdByName(const string memberTypeName) const;" << endl
                      << "const string getMemberFuncNameById(int memberFuncId) const;" << endl
                      << "const string getMemberTypeNameById(int memberTypeId) const;" << endl
                      << "//Functions to retreive class description & member values." << endl;

                      // Insert all type converter functions...
                         for(map<string,int>::iterator it = typeNameToIndexMap.begin();
                             it != typeNameToIndexMap.end();
                             ++it) {
                                //Only add declarations for non-sgnode types...
                                  map<string, Grammar::TYPE_CATEGORY>::iterator it2 = typeCategoryMap.find(it->first);
                                  if((it2 == typeCategoryMap.end()) || (it2->second == TCAT_SGNODE))
                                     continue;
                                //Add declaration
                                  classMemberDescH << "bool toValue(boost::any &valAny, " << realTypeNameMap.find( it->first )->second << " &valProper);" << endl;
                         }

     classMemberDescH << endl
                      << "bool getMemberFuncValue(SgNode *inputNode, MemberFuncEnums::Types::TID inputNodeType, MemberFuncEnums::Types::FID memberFunc, boost::any &val);" << endl
                      << endl << endl
                      << "#endif" << endl;

     return true;
}
