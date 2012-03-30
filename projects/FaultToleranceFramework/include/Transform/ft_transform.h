#ifndef FT_HEADER_BEGIN
#define FT_HEADER_BEGIN

#include <iostream>
#include <limits>
#include <map>
#include <vector>
#include <boost/regex.hpp>

#include "rose.h"
#include "OmpAttribute.h"

#include "TI.h"
#include "ft_common.h"

//The following defines the behavior/assumptions build into the FT modules.
#define FT_TRANFORMATION_NODE_ATTR "FT_TRANSFORMATION_"
#define FT_POST_ERROR_MSG_AS_SRC_COMMENT

/** 
 * @namespace FT
 * @brief Fault-tolerant main namespace, encapsulates FT::Common, FT::Transform, FT::Analysis
 * 
*/

namespace FT {
   /** 
     * @class FTTransform
     * @brief Fault-tolerant transformations main class. Contains basic transformations for adding redundant computations to
     * a given IR node.
     * The transformations create for inter-/intra- dimension a initialization, execute and unification section. The init. section
     * includes variable declarations and initialization. The execute section performs the redundant computations. Finally, the unifications
     * section unifies the results of the redundant computations by a fault handling policy.
     * @author Jacob Lidman
     * 
    */
   class Transform {
     public:
           //Visitors
             /** 
             * @class FTPragmaVisitor
             * @brief Default implmentation of FTVisitor. Applies fault-tolerance to each statement that has a "#pragma resiliency" proceeding it.
             */
             class FTPragmaVisitor : public Common::FTVisitor {
                    private:
                         bool langHasC, langHasCxx, langHasFortran;
                    public:
                         FTPragmaVisitor() {
                              //Detect language...
                                   langHasC = SageInterface::is_C_language() || SageInterface::is_C99_language();
                                   langHasCxx = SageInterface::is_Cxx_language();
                                   langHasFortran = SageInterface::is_Fortran_language();
                         }
                         bool targetNode(SgNode *node);
             };
           //Fault handling specification
             struct FailureSpecification {
                    private:

                    public:
                         FailureSpecification() {}
                         ~FailureSpecification() {}
             };

           //Fault handling policies
             /** 
             * @class FailurePolicy
             * @brief Abstract class for Fault-handling policies.
             */
             struct FailurePolicy {
                    private:

                    protected:
                         SgGlobal *globalScope;
                         /**
                          * Creates the unification section for a set of side effects.
                          * @param body Statements in the execution phase (= redundant computations)
                          * @param faultHandler The fault-handling policy used for this particular section.
                          * @param ftVars A map of the side effects and their respective set of redundant computations.
                          * @param condSuccess Expressions to deduce whether a fault occured.
                          **/   
                         SgStatement *createBasicIf(SgStatement *body, 
                                                    SgStatement *faultHandler, 
                                                    map<SgExpression *, set<SgExpression *> *> &ftVars, 
                                                    SgExpression *condSuccess,
                                                    SgStatement *elseCase,
                                                    bool alwaysFail);
                    public:
                         virtual ~FailurePolicy() {}
                         void setGlobalScope(SgGlobal *globalScope) {this->globalScope = globalScope;}
                         /**
                          * Creates the fault-handling implementation
                          * @param body Statements in the execution phase (= redundant computations)
                          * @param ftVars A map of the side effects and their respective set of redundant computations.
                          * @param condSuccess Expressions to deduce whether a fault occured.
                          * @param cascaded Parameter for indicating whether this is the top-level fault handler.
                          **/  
                         virtual SgStatement *getHandler(SgStatement *body, 
                                                         map<SgExpression *, set<SgExpression *> *> &ftVars, 
                                                         SgExpression *condSuccess, 
                                                         bool cascaded,
                                                         bool alwaysFail) {
                                   throw FT::Common::FTException("Invalid fault policy called.");
                         };
             };
             //////////////////////////////////////////////////////////////////////////////////////////////////////
             /////////////////////////////////////// N-order fault handlers ///////////////////////////////////////
             //////////////////////////////////////////////////////////////////////////////////////////////////////

             //Fault policy "Final wish".
               /** 
                * @class FailurePolicy_FinalWish
                * @brief Invokes a given statement before (optionally) calling a (N-1)-level handler.
                */
               struct FailurePolicy_FinalWish : public FailurePolicy {
                    private:
                         SgStatement *stm;
                         FailurePolicy *sLv;
                         bool alwaysExecStm;
                    public:
                         /**
                          * @param stm Statement that is invoked upon fault.
                          * @param secondLevel Optional second level fault handling policy.
                          **/ 
                         FailurePolicy_FinalWish(SgStatement *stm, bool alwaysExecStm, FailurePolicy *secondLevel) {
                              this->stm = stm;
                              this->sLv = secondLevel;
                              this->alwaysExecStm = alwaysExecStm; 
                         }

                         SgStatement *getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail);
               };

             //////////////////////////////////////////////////////////////////////////////////////////////////////
             /////////////////////////////////////// 2-order fault handlers ///////////////////////////////////////
             //////////////////////////////////////////////////////////////////////////////////////////////////////

             //Fault policy "Second chance".
               /** 
                * @class FailurePolicy_SecondChance
                * @brief Perform the computation upto N times while fault occurs, before calling second level handler
                */
               struct FailurePolicy_SecondChance : public FailurePolicy {
                    private:
                         int N;
                         FailurePolicy &sFP;
                    public:
                         /**
                          * @param N The maximum number of times the computations is performed.
                          * @param secondFP Second level fault handling policy.
                          **/ 
                         FailurePolicy_SecondChance(FailurePolicy &secondFP, int N) : sFP(secondFP) {this->N = N;}

                         SgStatement *getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail);
               };

             //////////////////////////////////////////////////////////////////////////////////////////////////////
             /////////////////////////////////////// 1-order fault handlers ///////////////////////////////////////
             //////////////////////////////////////////////////////////////////////////////////////////////////////

             //Fault policy "Die on error".
               /** 
                * @class FailurePolicy_DieOnError
                * @brief Print a messages that error has occured and call assertion to die.
                */
               struct FailurePolicy_DieOnError : public FailurePolicy {
                    FailurePolicy_DieOnError() {}
                    SgStatement *getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail);
               };

             //Fault policy "Adjudicator"
               /** 
                * @class FailurePolicy_Adjudicator
                * @brief Decide which of the variables that should be passed along...
                */
             struct FailurePolicy_Adjudicator : public FailurePolicy {
                  public:
                         //Adjucator
                          /** 
                           * @class Adjucator
                           * @brief Abstract class for adjucators
                           */  
                           struct Adjucator {
                              virtual ~Adjucator() {}
                              virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL) = 0;
                           };
                  private:
                    Adjucator *adj;
                  public:
                         FailurePolicy_Adjudicator(Adjucator *adj_) {this->adj = adj_;}

                         SgStatement *getHandler(SgStatement *body, map<SgExpression *, set<SgExpression *> *> &ftVars, SgExpression *condSuccess, bool cascaded, bool alwaysFail);
             };





             //Adjucator "Mean voting" - 
               /** 
                * @class Adjucator_Voting
                * @brief returns the mean of the results
                */
               class Adjucator_Voting_Mean : public FailurePolicy_Adjudicator::Adjucator {
                    private:
                         vector<double> weights;
                    public:
                         Adjucator_Voting_Mean() {this->weights = vector<double>();}
                         Adjucator_Voting_Mean(vector<double> weights) {this->weights = weights;}
                         virtual ~Adjucator_Voting_Mean() {}
                         virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL);
               };
             //Adjucator "Median voting" - 
               /** 
                * @class Adjucator_Voting
                * @brief returns the median of the results
                */
               class Adjucator_Voting_Median : public FailurePolicy_Adjudicator::Adjucator {
                    public:
                         Adjucator_Voting_Median() {}
                         virtual ~Adjucator_Voting_Median() {}
                         virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL);
               };
             //Adjucator "Exact voting" - 
               /** 
                * @class Adjucator_Voting
                * @brief returns the mean of the results
                */
               class Adjucator_Voting_Exact : public FailurePolicy_Adjudicator::Adjucator {
                    private:
                         map<string, int> varCount;
                         SgVariableDeclaration *cntVoteDecl, *iDecl;
                         bool assumeMajElemExist;
                    public:
                         Adjucator_Voting_Exact() {
                              this->cntVoteDecl = NULL;
                              this->iDecl = NULL;
                              this->assumeMajElemExist = true;
                         }
                         Adjucator_Voting_Exact(bool assumeMajElemExist) {
                              this->cntVoteDecl = NULL;
                              this->iDecl = NULL;
                              this->assumeMajElemExist = assumeMajElemExist;
                         }
                         virtual ~Adjucator_Voting_Exact() {}
                         virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL);
               };
             //Adjucator "Fuzzy voting" - 
               /** 
                * @class Adjucator_Voting
                * @brief 
                */
               class Adjucator_Voting_Fuzzy : public FailurePolicy_Adjudicator::Adjucator { 
                    private:
                         float epsF;
                         double epsD;
                         long double epsLD;
                    public:
                         Adjucator_Voting_Fuzzy(float epsF, double epsD, long double epsLD) {
                              this->epsF = epsF;
                              this->epsD = epsD;
                              this->epsLD = epsLD;
                         }
                         virtual ~Adjucator_Voting_Fuzzy() {}
                         virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL);
               };

             //Adjucator "Voting" - 
               /** 
                * @class Adjucator_Voting
                * @brief Vote on the results (resolves voter based on type)
                */
               struct Adjucator_Voting : public FailurePolicy_Adjudicator::Adjucator {
                    public:                         
                         //Secondary descriptors
                         class VotingMech_Desc {
                              private:
                                   boost::regex const *cond;
                                   VotingMech_Desc *accept, *fail;
                              public:
                                   VotingMech_Desc() {
                                        this->cond = new boost::regex(".*");
                                        this->accept = NULL;
                                        this->fail = NULL;
                                   }
                                   VotingMech_Desc(boost::regex *c, VotingMech_Desc *a, VotingMech_Desc *f) {
                                        this->cond = c;
                                        this->accept = a;
                                        this->fail = f;
                                   }
                                   virtual ~VotingMech_Desc() {};

                                   virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL) {
                                        if( (matchType == NULL) || boost::regex_match(matchType->class_name().c_str(), *cond) ) {
                                             //Avoid infinite loop...
                                               if(this->accept == NULL) {
                                                  cout << "Invalid voting mechanism detected in VotingMech_Desc::getHandler()/Accept on input '"
                                                       << SageInterface::get_name(matchType) << "'." << endl;
                                                  return NULL;
                                               }
                                             return accept->getHandler(initialVar, updatedValues, bb, (matchType == NULL ? NULL : SageInterface::getElementType(matchType)));
                                        } else {
                                             //Avoid infinite loop...
                                               if(this->fail == NULL) {
                                                  cout << "Invalid voting mechanism detected in VotingMech_Desc::getHandler()/Fail on input '"
                                                       << SageInterface::get_name(matchType) << "'." << endl;
                                                  return NULL;
                                               }
                                             return fail->getHandler(initialVar, updatedValues, bb, (matchType == NULL ? NULL : SageInterface::getElementType(matchType)));
                                        }
                                   }
                         };
                         template <class V> class VotingMech_Voter : public VotingMech_Desc {
                              private:
                                   V *adjVoter;
                              public:
                                   VotingMech_Voter() {}
                                   virtual ~VotingMech_Voter() {}
                                   virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType) {
                                        adjVoter = new V();
                                        return adjVoter->getHandler(initialVar, updatedValues, bb, matchType);
                                   }
                         };
                    private:
                         VotingMech_Desc *votingMechPerType;
                    public:
                         Adjucator_Voting(VotingMech_Desc *vMpT) {votingMechPerType = vMpT;}
                         Adjucator_Voting() {
                              votingMechPerType = new VotingMech_Desc(new boost::regex("Sg("
                                   "(Array|Function|MemberFunction|PartialFunction|PartialFunctionModifier|Named|Class|Pointer|PointerMember|Reference|Template)Type|"
                                   "Type(CrayPointer|Default|GlobalVoid|Label|String|Unknown|Void))"), 
                                   new VotingMech_Voter<Adjucator_Voting_Exact>(), 
                                   new VotingMech_Desc(
                                        new boost::regex("SgTypeComplex|SgTypeDouble|SgTypeEllipse|SgTypeFloat|SgTypeLongDouble"), 
                                        new VotingMech_Voter<Adjucator_Voting_Mean>(), 
                                        new VotingMech_Voter<Adjucator_Voting_Mean>()) );
                              /*new VotingMech_FuzzyVote(std::numeric_limits<float>::epsilon(), 
                                                                 std::numeric_limits<double>::epsilon(),
                                                                 std::numeric_limits<long double>::epsilon())*/
                         }

                         virtual SgStatement *getHandler(SgExpression *initialVar, set<SgExpression *> *updatedValues, SgBasicBlock *bb, SgType *matchType = NULL);
               };

           //Closure structures (for sharing multiple statements in the same)
            /**
             * @class FaultHandling_Closure
             * @brief Closures provides a mechanism for sharing data between fault-tolerant computations. This allows a BB to have a single initalization and unification
             * section rather than one of each per statement. Hence amortizing the cost.
             */
             struct FaultHandling_Closure {
                    private:
                         ////////////////// STATE SPACE //////////////////
                         //int *cfgNode; //To make sure all stmts belong to the same BB.
                         struct FaultHandling_Closure *parent_closure;
                         SgGlobal *globalScope;
                         SgBasicBlock **bbResult, *bbDecl, *bbExecIntra, *bbExecIntraOuter;
                         SgStatement *exitStm;

                         unsigned int redundancyInterCore, redundancyIntraCore, redundancyIntraOuter;
                         FailurePolicy &fPInter, &fPIntra;
                         map<SgExpression *, set<SgExpression *> *> intraFtVars, interFtVars, interCompExps;
                         vector<SgVariableDeclaration *> intraVarDecl, interVarDecl;


                         ////////////////// finalizer //////////////////
                         SgExpression *generateComparisonExp(map<SgExpression *, set<SgExpression *> *> &ftVars);
                         bool close();
                    public:
                         /**
                          * @param globalScope The global scope of the basic block/statement.
                          * @param bbResult Points to the pointer that after close() will contain the transformed basic block.
                          * @param redundancyInter The number of redundant computations in inter-core dimension.
                          * @param fPHandlerInter Fault-handling policy for faults in inter-core dimension.
                          * @param redundancyIntra The number of redundant computations in intra-core dimension.
                          * @param fPHandlerIntra Fault-handling policy for faults in inter-core dimension.
                          **/ 
                         FaultHandling_Closure(SgGlobal *globalScope,
                                               FaultHandling_Closure *parentClos,
                                               SgBasicBlock **bbResult,
                                               unsigned int redundancyInterCore, FailurePolicy &fPIe, 
                                               unsigned int redundancyIntraCore, unsigned int redundancyIntraOuter, FailurePolicy &fPIa) : fPInter(fPIe), fPIntra(fPIa) {
                              this->bbDecl = SageBuilder::buildBasicBlock();
                              this->bbExecIntra = SageBuilder::buildBasicBlock();
                              this->bbExecIntraOuter = (redundancyIntraOuter > 0 ? SageBuilder::buildBasicBlock() : NULL);
                              this->redundancyInterCore = redundancyInterCore;
                              this->redundancyIntraCore = redundancyIntraCore;
                              this->redundancyIntraOuter = redundancyIntraOuter;
                              this->globalScope = globalScope;
                              this->parent_closure = parentClos;

                              this->exitStm = NULL;
                              this->bbResult = bbResult;
                         }

                         ~FaultHandling_Closure() { close(); }

                         //Set functions
                         /**
                          * Set the exit statement of the basic block. Once this is entered the closure won't accept more statements.
                          * @param exitStm The exit statement.
                          **/ 
                         void setExitStm(SgStatement *exitStm) {this->exitStm = exitStm;}

                         //Get functions
                         bool hasExitSmt() {return (this->exitStm != NULL);}
                         SgStatement *getExitStm() {return this->exitStm;}
                         SgBasicBlock *getDeclScope() {return bbDecl;}
                         SgBasicBlock *getExecIntraScope() {return bbExecIntra;}
                         SgBasicBlock *getExecIntraOuterScope() {return bbExecIntraOuter;}

                         //Add functions
                         /**
                          * Add computations to the inter section of the closure.
                          * @param ftVarsMap A map of the side effects and their respective set of redundant computations.
                          * @param interCompare Map from side effect to comparison expression (as part of fault evaluation in the unification section).
                          **/ 
                         bool addInter(map<SgExpression *, set<SgExpression *> *> &ftVarsMap, map<SgExpression *, SgExpression *> &interCompare);
                         /**
                          * Get the variable declaration for a given variable name from the init. section.
                          * A declaration is added to the init. section in the event it previosly hasn't.
                          * @param varName The name of the variable.
                          * @param baseType Type of the variable.
                          * @param initExp Initializer of the variable.
                          **/ 
                         SgVariableDeclaration *getInterVar(string varName, SgType *baseType, SgExpression *initExp);
                         /**
                          * Add computations to the intra section of the closure.
                          * @param ftVarsMap A map of the side effects and their respective set of redundant computations.
                          * @param intraStm The body of the intra section in question.
                          **/ 
                         bool addIntra(map<SgExpression *, set<SgExpression *> *> *ftVarsMap, vector<SgStatement *> &intraStm);
                         /**
                          * Get the variable declaration for a given variable name from the init. section.
                          * A declaration is added to the init. section in the event it previosly hasn't.
                          * @param varName The name of the variable.
                          * @param baseType Type of the variable.
                          * @param initExp Initializer of the variable.
                          **/ 
                         SgVariableDeclaration *getIntraVar(string varName, SgType *baseType, SgExpression *initExp, bool declareIfNotFound = true);
                         /**
                          * Add an arbitrary statement to the current end of the intra section.
                          * This is used for taking care of unsupported statements.
                          * @param stm Statement
                          **/ 
                         bool addIntraStatement(SgStatement *stm);
               };

           //Constructor...  
 
             /**
              * Constructor, initializes class.
              * @param redundancyInter The number of redundant computations in inter-core dimension.
              * @param fPHandlerInter Fault-handling policy for faults in inter-core dimension.
              * @param redundancyIntra The number of redundant computations in intra-core dimension.
              * @param fPHandlerIntra Fault-handling policy for faults in inter-core dimension.
              * @param gScope Default global scope.
              * @param bbExecGlobal Basic block for placing global initialization.
              **/                   
             Transform(unsigned int redundancyIntra, unsigned int redundancyIntraOuter, FailurePolicy &fPHandlerIntra,
                       unsigned int redundancyInter = 0, FailurePolicy fPHandlerInter = FailurePolicy(),
                       SgProject *project = NULL, SgGlobal *gScope = NULL) : fPInter(fPHandlerInter), fPIntra(fPHandlerIntra) {

               srand ( time(NULL) );

               this->globalScope = gScope;
               this->project = project;
               this->initPerformed = false;
               this->redundancyInter = redundancyInter;
               this->redundancyIntra = redundancyIntra;
               this->redundancyIntraOuter = redundancyIntraOuter;
               this->fPInter.setGlobalScope(gScope);
               this->fPIntra.setGlobalScope(gScope); 

               if(this->project == NULL)
                    this->project = SageInterface::getProject();
               if(this->globalScope == NULL)
                    this->globalScope = SageInterface::getFirstGlobalScope(this->project);
             }

             ~Transform() {

             }

           //Functions for adding fault tolerance
             /**
              * Create redundant computations for all underlying nodes, recursivly.
              * @param inputNode AST input node.
              * @param closure Container for all side effects and results, needed in order for multiple statements (in a BB) to share a init. / unification stage.
              * @param globalScope Global scope of inputNode, overrides global scope given to class constructor.
              * @returns NULL if "inputNode" was added to closure or a transformed SgNode (possibly "inputNode" itself)
              **/
             SgNode *transformSingle(SgNode *inputNode, FaultHandling_Closure *closure = NULL, SgGlobal *globalScope = NULL);
             /**
              * Create redundant computations for all user specified IR nodes (by visitor).
              * @param startNode Top-most AST input node. If equal to NULL, transformMulti will perform a MemoryPool traversal.
              * @param decisionFunctor Visitor functor. Decides which IR nodes that will be transformed.
              * @param globalScope Global scope of inputNode, overrides global scope given to class constructor.
              **/
             SgNode *transformMulti(SgNode *startNode = NULL, Common::FTVisitor *decisionFunctor = NULL, SgGlobal *globalScope = NULL);

     private:
           //Instance variables...
             bool initPerformed;
             SgProject *project;
             SgGlobal *globalScope;
             unsigned int redundancyInter, redundancyIntra, redundancyIntraOuter;
             FailurePolicy &fPInter, &fPIntra;
           //Misc. functions
             /**
              * Perform one-time initialization that cannot be performed in (de)constructor due to possible side-effects (= FTException)
              **/ 
             void performInitialization();
           //Functions to apply fault tolerance
             bool applyIntraFT(map<SgExpression *, SgExpression *> &sideEffects, 
                               set<SgExpression *> &requireInit,
                               unsigned int redundancyIntraCore, 
                               FaultHandling_Closure *closure, SgGlobal *globalScope);
             bool applyInterFT(map<SgExpression *, SgExpression *> &sideEffects, 
                               set<SgExpression *> &requireInit,
                               unsigned int redundancyInterCore, unsigned int redundancyIntraCore,
                               FaultHandling_Closure *closure, SgGlobal *globalScope);
             bool applyFT(SgStatement *stm, unsigned int redundancyInterCore, unsigned int redundancyIntraCore, FaultHandling_Closure *closure, SgGlobal *globalScope);
   };
};
#endif
