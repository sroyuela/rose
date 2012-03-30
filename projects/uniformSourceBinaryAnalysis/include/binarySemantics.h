#ifndef BINSEMANTICS_X86
#define BINSEMANTICS_X86

#include <set>
#include <string>
#include <functional>

#include "rose.h"
#include "BinaryControlFlow.h"
#include "BinaryLoader.h"
#include "SymbolicSemantics.h"
#include "YicesSolver.h"

using namespace std;

namespace BT {
  //Helper classes
  struct BinTransException : public std::exception {
          string s;
          BinTransException(string ss) : s(ss) {}
          virtual ~BinTransException() throw() {}
          const char *what() const throw() {return s.c_str();}
  };

  //Semantics state
  class BinTransPolicyX86 : public SymbolicSemantics::Policy {
     public:
          //Helper structures
            template <size_t Len>
            struct SValComp : public std::binary_function<SymbolicSemantics::ValueType<Len>, SymbolicSemantics::ValueType<Len>, bool> {
               bool operator()(const SymbolicSemantics::ValueType<Len> &x, const SymbolicSemantics::ValueType<Len> &y){
                    return false;
               }
            };
            typedef SValComp<32> SVComp;

            struct FilterFunctor : public std::unary_function<unsigned long int, bool>,
                                   public std::unary_function<SymbolicSemantics::ValueType<32>, bool> {
                    virtual bool operator()(unsigned long int i) const {return true;}
                    virtual bool operator()(const SymbolicSemantics::ValueType<32> &addr) {return true;}
            } *filter;
            bool filterWasCreated;
          //Machine dependent extensions
            //Type definitions...
              typedef X86InstructionSemantics<BinTransPolicyX86, SymbolicSemantics::ValueType> InstSemantics;
            //Constants
              static const int GP_REGS = 8;
              static const int SEGS = 6;
              static const int FLAGS = 16;
     private:
            mutable set<unsigned long int> rReg, wReg;
            mutable set<SymbolicSemantics::ValueType<32>, SValComp<32> > rMem, wMem;
     public: 
            //Constructor functions
              BinTransPolicyX86(SMTSolver *solver, FilterFunctor *f) : SymbolicSemantics::Policy(solver) {
                    if(f != NULL) {
                         filterWasCreated = false;
                         filter = f;
                    } else {
                         filterWasCreated = true;
                         filter = new FilterFunctor();
                    }
              }
            //Functions
              bool processInstruction(SgAsmInstruction *i) {
               //Initialize semantics
                 rReg.clear();
                 wReg.clear();
                 rMem.clear();
                 wMem.clear();
                 InstSemantics semantics(*this);
               //Error check...
                 if(i->variantT() != V_SgAsmx86Instruction)
                    return false;
               //Execute instruction
                 try {
                      semantics.processInstruction(static_cast<SgAsmx86Instruction *>(i));
                 } catch (InstSemantics::Exception e) {
                      cout << "Exception occured '" << e << "'" << endl;
                      return false;
                 }
                 return true;
              }
            //Overloaded functions
              template <size_t Len> SymbolicSemantics::ValueType<Len> readMemory(X86SegmentRegister segreg, 
                                    const SymbolicSemantics::ValueType<32> &addr, const SymbolicSemantics::ValueType<1> cond) const {
                  if( (*filter)(toIndex(segreg)) && (wReg.find(toIndex(segreg)) == wReg.end()) )
                    rReg.insert(toIndex(segreg));

                  if( (*filter)(addr) && (wMem.find(addr) == wMem.end()) )
                    rMem.insert(addr);
                  return SymbolicSemantics::Policy::readMemory<Len>(segreg, addr, cond);
              }
              template <size_t Len> void writeMemory(X86SegmentRegister segreg, const SymbolicSemantics::ValueType<32> &addr, 
                                    const SymbolicSemantics::ValueType<Len> &data, SymbolicSemantics::ValueType<1> cond) {
                  if( (*filter)(toIndex(segreg)) && (wReg.find(toIndex(segreg)) == wReg.end()) )
                    rReg.insert(toIndex(segreg));

                  if( (*filter)(addr) )
                    wMem.insert(addr);
                  SymbolicSemantics::Policy::writeMemory<Len>(segreg, addr, data, cond);
              }
              SymbolicSemantics::ValueType<32> readGPR(X86GeneralPurposeRegister r) {
                  if( (*filter)(toIndex(r)) && (wReg.find(toIndex(r)) == wReg.end()) )
                    rReg.insert(toIndex(r));
                  return SymbolicSemantics::Policy::readGPR(r);
              }
              void writeGPR(X86GeneralPurposeRegister r, const SymbolicSemantics::ValueType<32> &value) {
                  if( (*filter)(toIndex(r)) )
                    wReg.insert(toIndex(r));
                  return SymbolicSemantics::Policy::writeGPR(r, value);
              }
              SymbolicSemantics::ValueType<16> readSegreg(X86SegmentRegister sr) {
                  if( (*filter)(toIndex(sr)) && (wReg.find(toIndex(sr)) == wReg.end()) )
                    rReg.insert(toIndex(sr));
                  return SymbolicSemantics::Policy::readSegreg(sr);
              }
              void writeSegreg(X86SegmentRegister sr, const SymbolicSemantics::ValueType<16> &value) {
                  if( (*filter)(toIndex(sr)))
                    wReg.insert(toIndex(sr));
                  return SymbolicSemantics::Policy::writeSegreg(sr, value);
              }
              SymbolicSemantics::ValueType<1> readFlag(X86Flag f) {
                  if( (*filter)(toIndex(f)) && (wReg.find(toIndex(f)) == wReg.end()) )
                    rReg.insert(toIndex(f));
                  return SymbolicSemantics::Policy::readFlag(f);
              }
              void writeFlag(X86Flag f, const SymbolicSemantics::ValueType<1> &value) {
                  if( (*filter)(toIndex(f)) )
                    wReg.insert(toIndex(f));
                  return SymbolicSemantics::Policy::writeFlag(f, value);
              }
            //Functions for accessing internal state
              FilterFunctor *get_filter() {return (filterWasCreated ? NULL : filter);} 
              set<unsigned long int> get_rReg() {return rReg;}
              set<unsigned long int> get_wReg() {return wReg;}
              set<SymbolicSemantics::ValueType<32>, SValComp<32> > get_rMem() {return rMem;}
              set<SymbolicSemantics::ValueType<32>, SValComp<32> > get_wMem() {return wMem;}
            //Functions used for resolving reg <-> index
              static unsigned long int toIndex(X86GeneralPurposeRegister r) {return r;}
              static unsigned long int toIndex(X86SegmentRegister sr) {return (GP_REGS + sr);}
              static unsigned long int toIndex(X86Flag f) {return (GP_REGS + SEGS + f);}
              static string toString(unsigned long int i) {
                    if(isGpReg(i))
                         return gprToString(toGpReg(i));
                    else if(isSegReg(i))
                         return segregToString(toSegReg(i));
                    else if(isFlag(i))
                         return flagToString(toFlag(i));
                    else
                         return "unknown";
              }
              static bool isGpReg(unsigned long int i) {return (i > GP_REGS ? false : true);}
              static X86GeneralPurposeRegister toGpReg(unsigned long int i) {
                 if(!isGpReg(i))
                     throw BinTransException("Invalid GP reg");
                 else
                     return (X86GeneralPurposeRegister) i;
              }
              static bool isSegReg(unsigned long int i) {return ((i >= SEGS+GP_REGS) || (i < GP_REGS) ? false : true);}
              static X86SegmentRegister toSegReg(unsigned long int i) {
                if(!isSegReg(i))
                     throw BinTransException("Invalid seg reg");
                 else
                     return (X86SegmentRegister) (i-GP_REGS);
              }
              static bool isFlag(unsigned long int i) {return ((i >= FLAGS+SEGS+GP_REGS) || (i < SEGS+GP_REGS) ? false : true);}
              static X86Flag toFlag(unsigned long int i) {
                 if(!isFlag(i))
                     throw BinTransException("Invalid flag");
                 else
                     return (X86Flag) (i-SEGS-GP_REGS);
              }
              static unsigned long int getNrRegs() {return FLAGS+SEGS+GP_REGS;}
  };
};
#endif
