
#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
struct Rule;
struct SLR1DFA;
struct Token;
struct Treenode;
struct Variable;
struct VariableTable;
struct Procedure;
struct ProcedureTable;

// Represents a grammar production rule with a left-hand side (lhs) and right-hand side (rhs)
struct Rule {
  std::string lhs;
  std::vector<std::string> rhs;
  void print(std::ostream &out = std::cout);
};

// Deterministic Finite Automaton for SLR(1) parsing
struct SLR1DFA {
  std::map<std::pair<int, std::string>, int> transitions;
  std::map<std::pair<int, std::string>, int> reductions;
  void print(std::ostream &out = std::cout);
};

// Represents a lexical token with type and value
struct Token {
  std::string type;
  std::string value;
  void print(std::ostream &out = std::cout);
};

// Node in the abstract syntax tree containing parsing information
struct Treenode {
  bool terminal;
  Rule NTrule;
  Token Ttoken;
  std::string type;
  std::vector<std::shared_ptr<Treenode>> children;

  Treenode(Rule NTrule);
  Treenode(Token Ttoken);
  ~Treenode();

  std::shared_ptr<Treenode> getChild(std::string lhs, int n = 1);
  void annotateTypes(ProcedureTable &pt, VariableTable &vt);
  void print(std::ostream &out = std::cout, std::string prefix = "");
  void debugPrint(std::ostream &out = std::cout, std::string prefix = "");
};

// Represents a variable entry in the symbol table
struct Variable {
  std::string name;
  std::string type;
  Variable(std::shared_ptr<Treenode> tree);
  void print(std::ostream &out = std::cout);
};

// Table storing all variables in a particular scope
struct VariableTable {
  std::map<std::string, Variable> table;
  void add(Variable v);
  Variable get(std::string name);
  void print(std::ostream &out = std::cout);
};

// Represents a procedure/function with its signature and symbol table
struct Procedure {
  std::string name;
  std::vector<std::string> signature;
  VariableTable symbolTable;
  Procedure(std::shared_ptr<Treenode> tree);
  void print(std::ostream &out = std::cout);
};

// Table storing all procedures in the program
struct ProcedureTable {
  std::map<std::string, Procedure> table;
  void add(Procedure p);
  Procedure get(std::string name);
  void print(std::ostream &out = std::cout);
};

#endif // STRUCTURES_H
