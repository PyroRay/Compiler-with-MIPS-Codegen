#include "codegen.h"
#include "mipsinstr.h"
#include "wlp4data.h"
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

/**************** Code Generation Implementation ****************/
/*
 * This file implements the code generation phase of the compiler.
 * It translates the abstract syntax tree into MIPS assembly code.
 * The code generator handles:
 * - Function/procedure generation
 * - Expression evaluation
 * - Control flow (if/while)
 * - Memory management
 * - Register allocation
 */

/*
 * Label Management
 * - label_set: Maintains set of used labels to prevent duplicates
 * - functionlabel_map: Maps function names to their assembly labels
 */
std::unordered_set<std::string> label_set{"print", "init", "new", "delete",
                                          "main"};
std::map<std::string, std::string> functionlabel_map;

void Rule::print(std::ostream &out) {
  out << lhs << " ";
  if (rhs.empty()) {
    out << ".EMPTY";
  } else {
    bool first = true;
    for (const auto &it : rhs) {
      if (first) {
        first = false;
        out << it;
      } else {
        out << " " << it;
      }
    }
  }
  out << std::endl;
}

void SLR1DFA::print(std::ostream &out) {
  out << "Transitions:" << std::endl;
  for (const auto &it : transitions) {
    out << it.first.first << " " << it.first.second << " " << it.second
        << std::endl;
  }
  out << "Reductions:" << std::endl;
  for (const auto &it : reductions) {
    out << it.first.first << " " << it.second << " " << it.first.second
        << std::endl;
  }
}

void Token::print(std::ostream &out) {
  out << type << ' ' << value << std::endl;
}

Treenode::Treenode(Rule NTrule) : terminal{false}, NTrule(NTrule) {}

Treenode::Treenode(Token Ttoken) : terminal{true}, Ttoken(Ttoken) {}

Treenode::~Treenode() {}

std::shared_ptr<Treenode> Treenode::getChild(std::string lhs, int n) {
  int index = 1;

  for (auto &it : children) {
    if (it->terminal) {
      if (it->Ttoken.type == lhs && n == index) {
        return it;
      } else if (it->Ttoken.type == lhs) {
        index++;
      }
    } else {
      if (it->NTrule.lhs == lhs && n == index) {
        return it;
      } else if (it->NTrule.lhs == lhs) {
        index++;
      }
    }
  }
  return nullptr;
}

void Treenode::annotateTypes(ProcedureTable &pt, VariableTable &vt) {
  for (auto &it : children) {
    it->annotateTypes(pt, vt);
  }
  if (!terminal) {
    if (NTrule.lhs == "expr") {
      if (NTrule.rhs.size() == 1) {
        if (NTrule.rhs[0] == "term") {
          type = children[0]->type;
        }
      } else if (NTrule.rhs.size() == 3) {
        if (NTrule.rhs[0] == "expr" && NTrule.rhs[1] == "PLUS" &&
            NTrule.rhs[2] == "term") {
          std::string exprType = children[0]->type;
          std::string termType = children[2]->type;
          if (exprType == "int" && termType == "int") {
            type = "int";
          } else if (exprType == "int*" && termType == "int") {
            type = "int*";
          } else if (exprType == "int" && termType == "int*") {
            type = "int*";
          } else {
            throw std::runtime_error("expr 'PLUS' derived type error");
          }
        } else if (NTrule.rhs[0] == "expr" && NTrule.rhs[1] == "MINUS" &&
                   NTrule.rhs[2] == "term") {
          std::string exprType = children[0]->type;
          std::string termType = children[2]->type;
          if (exprType == "int" && termType == "int") {
            type = "int";
          } else if (exprType == "int*" && termType == "int") {
            type = "int*";
          } else if (exprType == "int*" && termType == "int*") {
            type = "int";
          } else {
            throw std::runtime_error("expr 'PLUS' derived type error");
          }
        }
      }
    } else if (NTrule.lhs == "term") {
      if (NTrule.rhs.size() == 1) {
        if (NTrule.rhs[0] == "factor") {
          type = children[0]->type;
        }
      } else if (NTrule.rhs.size() == 3) {
        type = "int";
        if (children[0]->type != "int" || children[2]->type != "int") {
          throw std::runtime_error("invalid term or factor in term expression");
        }
      }
    } else if (NTrule.lhs == "factor") {
      if (NTrule.rhs.size() == 1) {
        if (NTrule.rhs[0] == "ID") {
          Variable v = vt.get(children[0]->Ttoken.value);
          type = v.type;
        } else if (NTrule.rhs[0] == "NUM") {
          type = "int";
        } else if (NTrule.rhs[0] == "NULL") {
          type = "int*";
        }
      } else if (NTrule.rhs.size() == 2) {
        if (NTrule.rhs[0] == "AMP" && NTrule.rhs[1] == "lvalue") {
          type = "int*";
          if (children[1]->type != "int") {
            throw std::runtime_error("invalid '&' address retrieval");
          }
        } else if (NTrule.rhs[0] == "STAR" && NTrule.rhs[1] == "factor") {
          type = "int";
          if (children[1]->type != "int*") {
            throw std::runtime_error("invalid '*' address retrieval");
          }
        }
      } else if (NTrule.rhs.size() == 3) {
        if (NTrule.rhs[0] == "LPAREN" && NTrule.rhs[1] == "expr" &&
            NTrule.rhs[2] == "RPAREN") {
          type = children[1]->type;
        } else if (NTrule.rhs[0] == "ID" && NTrule.rhs[1] == "LPAREN" &&
                   NTrule.rhs[2] == "RPAREN") {
          if (vt.table.find(children[0]->Ttoken.value) != vt.table.end()) {
            throw std::runtime_error("function call on local variable");
          }
          Procedure p = pt.get(children[0]->Ttoken.value);
          if (p.signature.size() != 0) {
            throw std::runtime_error("invalid parameters");
          }
          type = "int";
        }
      } else if (NTrule.rhs.size() == 4) {
        if (NTrule.rhs[0] == "ID" && NTrule.rhs[1] == "LPAREN" &&
            NTrule.rhs[2] == "arglist" && NTrule.rhs[3] == "RPAREN") {
          if (vt.table.find(children[0]->Ttoken.value) != vt.table.end()) {
            throw std::runtime_error("function call on local variable");
          }
          Procedure p = pt.get(children[0]->Ttoken.value);
          std::vector<std::string> argTypes = getArgTypes(children[2]);
          if (p.signature.size() != argTypes.size()) {
            throw std::runtime_error("invalid parameters incorrect amount");
          }
          for (int i = 0; i < argTypes.size(); i++) {
            if (argTypes[i] != p.signature[i]) {
              throw std::runtime_error("invalid parameters incorrect types");
            }
          }
          type = "int";
        }
      } else if (NTrule.rhs.size() == 5) {
        if (NTrule.rhs[0] == "NEW" && NTrule.rhs[1] == "INT" &&
            NTrule.rhs[2] == "LBRACK" && NTrule.rhs[3] == "expr" &&
            NTrule.rhs[4] == "RBRACK") {
          type = "int*";
          if (children[3]->type != "int") {
            throw std::runtime_error("invalid 'new' address retrieval");
          }
        }
      }
    } else if (NTrule.lhs == "lvalue") {
      if (NTrule.rhs.size() == 1) {
        if (NTrule.rhs[0] == "ID") {
          Variable v = vt.get(children[0]->Ttoken.value);
          type = v.type;
        }
      } else if (NTrule.rhs.size() == 2) {
        if (NTrule.rhs[0] == "STAR" && NTrule.rhs[1] == "factor") {
          type = "int";
          if (children[1]->type != "int*") {
            throw std::runtime_error("invalid '*' address retrieval");
          }
        }
      } else if (NTrule.rhs.size() == 3) {
        if (NTrule.rhs[0] == "LPAREN" && NTrule.rhs[1] == "lvalue" &&
            NTrule.rhs[2] == "RPAREN") {
          type = children[1]->type;
        }
      }
    }
  }
}

void Treenode::print(std::ostream &out, std::string prefix) {
  if (terminal) {
    Ttoken.print(out);
  } else {
    NTrule.print(out);
  }
  for (const auto &child : children) {
    child->print(out);
  }
}

void Treenode::debugPrint(std::ostream &out, std::string prefix) {
  if (terminal) {
    Ttoken.print(out);
  } else {
    NTrule.print(out);
  }
  for (size_t i = 0; i < children.size(); ++i) {
    if (i == children.size() - 1) {
      out << prefix << "╰─";
      children[i]->debugPrint(out, prefix + "  ");
    } else {
      out << prefix << "├─";
      children[i]->debugPrint(out, prefix + "│ ");
    }
  }
}

Variable::Variable(std::shared_ptr<Treenode> tree) {
  name = tree->children[1]->Ttoken.value;
  type = (tree->children[0]->children.size() == 1 ? "int" : "int*");
}

void Variable::print(std::ostream &out) {
  out << type << " " << name << std::endl;
}

void VariableTable::add(Variable v) {
  if (table.find(v.name) != table.end()) {
    throw std::runtime_error("duplicate variable declaration");
  } else {
    table.insert({v.name, v});
  }
}

Variable VariableTable::get(std::string name) {
  if (table.find(name) == table.end()) {
    throw std::runtime_error("use of undeclared variable");
  } else {
    return table.at(name);
  }
}

void VariableTable::print(std::ostream &out) {
  out << "VARIABLES:" << std::endl;
  for (auto it : table) {
    out << it.first << " : ";
    it.second.print(out);
  }
}

Procedure::Procedure(std::shared_ptr<Treenode> tree) {
  std::vector<std::shared_ptr<Treenode>> locDCLS =
      getDeclarations(tree->getChild("dcls"));
  std::vector<std::shared_ptr<Treenode>> params;

  // acquires the procedure's parameters
  if (tree->NTrule.lhs == "procedure") {
    params = getDeclarations(tree->getChild("params", 1));
  } else {
    params.push_back(tree->getChild("dcl", 1));
    params.push_back(tree->getChild("dcl", 2));
    if (params[1]->children[0]->children.size() != 1) {
      throw std::runtime_error("main invalid second parameter declaration");
    }
  }

  // for each parameter, we push back the signature with the type name
  for (auto it : params) {
    Variable v = Variable(it);
    symbolTable.add(v);
    signature.push_back(v.type);
  }

  // set name to the procedure's name
  name = tree->children[1]->Ttoken.value;

  for (auto it : locDCLS) {
    Variable v = Variable(it);
    symbolTable.add(v);
  }
}

void Procedure::print(std::ostream &out) {
  out << "Procedure " << name << ":" << std::endl;
  out << "  Signature: ";
  for (auto it : signature) {
    out << it << " ";
  }
  out << "\n  Declarations:\n";
  for (auto it : symbolTable.table) {
    out << "    " << it.first << " : ";
    it.second.print(out);
  }
}

void ProcedureTable::add(Procedure p) {
  if (table.find(p.name) != table.end()) {
    throw std::runtime_error("duplicate procedure declaration");
  } else {
    table.insert({p.name, p});
  }
}

Procedure ProcedureTable::get(std::string name) {
  if (table.find(name) == table.end()) {
    throw std::runtime_error("use of undeclared procedure");
  } else {
    return table.at(name);
  }
}

void ProcedureTable::print(std::ostream &out) {
  out << "PROCEDURES:" << std::endl;
  for (auto it : table) {
    out << it.first << " : ";
    it.second.print(out);
  }
}

std::vector<Rule> getRules(std::string input) {
  std::vector<std::string> processedInput;
  std::string currLine;
  std::istringstream iss{WLP4_CFG};

  std::getline(iss, currLine);
  while (std::getline(iss, currLine)) {
    processedInput.push_back(currLine);
  }

  std::vector<Rule> rules;
  for (int i = 0; i < processedInput.size(); i++) {
    std::istringstream iss{processedInput[i]};
    std::string lhs;
    iss >> lhs;

    std::vector<std::string> rhs;
    std::string temp;
    while (iss >> temp) {
      if (temp == ".EMPTY")
        continue;
      rhs.push_back(temp);
    }

    Rule r;
    r.lhs = lhs;
    r.rhs = rhs;
    rules.push_back(r);
  }
  return rules;
}

SLR1DFA buildDFA(std::string transitions, std::string reductions) {
  std::istringstream tiss{transitions};
  std::istringstream riss{reductions};
  std::map<std::pair<int, std::string>, int> transitionsMap;
  std::map<std::pair<int, std::string>, int> reductionsMap;
  std::string currLine;

  getline(tiss, currLine);
  while (getline(tiss, currLine)) {
    std::istringstream iss{currLine};
    int currState;
    std::string symbol;
    int nextState;

    iss >> currState >> symbol >> nextState;
    transitionsMap.insert({std::make_pair(currState, symbol), nextState});
  }

  getline(riss, currLine);
  while (getline(riss, currLine)) {
    std::istringstream iss{currLine};
    int currState;
    std::string symbol;
    int ruleNum;

    iss >> currState >> ruleNum >> symbol;
    reductionsMap.insert({std::make_pair(currState, symbol), ruleNum});
  }

  SLR1DFA dfa;
  dfa.transitions = transitionsMap;
  dfa.reductions = reductionsMap;
  return dfa;
}

std::deque<Token> convertInput(std::istream &in) {
  std::deque<Token> tokens;
  std::string currLine;
  Token tok;
  tok.type = "BOF";
  tok.value = "BOF";
  tokens.push_back(tok);
  while (getline(in, currLine)) {
    std::istringstream iss{currLine};
    std::string type;
    std::string value;
    iss >> type >> value;
    tok.type = type;
    tok.value = value;
    tokens.push_back(tok);
  }
  tok.type = "EOF";
  tok.value = "EOF";
  tokens.push_back(tok);
  return tokens;
}

std::deque<Token> convertInput(std::vector<Token> &tokVec) {
  std::deque<Token> tokens;
  std::string currLine;
  Token tok;
  tok.type = "BOF";
  tok.value = "BOF";
  tokens.push_back(tok);
  for (auto i : tokVec) {
    tokens.push_back(i);
  }
  tok.type = "EOF";
  tok.value = "EOF";
  tokens.push_back(tok);
  return tokens;
}

void reduceTrees(std::vector<std::shared_ptr<Treenode>> &trees, const Rule r) {
  std::shared_ptr<Treenode> newRule = std::make_shared<Treenode>(r);
  int len = r.rhs.size();

  for (int i = trees.size() - len; i < trees.size(); i++) {
    newRule->children.push_back(std::move(trees[i]));
  }

  for (int i = 0; i < len; ++i) {
    trees.pop_back();
  }

  trees.push_back(std::move(newRule));
}

void reduceStates(std::vector<int> &states, const Rule r, SLR1DFA &dfa) {
  int len = r.rhs.size();

  for (int i = 0; i < len; ++i) {
    states.pop_back();
  }

  std::pair<int, std::string> key = std::make_pair(states.back(), r.lhs);
  states.push_back(dfa.transitions[key]);
}

void shift(std::deque<Token> &tokens,
           std::vector<std::shared_ptr<Treenode>> &trees,
           std::vector<int> &states, SLR1DFA &dfa) {
  Token topToken = tokens.front();
  std::shared_ptr<Treenode> newNode = std::make_shared<Treenode>(topToken);
  trees.push_back(std::move(newNode));

  int currState = states.back();
  std::pair<int, std::string> key = std::make_pair(currState, topToken.type);
  if (dfa.transitions.find(key) == dfa.transitions.end()) {
    throw std::runtime_error("No next transition");
  } else {
    states.push_back(dfa.transitions[key]);
  }

  tokens.pop_front();
}

std::vector<std::shared_ptr<Treenode>>
getDeclarations(std::shared_ptr<Treenode> tree) {
  std::vector<std::shared_ptr<Treenode>> declarations;
  if (!tree->terminal) {
    if (tree->NTrule.lhs == "dcls" && tree->NTrule.rhs.size() != 0) {

      std::vector<std::shared_ptr<Treenode>> d =
          getDeclarations(tree->children[1]);
      if (d.size() != 0) {
        if ((d.front()->children[0]->children.size() == 1 &&
             tree->NTrule.rhs[3] == "NULL") ||
            (d.front()->children[0]->children.size() == 2 &&
             tree->NTrule.rhs[3] == "NUM")) {
          throw std::runtime_error("incorrect assignment in declaration");
        }
      }
      declarations.insert(declarations.end(), d.begin(), d.end());
      std::vector<std::shared_ptr<Treenode>> d1 =
          getDeclarations(tree->children[0]);
      declarations.insert(declarations.end(), d1.begin(), d1.end());
    } else {
      if (tree->NTrule.lhs == "dcl") {
        declarations.push_back(tree);
      } else {
        for (auto child : tree->children) {
          std::vector<std::shared_ptr<Treenode>> d = getDeclarations(child);
          declarations.insert(declarations.end(), d.begin(), d.end());
        }
      }
    }
  }
  return declarations;
}

std::vector<std::string> getArgTypes(std::shared_ptr<Treenode> tree) {
  std::vector<std::string> argTypes;
  if (!tree->terminal) {
    if (tree->NTrule.lhs == "expr") {
      argTypes.push_back(tree->type);
    } else {
      for (auto child : tree->children) {
        std::vector<std::string> at = getArgTypes(child);
        argTypes.insert(argTypes.end(), at.begin(), at.end());
      }
    }
  }
  return argTypes;
}

void collectProcedures(std::shared_ptr<Treenode> tree, ProcedureTable &pt) {
  if (!tree->terminal) {
    if (tree->NTrule.lhs == "procedure" || tree->NTrule.lhs == "main") {
      Procedure p{tree};
      pt.add(p);
      tree->annotateTypes(pt, p.symbolTable);
      checkStatementsAndTests(tree);
      if (tree->getChild("expr")->type != "int") {
        throw std::runtime_error(
            "expression derived from procedure/main must return int");
      }
    } else if (tree->NTrule.lhs == "procedures") {
      for (auto child : tree->children) {
        collectProcedures(child, pt);
      }
    }
  }
}

void checkStatementsAndTests(std::shared_ptr<Treenode> tree) {
  if (!tree->terminal) {
    if (tree->NTrule.lhs == "statement") {
      if (tree->NTrule.rhs.size() == 4) {
        if (tree->NTrule.rhs[0] == "lvalue" &&
            tree->NTrule.rhs[1] == "BECOMES" && tree->NTrule.rhs[2] == "expr" &&
            tree->NTrule.rhs[3] == "SEMI") {
          if (tree->children[0]->type != tree->children[2]->type) {
            throw std::runtime_error(
                "lvalue and expression must have the same type");
          }
        }
      } else if (tree->NTrule.rhs.size() == 5) {
        if (tree->NTrule.rhs[0] == "PRINTLN" &&
            tree->NTrule.rhs[1] == "LPAREN" && tree->NTrule.rhs[2] == "expr" &&
            tree->NTrule.rhs[3] == "RPAREN" && tree->NTrule.rhs[4] == "SEMI") {
          if (tree->children[2]->type != "int") {
            throw std::runtime_error(
                "expression derived from PRINTLN must be of type int");
          }
        } else if (tree->NTrule.rhs[0] == "DELETE" &&
                   tree->NTrule.rhs[1] == "LBRACK" &&
                   tree->NTrule.rhs[2] == "RBRACK" &&
                   tree->NTrule.rhs[3] == "expr" &&
                   tree->NTrule.rhs[4] == "SEMI") {
          if (tree->children[3]->type != "int*") {
            throw std::runtime_error(
                "expression derived from DELETE must be of type int*");
          }
        }
      }
    } else if (tree->NTrule.lhs == "test") {
      if (tree->NTrule.rhs.size() == 3) {
        if (tree->NTrule.rhs[0] == "expr" && tree->NTrule.rhs[2] == "expr") {
          if (tree->children[0]->type != tree->children[2]->type) {
            throw std::runtime_error(
                "expression derived from test must have the same type");
          }
        }
      }
    }
  }

  for (auto &it : tree->children) {
    checkStatementsAndTests(it);
  }
}

std::shared_ptr<Treenode> getNode(std::shared_ptr<Treenode> tree,
                                  std::string type) {
  if (tree->terminal) {
    if (tree->Ttoken.type == type) {
      return tree;
    } else {
      return nullptr;
    }
  } else {
    if (tree->NTrule.lhs == type) {
      return tree;
    } else {
      std::shared_ptr<Treenode> find;
      for (auto it : tree->children) {
        find = getNode(it, type);
        if (find) {
          break;
        }
      }
      return find;
    }
  }
}

std::string generateLabel() {
  std::string newLabel;
  do {
    newLabel = "";
    for (int i = 0; i < 10; ++i) {
      newLabel += 'a' + rand() % 26;
    }
  } while (label_set.count(newLabel));
  label_set.insert(newLabel);
  return newLabel;
}

/*
 * generateCodeOther: Generates MIPS assembly for expressions and statements
 * - Handles arithmetic operations with type checking
 * - Manages memory operations and pointer arithmetic
 * - Implements control flow structures (if/while)
 * - Processes function calls with parameter passing
 */
void generateCodeOther(std::shared_ptr<Treenode> tree, ProcedureTable &pt,
                       std::map<std::string, int> offsetTable) {
  if (!tree->terminal) {
    if (tree->NTrule.lhs == "expr") {
      // CODE GENERATION FOR EXPRESSIONS
      std::shared_ptr<Treenode> expression = tree->getChild("expr");
      std::shared_ptr<Treenode> term = tree->getChild("term");
      if (tree->NTrule.rhs.size() > 1) {
        std::shared_ptr<Treenode> operation = tree->getChild("PLUS")
                                                  ? tree->getChild("PLUS")
                                                  : tree->getChild("MINUS");
        // push original 5 to stack since it will be modified
        push(5);
        // generate code for expression
        generateCodeOther(expression, pt, offsetTable);
        // store output into $3
        push(3);
        // generate code for expression
        generateCodeOther(term, pt, offsetTable);
        // load output from expression into $5
        pop(5);
        // output code for operation
        if (expression->type == "int" && term->type == "int") {
          if (operation->Ttoken.type == "PLUS") {
            Add(3, 5, 3);
          } else if (operation->Ttoken.type == "MINUS") {
            Subtract(3, 5, 3);
          } else {
            // THIS SHOULD NEVER HAPPEN
            throw std::runtime_error("valid operations not found");
          }
        } else if (expression->type == "int*" && term->type == "int") {
          if (operation->Ttoken.type == "PLUS") {
            Multiply(3, 4);
            Mflo(3);
            Add(3, 5, 3);
          } else if (operation->Ttoken.type == "MINUS") {
            Multiply(3, 4);
            Mflo(3);
            Subtract(3, 5, 3);
          } else {
            // THIS SHOULD NEVER HAPPEN
            throw std::runtime_error("valid operations not found");
          }
        } else if (expression->type == "int" && term->type == "int*") {
          if (operation->Ttoken.type == "PLUS") {
            Multiply(5, 4);
            Mflo(5);
            Add(3, 5, 3);
          } else if (operation->Ttoken.type == "MINUS") {
            Multiply(5, 4);
            Mflo(5);
            Subtract(3, 5, 3);
          } else {
            // THIS SHOULD NEVER HAPPEN
            throw std::runtime_error("valid operations not found");
          }
        } else if (expression->type == "int*" && term->type == "int*") {
          if (operation->Ttoken.type == "MINUS") {
            Subtract(3, 5, 3);
            Divide(3, 4);
            Mflo(3);
          } else {
            throw std::runtime_error("cannot add two int*'s");
          }
        }
        // pop 5 from stack to restore
        pop(5);
      } else {
        if (term) {
          generateCodeOther(term, pt, offsetTable);
        } else {
          // this should never happen
          throw std::runtime_error("expression must have at least one term");
        }
      }
    } else if (tree->NTrule.lhs == "term") {
      // CODE GENERATION FOR TERMS
      std::shared_ptr<Treenode> term = tree->getChild("term");
      std::shared_ptr<Treenode> factor = tree->getChild("factor");
      if (tree->NTrule.rhs.size() > 1) {
        std::shared_ptr<Treenode> operation = tree->getChild("STAR");
        if (!operation) {
          operation = tree->getChild("SLASH");
        }
        if (!operation) {
          operation = tree->getChild("PCT");
        }
        push(5);
        // generate code for term
        generateCodeOther(term, pt, offsetTable);
        // store output into $3
        push(3);
        // generate code for expression
        generateCodeOther(factor, pt, offsetTable);
        // load output from expression into $5
        pop(5);
        // output code for operation
        if (operation->Ttoken.type == "STAR") {
          Multiply(5, 3);
          Mflo(3);
        } else if (operation->Ttoken.type == "SLASH") {
          Divide(5, 3);
          Mflo(3);
        } else if (operation->Ttoken.type == "PCT") {
          Divide(5, 3);
          Mfhi(3);
        }
        pop(5);
      } else {
        if (factor) {
          generateCodeOther(factor, pt, offsetTable);
        }
      }
    } else if (tree->NTrule.lhs == "factor") {
      // CODE GENERATION FOR FACTORS
      if (tree->NTrule.rhs.size() == 1) {
        if (tree->NTrule.rhs[0] == "ID") {
          std::string ID = tree->getChild("ID")->Ttoken.value;
          // std::cout << "CHILD ID IS: " << ID << std::endl;
          Load(3, 29, offsetTable[ID]);
        } else if (tree->NTrule.rhs[0] == "NUM") {
          int val = std::stoi(tree->getChild("NUM")->Ttoken.value);
          Lis(3);
          Word(val);
        } else if (tree->NTrule.rhs[0] == "NULL") {
          // IDK IF THIS IS CORRECT PROBABLY IS MAYBE ISNT
          int val = 1;
          Lis(3);
          Word(val);
        }
      } else if (tree->NTrule.rhs.size() == 2) {
        if (tree->NTrule.rhs[0] == "AMP" && tree->NTrule.rhs[1] == "lvalue") {
          std::shared_ptr<Treenode> lvalue = tree->getChild("lvalue");
          while (lvalue->children.size() == 3) {
            lvalue = lvalue->getChild("lvalue");
          }
          if (lvalue->children.size() == 1) {
            std::string name = lvalue->getChild("ID")->Ttoken.value;
            int offset = offsetTable[name];
            Lis(3);
            Word(offset);
            Add(3, 29, 3);
          } else if (lvalue->children.size() == 2) {
            generateCodeOther(lvalue->getChild("factor"), pt, offsetTable);
          }
        } else if (tree->NTrule.rhs[0] == "STAR" &&
                   tree->NTrule.rhs[1] == "factor") {
          // !!! MEMORY THING NOT SURE IF WORKS !!!
          generateCodeOther(tree->getChild("factor"), pt, offsetTable);
          Load(3, 3, 0);
        }
      } else if (tree->NTrule.rhs.size() == 3) {
        if (tree->NTrule.rhs[0] == "LPAREN" && tree->NTrule.rhs[1] == "expr" &&
            tree->NTrule.rhs[2] == "RPAREN") {
          std::shared_ptr<Treenode> expression = tree->getChild("expr");
          generateCodeOther(expression, pt, offsetTable);
          // !!! NOT 100% SURE THIS WILL WORK !!!
        } else if (tree->NTrule.rhs[0] == "ID" &&
                   tree->NTrule.rhs[1] == "LPAREN" &&
                   tree->NTrule.rhs[2] == "RPAREN") {
          // factor ID LPAREN RPAREN
          push(29);
          push(31);
          Lis(31);
          Word(functionlabel_map[tree->getChild("ID")->Ttoken.value]);
          Jalr(31);
          pop(31);
          pop(29);
        }
      } else if (tree->NTrule.rhs.size() == 4) {
        if (tree->NTrule.rhs[0] == "ID" && tree->NTrule.rhs[1] == "LPAREN" &&
            tree->NTrule.rhs[2] == "arglist" &&
            tree->NTrule.rhs[3] == "RPAREN") {
          // factor ID LPAREN arglist RPAREN
          push(29);
          push(31);
          std::shared_ptr<Treenode> arglist = tree->getChild("arglist");
          int args = 0;
          // need to iterate through the arglist and push any arguments to stack
          while (arglist) {
            generateCodeOther(arglist->getChild("expr"), pt, offsetTable);
            push(3);
            args++;
            arglist = arglist->getChild("arglist");
          }
          Lis(31);
          Word(functionlabel_map[tree->getChild("ID")->Ttoken.value]);
          Jalr(31);
          // pop the args we sent
          for (int i = 0; i < args; i++) {
            pop();
          }
          pop(31);
          pop(29);
        }
      } else if (tree->NTrule.rhs.size() == 5) {
        // factor NEW INT LBRACK expr RBRACK
        generateCodeOther(tree->getChild("expr"), pt, offsetTable);
        std::string endlabel = generateLabel();
        push(1);
        Add(1, 3, 0);
        push(31);
        Lis(31);
        Word("new");
        Jalr(31);
        pop(31);
        pop(1);
        Bne(3, 0, endlabel);
        Lis(3);
        Word(1);
        Label(endlabel);
      }
    } else if (tree->NTrule.lhs == "statements") {
      // CODE GENERATION FOR STATEMENTS
      // std::cout << "IN STATEMENTS" << std::endl;
      if (tree->NTrule.rhs.size() == 0) {
      } else if (tree->NTrule.rhs.size() == 2) {
        // std::cout << "STATEMENTS SIZE 2" << std::endl;
        generateCodeOther(tree->getChild("statements"), pt, offsetTable);
        generateCodeOther(tree->getChild("statement"), pt, offsetTable);
      }
    } else if (tree->NTrule.lhs == "statement") {
      // CODE GENERATION FOR STATEMENT
      // statement lvalue BECOMES expr SEMI
      // std::cout << "IN STATEMENT" << std::endl;
      if (tree->NTrule.rhs.size() == 4) {
        std::shared_ptr<Treenode> lvalue = tree->getChild("lvalue");
        std::shared_ptr<Treenode> expr = tree->getChild("expr");
        while (lvalue->children.size() == 3) {
          lvalue = lvalue->getChild("lvalue");
        }
        if (lvalue->children.size() == 1) {
          std::string name = lvalue->getChild("ID")->Ttoken.value;
          int offset = offsetTable[name];
          generateCodeOther(expr, pt, offsetTable);
          Store(3, 29, offset);
        } else if (lvalue->children.size() == 2) {
          // !!! NEED THIS LATER BUT NOT NOW !!!
          push(5);
          generateCodeOther(lvalue->getChild("factor"), pt, offsetTable);
          push(3);
          generateCodeOther(expr, pt, offsetTable);
          pop(5);
          Store(3, 5, 0);
          pop(5);
        }
      } else if (tree->NTrule.rhs.size() == 5) {
        if (tree->NTrule.rhs[0] == "PRINTLN" &&
            tree->NTrule.rhs[1] == "LPAREN" && tree->NTrule.rhs[2] == "expr" &&
            tree->NTrule.rhs[3] == "RPAREN" && tree->NTrule.rhs[4] == "SEMI") {
          // statement PRINTLN LPAREN expr RPAREN SEMI
          generateCodeOther(tree->getChild("expr"), pt, offsetTable);
          push(1);
          Add(1, 3, 0);
          push(31);
          Lis(31);
          Word("print");
          Jalr(31);
          pop(31);
          pop(1);
          // generateCodePrintln();
        } else if (tree->NTrule.rhs[0] == "DELETE" &&
                   tree->NTrule.rhs[1] == "LBRACK" &&
                   tree->NTrule.rhs[2] == "RBRACK" &&
                   tree->NTrule.rhs[3] == "expr" &&
                   tree->NTrule.rhs[4] == "SEMI") {
          // statement DELETE LBRACK RBRACK expr SEMI
          generateCodeOther(tree->getChild("expr"), pt, offsetTable);
          std::string skiplabel = generateLabel();
          push(1);
          Lis(1);
          Word(1);
          Beq(3, 1, skiplabel);
          Add(1, 3, 0);
          push(31);
          Lis(31);
          Word("delete");
          Jalr(31);
          pop(31);
          Label(skiplabel);
          pop(1);
          // CHECK IF THIS WORKS WHEN I WAKE UP
        }
      } else if (tree->NTrule.rhs.size() == 7) {
        // statement WHILE LPAREN test RPAREN LBRACE statements RBRACE
        std::string beginlabel = generateLabel();
        std::string endlabel = generateLabel();
        // beginning of the while loop (before test is run)
        Label(beginlabel);
        // generates code for test
        generateCodeOther(tree->getChild("test"), pt, offsetTable);
        // if test is false, jump to end of while loop
        Beq(3, 0, endlabel);
        // otherwise generates code for statements
        generateCodeOther(tree->getChild("statements"), pt, offsetTable);
        // jump to beginning of while loop
        Beq(0, 0, beginlabel);
        // end of the while loop
        Label(endlabel);

        // !!! CONTINUE THIS IN A BIT !!!
      } else if (tree->NTrule.rhs.size() == 11) {
        // statement
        // IF LPAREN test RPAREN LBRACE statements RBRACE
        // ELSE LBRACE statements RBRACE

        // label to jump to if test is false
        std::string elselabel = generateLabel();
        std::string endlabel = generateLabel();
        generateCodeOther(tree->getChild("test"), pt, offsetTable);
        Beq(3, 0, elselabel);
        generateCodeOther(tree->getChild("statements"), pt, offsetTable);
        Beq(0, 0, endlabel);
        Label(elselabel);
        generateCodeOther(tree->getChild("statements", 2), pt, offsetTable);
        Label(endlabel);
      }
    } else if (tree->NTrule.lhs == "test") {
      std::shared_ptr<Treenode> left = tree->getChild("expr");
      std::shared_ptr<Treenode> right = tree->getChild("expr", 2);
      // get the operation since its always the 2nd child in test
      std::string op = tree->children[1]->Ttoken.type;
      push(5);
      // result in $5
      generateCodeOther(left, pt, offsetTable);
      push(3);
      // result in $3
      generateCodeOther(right, pt, offsetTable);
      pop(5);
      if (op == "EQ") {
        std::string labeltrue = generateLabel();
        std::string labelfalse = generateLabel();
        // test expr EQ expr
        Bne(3, 5, labelfalse); // check this later not sure if pc is already +1
        Lis(3);
        Word(1);
        Beq(0, 0, labeltrue);
        Label(labelfalse);
        Add(3, 0, 0);
        Label(labeltrue);
      } else if (op == "NE") {
        std::string labeltrue = generateLabel();
        std::string labelfalse = generateLabel();
        // test expr NE expr
        Beq(3, 5, labelfalse); // check this later not sure if pc is already +1
        Lis(3);
        Word(1);
        Beq(0, 0, labeltrue);
        Label(labelfalse);
        Add(3, 0, 0);
        Label(labeltrue);
      } else if (op == "LT") {
        // test expr LT expr
        if (left->type == "int" && right->type == "int") {
          Slt(3, 5, 3);
        } else {
          Sltu(3, 5, 3);
        }
      } else if (op == "LE") {
        // test expr LE expr
        if (left->type == "int" && right->type == "int") {
          Slt(3, 3, 5); // will be 0 if less than equal
          Lis(5);
          Word(1);
          Slt(3, 3, 5);
        } else {
          Sltu(3, 3, 5);
          Lis(5);
          Word(1);
          Slt(3, 3, 5);
        }
      } else if (op == "GE") {
        // test expr GE expr
        if (left->type == "int" && right->type == "int") {
          Slt(3, 5, 3); // will be 0 if less than equal
          Lis(5);
          Word(1);
          Slt(3, 3, 5);
        } else {
          Sltu(3, 5, 3);
          Lis(5);
          Word(1);
          Slt(3, 3, 5);
        }
      } else if (op == "GT") {
        // test expr GT expr
        if (left->type == "int" && right->type == "int") {
          Slt(3, 3, 5);
        } else {
          Sltu(3, 3, 5);
        }
      }
      pop(5);
    }
  }
}

void generateCodeProcedures(std::shared_ptr<Treenode> tree,
                            ProcedureTable &pt) {
  std::map<std::string, int> offsetTable;
  int offset = 0;
  int localVarCount = 0;

  std::shared_ptr<Treenode> procedure = tree;
  // std::cout << "PROCEDURE LHS: " << procedure->NTrule.lhs << std::endl;
  if (procedure->NTrule.lhs == "procedure") {
    // get the name of the procedure to use as a label
    std::string proclabel = procedure->getChild("ID")->Ttoken.value;
    if (label_set.count(proclabel)) {
      // functionlabel_map
      std::string customlabel = generateLabel();
      label_set.insert(customlabel);
      functionlabel_map.insert({proclabel, customlabel});
    } else {
      label_set.insert(proclabel);
      functionlabel_map.insert({proclabel, proclabel});
    }
    // outputs the label
    Label(functionlabel_map[proclabel]);
    // tree we will use to iterate through the procedure params
    std::shared_ptr<Treenode> params = procedure->getChild("params");
    if (params->NTrule.rhs.size() == 0) {
      // if there are no params, offset is set to be 0
      offset = 0;
    } else {
      // paramlist dcl
      // paramlist dcl COMMA paramlist
      std::vector<std::string> paramlist;
      params = params->getChild("paramlist");
      // loops through and gets all the variable names
      // also adds to the offset counter
      while (params) {
        std::shared_ptr<Treenode> dcl = params->getChild("dcl");
        // store the name of the declared variable for later
        paramlist.push_back(dcl->getChild("ID")->Ttoken.value);
        offset += 4;
        params = params->getChild("paramlist");
      }
      // pushes the variables and offset to the offset table
      for (auto it : paramlist) {
        offsetTable.insert(std::make_pair(it, offset));
        offset -= 4;
        // we dont add 1 to localVarCount since we won't pop these
      }
      // offset should be 0 at this point
      // we don't push anything to stack since we assume the caller does that

      // debug print for parameters
      // for (auto it : paramlist) {
      //   std::cout << it << ": " << offsetTable[it] << std::endl;
      // }
    }

    // set value of frame pointer - offset table now has params
    // at this points, $29 + 8 is param 1 and $29 + 4 is param 2
    Subtract(29, 30, 4);
  } else {
    // generate label for main
    Label("main");
    // set offset to 8 for wain
    offset = 8;
    // collects param and declaration nodes from main
    std::shared_ptr<Treenode> param1 = procedure->getChild("dcl");
    std::shared_ptr<Treenode> param2 = procedure->getChild("dcl", 2);

    // runs init if first param of main is of type int*
    if (param1->type == "int*") {
      push(31);
      Lis(31);
      Word("init");
      Jalr(31);
      pop(31);
    } else {
      push(2);
      Lis(2);
      Word(0);
      push(31);
      Lis(31);
      Word("init");
      Jalr(31);
      pop(31);
      pop(2);
    }

    // add param1 of wain to offsetTable
    offsetTable.insert(
        std::make_pair(param1->getChild("ID")->Ttoken.value, offset));
    offset -= 4;
    localVarCount++;
    // generate code to store $1 to stack
    push(1);

    // add param2 of wain to offsetTable
    offsetTable.insert(
        std::make_pair(param2->getChild("ID")->Ttoken.value, offset));
    offset -= 4;
    localVarCount++;
    // generate code to store $2 to stack
    push(2);

    // set value of frame pointer - offset table now has params
    // at this points, $29 + 8 is param 1 and $29 + 4 is param 2
    Subtract(29, 30, 4);
  }

  // now we do the dcls stuff :sob:
  std::shared_ptr<Treenode> dcls = procedure->getChild("dcls");
  std::vector<std::pair<std::string, int>> declarations;

  while (dcls) {
    std::shared_ptr<Treenode> dcl = dcls->getChild("dcl");
    std::shared_ptr<Treenode> becomesNum = dcls->getChild("NUM");
    std::shared_ptr<Treenode> becomesNull = dcls->getChild("NULL");
    if (dcl && becomesNum) {
      // gets the ID of the variable and the value it is assigned
      declarations.push_back(
          std::make_pair(dcl->getChild("ID")->Ttoken.value,
                         std::stoi(becomesNum->Ttoken.value)));
    } else if (dcl && becomesNull) {
      // gets the ID of the variable and gives it the value 1
      declarations.push_back(
          std::make_pair(dcl->getChild("ID")->Ttoken.value, 1));
    }
    dcls = dcls->getChild("dcls");
  }

  // // print out the declarations stack for debugging
  // std::cout << "declarations:\n";
  // while (!declarations.empty()) {
  //   std::cout << declarations.back().first << " = ";
  //   std::cout << declarations.back().second << "\n";
  //   declarations.pop_back();
  // }

  // push the variables acquired to the offset table
  // generate code for them as well
  while (!declarations.empty()) {
    std::pair<std::string, int> var = declarations.back();
    declarations.pop_back();
    offsetTable.insert(std::make_pair(var.first, offset));
    offset -= 4;
    localVarCount++;
    Lis(3);
    Word(var.second);
    push(3);
  }

  // // prints the offset table (not in order) for debugging
  // std::cout << "Current offsetTable:\n";
  // for (auto it : offsetTable) {
  //   std::cout << it.first << " " << it.second << "\n";
  // }

  // generate code for statements
  generateCodeOther(procedure->getChild("statements"), pt, offsetTable);

  // wain->debugPrint();
  // generate code for the return function
  generateCodeOther(procedure->getChild("expr"), pt, offsetTable);

  for (int i = 0; i < localVarCount; i++) {
    pop();
  }

  // end procedure
  Jr(31);
}

int generateCode(std::vector<Token> testVecToken) {
  /*
   * Code Generation Pipeline:
   * 1. Initialize context-free grammar rules from WLP4 specification
   * 2. Construct SLR(1) DFA for parsing using transition/reduction tables
   * 3. Convert input tokens into parse-ready format
   */
  std::vector<Rule> CFG = getRules(WLP4_CFG);
  SLR1DFA dfa = buildDFA(WLP4_TRANSITIONS, WLP4_REDUCTIONS);
  std::deque<Token> tokens = convertInput(testVecToken);

  // create stacks for both trees and states
  std::vector<std::shared_ptr<Treenode>> treeStack;
  std::vector<int> stateStack;
  // populate stateStack with the element 0
  stateStack.push_back(0);

  // try catch loop to detect runtime errors from shift
  try {
    // while the input token vector is not empty
    while (tokens.size() != 0) {
      // keep repeating until no longer in reduction state
      while (true) {
        // grab current state and input token at top of stack
        int currState = stateStack.back();
        Token topToken = tokens.front();

        // set up key with current state and the type of topToken
        std::pair<int, std::string> key =
            std::make_pair(currState, topToken.type);
        if (dfa.reductions.find(key) != dfa.reductions.end()) {
          Rule r = CFG[dfa.reductions[key]];
          reduceTrees(treeStack, r);
          reduceStates(stateStack, r, dfa);
        } else {
          break;
        }
      }
      shift(tokens, treeStack, stateStack, dfa);
    }
  } catch (std::runtime_error &err) {
    std::cerr << "ERROR in setup: " << err.what() << '\n';
    return 1;
  }

  ProcedureTable pt;

  // this part reduces the tree and collects all the procedures in the tree
  // checking for errors in syntax while doing so
  try {
    reduceTrees(treeStack, CFG[0]);
    collectProcedures(treeStack[0]->getChild("procedures"), pt);
    // treeStack[0]->debugPrint();
  } catch (std::runtime_error &err) {
    std::cerr << "ERROR in processing: " << err.what() << '\n';
    return 1;
  }

  // code generation
  try {
    // treeStack[0]->debugPrint();
    std::cout << ".import print\n.import init\n.import new\n.import delete\n";
    // sets up $4 to hold the value 4
    Lis(4);
    Word(4);
    // jump to main
    Beq(0, 0, "main");
    // set a variable procedures to modify as we iterate through the tree
    std::shared_ptr<Treenode> procedures = treeStack[0]->getChild("procedures");
    // iterates through the tree to find any procedure nodes and main nodes
    // calls generateCodeProcedures on the nodes it finds
    while (procedures) {
      std::shared_ptr<Treenode> procedure =
          procedures->getChild("procedure") ? procedures->getChild("procedure")
                                            : procedures->getChild("main");
      // procedure->debugPrint();
      generateCodeProcedures(procedure, pt);
      procedures = procedures->getChild("procedures");
    }
  } catch (std::runtime_error &err) {
    std::cerr << "ERROR in code generation: " << err.what() << '\n';
    return 1;
  }
  return 0;
}