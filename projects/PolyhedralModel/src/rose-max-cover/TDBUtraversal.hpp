
#ifndef _TDBU_TRAVERSAL_HPP_
#define _TDBU_TRAVERSAL_HPP_


#ifndef DEBUG_ROSE_MAX_COVER
#  define DEBUG_ROSE_MAX_COVER 1
#endif


#include "maths/PPLUtils.hpp"
#include "rose.h"

#include <set>
#include <vector>

// *************************************************
// FIXME move to maths/Polynome.hpp                *

template <class T>
class Expression {};

template <class T>
class LinearExpression : public Expression<T> {};

template <class T>
class BilinearExpression : public Expression<T> {};

template <class T>
class PolynomialExpression : public Expression<T> {};

//                                                 *
// *************************************************

// *************************************************
// FIXME move to .../ArrayDescriptor.hpp           *

template <class T>
struct ArrayShape {
	public:
		unsigned dimension;
		std::vector<Expression<T> *> lower_bound;
		std::vector<Expression<T> *> upper_bound;

	public:
		bool operator == (const ArrayShape<T> &) const;
		bool operator <  (const ArrayShape<T> &) const;
};

template <class T>
class Subscript {
	public:
		std::vector<Expression<T> *> p_subscripts;

	public:
		// TODO

		// TODO Subscript & operator + - * [] (Subscript, int Subscript) = 0;
};

template <class T>
struct SubscriptTranslator {
	public:
		// TODO

	public:
		// TODO

		Subscript<T> & operator () (Subscript<T> &) const;
};

// TODO obtains informations on an array (symbol) that may be use to solve a polynomial access...
template <class T>
class ArrayDescriptor {
	protected:
		std::vector<ArrayShape<T> > p_saw_as;

	public:
		ArrayDescriptor();
		virtual ~ArrayDescriptor();

		ArrayShape<T> & minimalShape() const;

		SubscriptTranslator<T> & toMinimalShape() const;
		SubscriptTranslator<T> & toMaximalShape() const;
};

//                                                 *
// *************************************************

enum constraint_type_e {
	equality,
	greater_equal,
	lesser_equal
};

struct Symbol;
class SymbolTable;

class FunctionTable;

class PolyhedralElement;



class PolyhedralFunction { // Does it need to be a 'PolyhedralElement' ?
	public:
		FunctionTable * function_table;

	protected:
		std::set<SgFunctionDeclaration *> p_decls_set;
		SgFunctionDeclaration * p_first_decl;
		SgFunctionDefinition * p_defn;
		
		std::vector<Symbol *> p_parameters;
		
		SymbolTable * p_top_symbol_table;
		
		Symbol * p_return_symbol;

	public:
		PolyhedralFunction(SgFunctionDeclaration *, FunctionTable *);

		bool isSameFunction(SgFunctionDeclaration *);
		bool isDefined() const;
		
		SgFunctionDefinition * getDefinition();
		SgFunctionDeclaration * getDefDecl();
		SymbolTable * getSymbolTable();
		Symbol * getReturnSymbol();
};

class FunctionTable {
	protected:
		std::set<PolyhedralFunction *> p_function_set; 

	public:
		FunctionTable();
		~FunctionTable();
		
		void add(SgFunctionDeclaration *);
		PolyhedralFunction * get(SgFunctionDeclaration *);
		
		const std::set<PolyhedralFunction *> & get() const;
		std::set<PolyhedralFunction *> & get();
};

// FIXME only 2 kind of access: 1 element (1 Subscript<Symbol>) or 1 domain (2 Subscript<Symbol>). 
class Access {
	public:
		enum AccessType {
			read,
			write
		};

	public:
		PolyhedralElement * associated_element;

	public:
		Access(PolyhedralElement *);
		virtual ~Access();
		
		virtual void print(std::ostream &, std::string = std::string()) = 0;
};

class DataAccess : public Access {
	public:
		AccessType access_type;
		Symbol * symbol;

		Polyhedron accessed_domain;
		
		unsigned int dimension;

	public:
		DataAccess(PolyhedralElement *, AccessType, Symbol *);
		virtual ~DataAccess();
		
		void restrainAccessedDomain(LinearExpression_ppl &, unsigned int, constraint_type_e = equality);
		
		void addAccessDimension();
		unsigned int getAccessDimension() const;
		
		virtual void print(std::ostream &, std::string = std::string());
};

class ParametrizedAccess : public Access {
	public:
		PolyhedralFunction * associated_function;
		std::map<Symbol *, SgExpression *> p_params_args_map;

	public:
		ParametrizedAccess(PolyhedralElement *, PolyhedralFunction *);
		virtual ~ParametrizedAccess();
		
		virtual void print(std::ostream &, std::string = std::string());
};

class PolynomialAccess : public Access {
	public:
		std::vector<PolynomialExpression<Symbol> > p_polynomial_subscript;

	public:
		PolynomialAccess(PolyhedralElement *, AccessType, Symbol *);
		virtual ~PolynomialAccess();

		// TODO
};

class PolyhedralPredicate;
class PolyhedralControl;

class PolyhedralElement {
	public:
		const PolyhedralControl * parent;

	protected:
		std::map<PolyhedralPredicate *, bool> predicate_map;
		std::map<Symbol *, VariableID> symbol_id_map;
		Polyhedron domain;

	public:
		PolyhedralElement(const PolyhedralElement &);

		PolyhedralElement(PolyhedralControl *);
		virtual ~PolyhedralElement();

		void refineDomain(Constraint);
		void addPredicate(PolyhedralPredicate *, bool);

		virtual void print(std::ostream &, std::string = std::string()) = 0;

		VariableID getID(Symbol *);
};

class PolyhedralStatement : public PolyhedralElement {
	public:
		SgStatement * statement;

	protected:
		std::set<Access *> p_access_set;

	public:
		PolyhedralStatement(SgStatement *);
		virtual ~PolyhedralStatement();

		virtual void print(std::ostream &, std::string = std::string());
		
		void addAccess(Access *);
		void addAccess(const std::set<Access *> &);
};

class PolyhedralPredicate : public PolyhedralStatement {
	public:
		Symbol * predicate;

	public:
		PolyhedralPredicate(SgStatement *, SgScopeStatement *, SymbolTable *);
		
		virtual void print(std::ostream &, std::string = std::string());
};

class PolyhedralControl : public PolyhedralElement {
	public:
		PolyhedralControl(PolyhedralControl *); // kind of copy constructor...
		PolyhedralControl(SgNode * = NULL, SymbolTable * = NULL);
		virtual ~PolyhedralControl();
	
		PolyhedralControl * genNext(SgNode *) const;

		virtual void print(std::ostream &, std::string = std::string());
		
		void collectPredicate(PolyhedralStatement *) const;
	
	public:
		SgNode * associated_node;
		
		SymbolTable * symbol_table;

		// empty during top-down, filled by bottom-up needed for scattering generation
		std::vector<PolyhedralElement *> elements_set;
		
		// store a statement that needs to be move before the current control (generated by top-down, consume by bottom-up)
		std::vector<PolyhedralElement *> previous_statements;
		
		// Only valid for loops
		unsigned int stride; // absolute value of the increment
		bool natural_order; // sign of the increment (true for positive)
		
		// Only valid for statically controlled conditionals and if it have a false statement
		PolyhedralControl * false_statement;
};

/**/

enum SymbolRelation {
// for a field
	arrow,
	dot,
// to be use in inter-procedural analysis for arguments and return value
	value, // read
	reference, // read/write
	dereference, // read/write
// for pointers dereference and addresse_of operator
	ptr_deref,
	addr_of,
// if we internally modify a variable (only use case for now is privatization):
	original_variable,
// default
	nothing
};

struct Symbol {
	Symbol(unsigned int);

	SgSymbol * sage_symbol;
	
	Symbol * parent_symbol;
	SymbolRelation relation_with_parent;
	
	bool function_return;
	PolyhedralFunction * associated_function;
	SgFunctionCallExp * original_function_call;

	SgScopeStatement * scope; // NULL means the whole "program"
	
	bool generated; // <=> sage_symbol == NULL && (parent_symbol == NULL || (privatized == true && relation_with_parent == original_variable)
	
	bool iterator; // Only for iterator pattern in FOR loop (or generated).
	bool predicate; // is a data
	bool data; // saw as array or lvalue in an expression statement (or generated) or not declared as scalar
	
	bool function_parameter; // special case for function parameter
	
	bool privatized; // indicated wether or not this symbol refers to a privatized variable
	
	unsigned int dimension; // for array only, TODO remove
	ArrayDescriptor<Symbol> * array_descriptor; // always present, TODO move dimension here
	
	unsigned int id;

	void print(std::ostream &, std::string = std::string());
};

class SymbolTable {
	public:
		SymbolTable * parent_table;
		
		FunctionTable * function_table;
		PolyhedralFunction * current_function;

	protected:
		unsigned int nb_child;
		
		std::set<Symbol *> symbol_set;
		

		Symbol * makeSymbol();
		
		Symbol * symbolFor(SgSymbol *, bool, bool);
		
		bool haveSymbol(Symbol *) const;

	public:
		SymbolTable(PolyhedralFunction *);
		SymbolTable(SymbolTable *);
		~SymbolTable(); // assert if nb_child > 0 && parent->nb_child--
		
		Symbol * genSymbol(SgInitializedName *);
		
		Symbol * genSymbol(SgVarRefExp *, bool = true);
		
		Symbol * genSymbol(SgArrowExp *, bool = true);
		Symbol * genSymbol(SgDotExp *, bool = true);
		Symbol * genSymbol(SgPntrArrRefExp *, bool = true);
		Symbol * genSymbol(SgPointerDerefExp *, bool = true);
		Symbol * genSymbol(SgAddressOfOp *, bool = true);
		
		Symbol * genSymbol(SgFunctionCallExp *, bool = true);

		Symbol * genSymbol(SgExpression *, bool = true);
		
		Symbol * genIteratorSymbol(SgScopeStatement *);
		Symbol * genPredicate(SgScopeStatement *);
		Symbol * genReturnSymbol();
		
		bool isSymbol(SgExpression *, Symbol *);
		
		unsigned int nbrReferencedVariables() const;
		
		VariableID getID(Symbol *) const;
		
		void addControl(PolyhedralControl *);
		
		void print(std::ostream &, std::string = std::string());
		
		void embedSymbolTable(SymbolTable *);
};

/**/

class PolyhedralModelisation {
	protected:
		std::set<PolyhedralElement *> p_results;

	public:
		PolyhedralModelisation();
		
		FunctionTable * traverse(SgProject *);
		
	protected:	
		PolyhedralElement * evaluate(SgNode *, PolyhedralControl *, std::string = std::string());
};

#endif /* _TDBU_TRAVERSAL_HPP_ */

