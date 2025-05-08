#ifndef CODEGEN_H
#define CODEGEN_H

#include "structures.h"

// Parses input string into vector of grammar rules
std::vector<Rule> getRules(std::string input);

// Constructs SLR(1) DFA from transition and reduction tables
SLR1DFA buildDFA(std::string transitions, std::string reductions);

// Converts input stream to token deque
std::deque<Token> convertInput(std::istream &in);

// Converts token vector to token deque
std::deque<Token> convertInput(std::vector<Token> &tokVec);

// Reduces parse trees according to grammar rule
void reduceTrees(std::vector<std::shared_ptr<Treenode>> &trees, const Rule r);

// Reduces parser states according to DFA and grammar rule  
void reduceStates(std::vector<int> &states, const Rule r, SLR1DFA &dfa);

// Shifts next token and updates parser state
void shift(std::deque<Token> &tokens,
           std::vector<std::shared_ptr<Treenode>> &trees,
           std::vector<int> &states, SLR1DFA &dfa);
std::vector<std::shared_ptr<Treenode>>
getDeclarations(std::shared_ptr<Treenode> tree);
std::vector<std::string> getArgTypes(std::shared_ptr<Treenode> tree);
void collectProcedures(std::shared_ptr<Treenode> tree, ProcedureTable &pt);
void checkStatementsAndTests(std::shared_ptr<Treenode> tree);
std::shared_ptr<Treenode> getNode(std::shared_ptr<Treenode>, std::string type);
std::string generateLabel(int number);
void generateCodePrintln();
void generateCodeOther(std::shared_ptr<Treenode> tree, ProcedureTable &pt,
                       std::map<std::string, int> offsetTable);
void generateCodeProcedures(std::shared_ptr<Treenode> tree, ProcedureTable &pt);
int generateCode(std::vector<Token> testVecToken);

#endif // CODEGEN_H
