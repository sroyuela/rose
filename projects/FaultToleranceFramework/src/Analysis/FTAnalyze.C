#include <iostream>
#include <stack>

#include <utility>
#include <limits>
#include <yalaa.hpp>
#include <cppad/cppad.hpp>

#include "rose.h"
#include "ft.h"
#include "SymbolicSemantics.h"
#include "YicesSolver.h"

#include "StructAnal.h"

using CppAD::AD;
using namespace std;

typedef yalaa::details::double_iv_t iv_t;
typedef yalaa::traits::interval_traits<iv_t> iv_traits;
static const iv_t ZERO(0.0, 0,0), ONE(1.0, 1.0);
class AA {
     private:
          yalaa::aff_e_d x;
     public:
       //Class functions
          AA() : x(iv_t(numeric_limits<double>::min(), numeric_limits<double>::max())) {}
          AA(const AA &a) : x( iv_t(a.toInterval()) ){ }
          AA(double x) : x(iv_t(x,x)) {}
          AA(yalaa::aff_e_d xin) : x(xin) {}
          AA(double x, double y) : x(iv_t(x, y)) {}
       //Operator handling...
         //Arithmetic
           AA& operator=(const AA &rhs) {x = yalaa::aff_e_d(rhs.toInterval()); return *this;}
           friend AA& operator+=(AA &ex, const AA &scalar) {ex.x += scalar.x;  return ex;}
           friend AA& operator-=(AA &ex, const AA &scalar) {ex.x -= scalar.x;  return ex;}
           friend AA& operator*=(AA &ex, const AA &scalar) {ex.x *= scalar.x;  return ex;}
           friend AA& operator/=(AA &ex, const AA &scalar) {ex.x /= scalar.x;  return ex;}
           friend AA operator+(const AA &x, const AA &y) {return AA(yalaa::aff_e_d(iv_traits::my_add( yalaa::to_iv(x.x), yalaa::to_iv(y.x) )));}                    
           friend AA operator-(const AA &x, const AA &y) {return AA(yalaa::aff_e_d(iv_traits::my_sub( yalaa::to_iv(x.x), yalaa::to_iv(y.x) )));}    
           friend AA operator*(const AA &x, const AA &y) {return AA(yalaa::aff_e_d(iv_traits::my_mul( yalaa::to_iv(x.x), yalaa::to_iv(y.x) )));}    
           friend AA operator/(const AA &x, const AA &y) {
               //filib doesn't handle divide by zero well..
               if(IdenticalZero(y))
                    return AA(yalaa::aff_e_d(iv_t( std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN())));
               else
                    return AA(yalaa::aff_e_d(iv_traits::my_div( yalaa::to_iv(x.x), yalaa::to_iv(y.x) )));
           }          
         //Logical
           friend AA operator&(const AA &x, const AA &y);
           friend bool GreaterThanZero(AA x) {return yalaa::to_iv(x.x).cgt(ZERO);}
           friend bool LessThanZero(AA x) {return yalaa::to_iv(x.x).clt(ZERO);}
           friend bool GreaterThanOrZero(AA x) {return yalaa::to_iv(x.x).cge(ZERO);}
           friend bool LessThanOrZero(AA x) {return yalaa::to_iv(x.x).cle(ZERO);}
           friend bool IdenticalEqualPar(AA x, const AA y) {return (x.x == y.x);}

           friend bool operator<=(const AA &x, const AA &y) {return yalaa::to_iv(x.x).cle( yalaa::to_iv(y.x) );}
           friend bool operator>=(const AA &x, const AA &y) {return yalaa::to_iv(x.x).cge( yalaa::to_iv(y.x) );}
           friend bool operator<(const AA &x, const AA &y) {return yalaa::to_iv(x.x).clt( yalaa::to_iv(y.x) );}
           friend bool operator>(const AA &x, const AA &y) {return yalaa::to_iv(x.x).cgt( yalaa::to_iv(y.x) );}
           friend bool operator==(const AA &x, const AA &y) {return (x.x == y.x);}
           friend bool operator!=(const AA &x, const AA &y) {return !(yalaa::to_iv(x.x) == yalaa::to_iv(y.x));}
           //friend bool operator!(const AA &x) {return (x.x == y.x);}
         //Complex
           friend AA operator-(const AA x) {return AA(yalaa::aff_e_d(iv_traits::my_neg( yalaa::to_iv(x.x) )));}

           friend bool IdenticalOne(const AA x) {return (yalaa::to_iv(x.x) == ONE);}
           friend bool IdenticalZero(const AA x) {return (yalaa::to_iv(x.x) == ZERO);}
           friend int Integer(AA x) {return round( yalaa::to_iv(x.x).mid() );}
         
           friend AA exp(AA x) {return AA(yalaa::aff_e_d( filib::exp(yalaa::to_iv(x.x)) ));}
           friend AA pow(AA x, AA y) {
               iv_t ivL = iv_traits::my_pow( yalaa::to_iv(x.x), yalaa::to_iv(y.x).inf() ),
                    ivU = iv_traits::my_pow( yalaa::to_iv(x.x), yalaa::to_iv(y.x).sup() ); 
               return AA( (ivL.inf() < ivU.inf() ? ivL.inf() : ivU.inf()), 
                          (ivL.sup() < ivU.sup() ? ivL.sup() : ivU.sup()));}
           friend AA asin(AA x) {return AA(yalaa::aff_e_d( filib::asin(yalaa::to_iv(x.x)) ));}
           friend AA acos(AA x) {return AA(yalaa::aff_e_d( filib::acos(yalaa::to_iv(x.x)) ));}
           friend AA atan(AA x) {return AA(yalaa::aff_e_d( filib::atan(yalaa::to_iv(x.x)) ));}
           friend AA log(AA x) {return AA(yalaa::aff_e_d( filib::log(yalaa::to_iv(x.x)) ));}
           friend AA sqrt(AA x) {return AA(yalaa::aff_e_d(iv_traits::my_sqrt(yalaa::to_iv(x.x))));}
           friend AA cos(AA x) {return AA(yalaa::aff_e_d( filib::cos(yalaa::to_iv(x.x)) ));}
           friend AA sin(AA x) {return AA(yalaa::aff_e_d( filib::sin(yalaa::to_iv(x.x)) ));}
           friend AA tan(AA x) {return AA(yalaa::aff_e_d( filib::tan(yalaa::to_iv(x.x)) ));}
           friend AA cosh(AA x) {return AA(yalaa::aff_e_d( filib::cosh(yalaa::to_iv(x.x)) ));}
           friend AA sinh(AA x) {return AA(yalaa::aff_e_d( filib::sinh(yalaa::to_iv(x.x)) ));}
           friend AA tanh(AA x) {return AA(yalaa::aff_e_d( filib::tanh(yalaa::to_iv(x.x)) ));}
           friend AA CondExpOp(CppAD::CompareOp, AA&, AA&, AA&, AA&) {return AA(0.0, 0.0);}
       //Interval handling
         friend std::ostream& operator<<(std::ostream &os, const AA &x) {
               os << "[" << yalaa::to_iv(x.x).inf() << "," << yalaa::to_iv(x.x).sup() << "]";
               return os;
         }
         iv_t toInterval() const {
               return yalaa::to_iv(x);
         }
};

struct ETransform {
       template<size_t nBits> AD<AA> *operator()(SymbolicSemantics::ValueType<nBits> &v) {
               if(typeid(v.expr).name() == "TreeNode *")
                  cout << "Tree";
               else if(typeid(v.expr).name() == "InternalNode *") {
                    InsnSemanticsExpr::InternalNode *iNode = static_cast<InsnSemanticsExpr::InternalNode *>(v.expr);
                    switch(iNode->get_operator()) {
                     case InsnSemanticsExpr::OP_ADD:  
                     case InsnSemanticsExpr::OP_AND:
                     case InsnSemanticsExpr::OP_ASR:
                     case InsnSemanticsExpr::OP_BV_AND:
                     case InsnSemanticsExpr::OP_BV_OR:
                     case InsnSemanticsExpr::OP_BV_XOR:
                     case InsnSemanticsExpr::OP_CONCAT:
                     case InsnSemanticsExpr::OP_EQ:
                     case InsnSemanticsExpr::OP_EXTRACT:
                     case InsnSemanticsExpr::OP_INVERT:
                     case InsnSemanticsExpr::OP_ITE:
                     case InsnSemanticsExpr::OP_LSSB:
                     case InsnSemanticsExpr::OP_MSSB:
                     case InsnSemanticsExpr::OP_NE:
                     case InsnSemanticsExpr::OP_NEGATE:
                     case InsnSemanticsExpr::OP_NOOP:
                     case InsnSemanticsExpr::OP_OR:
                     case InsnSemanticsExpr::OP_ROL:
                     case InsnSemanticsExpr::OP_ROR:
                     case InsnSemanticsExpr::OP_SDIV:
                     case InsnSemanticsExpr::OP_SEXTEND:
                     case InsnSemanticsExpr::OP_SGE:
                     case InsnSemanticsExpr::OP_SGT:
                     case InsnSemanticsExpr::OP_SHL0:
                     case InsnSemanticsExpr::OP_SHL1:
                     case InsnSemanticsExpr::OP_SHR0:
                     case InsnSemanticsExpr::OP_SHR1:
                     case InsnSemanticsExpr::OP_SLE:
                     case InsnSemanticsExpr::OP_SLT:
                     case InsnSemanticsExpr::OP_SMOD:
                     case InsnSemanticsExpr::OP_SMUL:
                     case InsnSemanticsExpr::OP_UDIV:
                     case InsnSemanticsExpr::OP_UEXTEND:
                     case InsnSemanticsExpr::OP_UGE:
                     case InsnSemanticsExpr::OP_UGT:
                     case InsnSemanticsExpr::OP_ULE:
                     case InsnSemanticsExpr::OP_ULT:
                     case InsnSemanticsExpr::OP_UMOD:
                     case InsnSemanticsExpr::OP_UMUL:
                     case InsnSemanticsExpr::OP_ZEROP:
                         break;
                    }
               } else if(typeid(v.expr).name() == "LeafNode *")
                    cout << "Leaf ";
       }
};  

bool FTAnalysis::FTAnalyzeVisitor::targetNode(SgNode *node) {
     if(node == NULL)
          return false;
     switch(node->variantT()) {
      case V_SgAsmBlock:      return true;
      default:                return false;
     }
}

SgNode *FTAnalysis::analyzeSingle(SgNode *inputNode, SymbolicSemantics::Policy *semPolicy) {
     if(inputNode == NULL)
          return false;
     //Handle statement...
       switch(inputNode->variantT()) {
          case V_SgAsmBlock: {
             //Analyze all statements...
               //cout << "Block (" << inputNode << "): " << endl;
               for(Rose_STL_Container<SgAsmStatement*>::iterator it = static_cast<SgAsmBlock *>(inputNode)->get_statementList().begin();
                   it != static_cast<SgAsmBlock *>(inputNode)->get_statementList().end();
                   ++it)
                    analyzeSingle(*it, semPolicy);
               //cout << *semPolicyBlock << endl;
             //Transform into a expr state...
               ETransform et;
               map<string, AD<AA> *> PC;
               for(size_t i = 0; i < SymbolicSemantics::State::n_gprs ; i++)
                   PC[ gprToString((X86GeneralPurposeRegister)i) ] = et( semPolicy->get_state().gpr[i] );
               /*for(size_t i = 0; i < SymbolicSemantics::State::n_segregs; i++)
                   PC[ segregToString((X86SegmentRegister)i) ] = et( semPolicyBlock->get_state().segreg[i] );*/
               for(size_t i = 0; i < SymbolicSemantics::State::n_flags; i++)
                   PC[ flagToString((X86Flag)i) ] = et( semPolicy->get_state().flag[i] );         
             } return inputNode;
          case V_SgAsmFunction:
          case V_SgAsmStaticData:
               return NULL;
          case V_SgAsmArmInstruction:
          case V_SgAsmPowerpcInstruction:
               return NULL;
          case V_SgAsmx86Instruction: {
             //Analyze instruction...
               X86InstructionSemantics<SymbolicSemantics::Policy, SymbolicSemantics::ValueType> semantics(*semPolicy);
               try {
                    semantics.processInstruction(static_cast<SgAsmx86Instruction *>(inputNode));
               } catch (X86InstructionSemantics<SymbolicSemantics::Policy, SymbolicSemantics::ValueType>::Exception e) {
                    cout << "Exception occured '" << e << "'" << endl;
               }
             } return inputNode;
          default:
               return inputNode;
     }
}
SgNode *FTAnalysis::analyzeMulti(SgNode *startNode) {
     //Collect nodes...
       if(startNode == NULL)
          return NULL;
     //Analyze selected nodes...
       std::vector<SgAsmInterpretation*> interps = SageInterface::querySubTree<SgAsmInterpretation>(startNode);
       if(interps.empty())
          return NULL;
       BinAnalVist vis(*this);
       BinaryAnalysis::ControlFlow cfgAnal;
       BinaryAnalysis::ControlFlow::Graph cfg = cfgAnal.build_cfg_from_ast<BinaryAnalysis::ControlFlow::Graph>(interps.back());
       typedef boost::graph_traits<BinaryAnalysis::ControlFlow::Graph>::vertices_size_type FGVertexSize;
       typedef boost::graph_traits<BinaryAnalysis::ControlFlow::Graph>::vertex_iterator FGIter;
       //Collect addresses of all BB's
         map<rose_addr_t, FGVertexSize> addrToBBVMap;
         for(std::pair<FGIter, FGIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
               SgAsmBlock *stm = get(boost::vertex_name, cfg, *vP.first);
               addrToBBVMap[stm->get_address()] = get(boost::vertex_index, cfg, *vP.first);
         }
       //Normalize vertices (move edge condition out of BB)
         typedef boost::property<boost::edge_name_t, SgAsmInstruction*> FEdgeName;
         typedef boost::property<boost::vertex_name_t, SgAsmBlock*> FVertexName;
         typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, FVertexName, FEdgeName> FGraph;

         FGraph gNorm(num_vertices(cfg));
         Rose_STL_Container<SgAsmStatement*>::iterator it;
         

         //TODO! What if there are multiple jumps?! Need edge_name = program context for test

         boost::property_map<FGraph, boost::vertex_name_t>::type vertMap = get(boost::vertex_name, gNorm);
         for(std::pair<FGIter, FGIter> vP = vertices(cfg); vP.first != vP.second; ++vP.first) {
            //Initialize state
              YicesSolver smtS;
              smtS.set_linkage(YicesSolver::LM_EXECUTABLE);
              SymbolicSemantics::Policy *semPolicyBlock = new SymbolicSemantics::Policy(&smtS);
            //Build new graph with conditions on edges and BBs - {branch} as vertices
              SgAsmBlock *stm = get(boost::vertex_name, cfg, *vP.first);
              FGVertexSize idBB = get(boost::vertex_index, cfg, *vP.first);
              map<FGVertexSize, SgAsmInstruction *> instMap;
              for(it = stm->get_statementList().end()-1; it != stm->get_statementList().begin(); --it) {
                 if(isSgAsmInstruction(*it) == NULL)
                    break;
                 //Resolve addr of BB pointed to by operand[0]
                   Rose_STL_Container<SgAsmExpression*> &op = static_cast<SgAsmInstruction *>(*it)->get_operandList()->get_operands();
                   FGVertexSize idTargetBB;
                   bool foundTarget = false;
                   if(op.size() > 0) { 
                      //... for 32 bit addrs
                        if(op[0]->variantT() != V_SgAsmDoubleWordValueExpression) {
                          uint32_t a32 = static_cast<SgAsmDoubleWordValueExpression *>(op[0])->get_value();
                          if(addrToBBVMap.find(a32) != addrToBBVMap.end()) {
                             idTargetBB = addrToBBVMap.find(a32)->second;
                             //cout << unparseInstructionWithAddress(static_cast<SgAsmInstruction *>(*it)) << " {" << a32 << ", " << idTargetBB << "}" << endl;
                             foundTarget = true;
                          }
                        }
                      //... for 64 bit addrs
                        if(op[0]->variantT() != V_SgAsmDoubleWordValueExpression) {
                          uint64_t a64 = static_cast<SgAsmQuadWordValueExpression *>(op[0])->get_value();
                          if(addrToBBVMap.find(a64) != addrToBBVMap.end()) {
                             idTargetBB = addrToBBVMap.find(a64)->second;
                             //cout << unparseInstructionWithAddress(static_cast<SgAsmInstruction *>(*it)) << " {" << a64 << ", " << idTargetBB << "}" << endl;
                             foundTarget = true;
                          }
                        }
                   }
                   if(!foundTarget)
                         break;
                 //Apply symbolic analysis to instruction...
                   SgAsmx86Instruction *x86I;
                   if((x86I = isSgAsmx86Instruction(*it)) != NULL)
                      switch(x86I->get_kind()) {
                       case x86_call:   
                       case x86_ja:     
                       case x86_jae:    
                       case x86_jb:     
                       case x86_jbe:    
                       case x86_jcxz:   
                       case x86_je:     
                       case x86_jecxz:  
                       case x86_jg:     
                       case x86_jge:    
                       case x86_jl:     
                       case x86_jle:    
                       case x86_jmp:
                       case x86_jmpe:   
                       case x86_jne:
                       case x86_jno:
                       case x86_jns:
                       case x86_jo:
                       case x86_jpe:
                       case x86_jpo:   
                       case x86_jrcxz:  
                       case x86_js:
                       case x86_loop:
                       case x86_loopnz:
                       case x86_loopz:
                       case x86_syscall:
                       case x86_sysenter:
                       case x86_sysexit:
                       case x86_sysret:
                              instMap[idTargetBB] = x86I;
                              break;
                       case x86_ret:
                       case x86_retf:
                       default:
                              break;
                      }
              }
            //Add vertex name property
              if(it != stm->get_statementList().end())
                stm->get_statementList().erase(it, stm->get_statementList().end()-1);
              vertMap[vertex(idBB, gNorm)] = stm;
            //Create edge...
              boost::graph_traits<FGraph>::edge_descriptor e;
              boost::property_map<FGraph, boost::edge_name_t>::type edgeMap = get(boost::edge_name, gNorm);
              for(map<FGVertexSize, SgAsmInstruction *>::iterator it = instMap.begin(); it != instMap.end(); ++it) {
                   //cout << idBB << " : " << it->first << endl;
                   boost::graph_traits<FGraph>::vertex_descriptor v1 = vertex(idBB, gNorm), v2 = vertex(it->first, gNorm);
                   ROSE_ASSERT(v1 != boost::graph_traits<FGraph>::null_vertex());
                   ROSE_ASSERT(v2 != boost::graph_traits<FGraph>::null_vertex());
                   ROSE_ASSERT(!edge(v1,v2,gNorm).second);

                   e = add_edge(v1, v2, gNorm).first;
                   edgeMap[e] = it->second;
              }
            //Create all other edges...
              typename boost::graph_traits<BinaryAnalysis::ControlFlow::Graph>::out_edge_iterator oI, oE;
              for(tie(oI, oE) = boost::out_edges(*vP.first, cfg); oI != oE; ++oI) {
                  FGVertexSize idTBB = get(boost::vertex_index, cfg, target(*oI,cfg));
                  if( instMap.find(idTBB) == instMap.end() ) {
                      e = add_edge(vertex(idBB, gNorm), vertex(idTBB, gNorm), gNorm).first;
                      edgeMap[e] = NULL;
                  }
              }
            //Determine probability of failure for each instruction...
              analyzeSingle(stm, semPolicyBlock);
       }

       StructAnal sA = StructAnal();
       sA( (StructAnal::FlowGraph &) cfg, sA.get_root( (const StructAnal::FlowGraph &) cfg) );

     return startNode;
}
