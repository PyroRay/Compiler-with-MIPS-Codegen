#include "scanner.h"
#include <algorithm>
#include <bitset>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

const std::string DFAstring = R"(
.STATES
start
ID!
ZERO!
invalidnum
NUM!
LPAREN!
RPAREN!
LBRACE!
RBRACE!
LBRACK!
RBRACK!
BECOMES!
PLUS!
MINUS!
STAR!
SLASH!
PCT!
AMP!
COMMA!
SEMI!
LT!
GT!
LE!
GE!
EQ!
not
NE!
?WHITESPACE!
?COMMENT!
.TRANSITIONS
start a-z A-Z     ID
ID    a-z A-Z 0-9 ID
start 0 ZERO
ZERO 0-9 invalidnum
start  1-9 NUM
start  -   MINUS
NUM 0-9 NUM
start ( LPAREN
start ) RPAREN
start { LBRACE
start } RBRACE
start [ LBRACK
start ] RBRACK
start = BECOMES
BECOMES = EQ
start + PLUS
start - MINUS
start * STAR
start / SLASH
SLASH / ?COMMENT
start % PCT
start & AMP
start , COMMA
start ; SEMI
start < LT
LT = LE
start > GT
GT = GE
start ! not
not = NE
start       \s \t \n \r ?WHITESPACE
?WHITESPACE \s \t \n \r ?WHITESPACE
start    ; ?COMMENT
?COMMENT \x00-\x09 \x0B \x0C \x0E-\x7F ?COMMENT
)";

const std::string STATES = ".STATES";
const std::string TRANSITIONS = ".TRANSITIONS";
const std::string INPUT = ".INPUT";

class DFA {
  // initial state
  std::pair<std::string, bool> initState;
  // name of current state + accepting characters + name of next state
  std::map<std::pair<std::string, char>, std::pair<std::string, bool>>
      transitionMap;

public:
  // Constructor
  DFA(std::map<std::string, bool> states,
      std::vector<
          std::pair<std::pair<std::string, std::string>, std::vector<char>>>
          transitions);
  // Destructor
  ~DFA();
  // finds and return a state given the name of the state
  std::pair<std::string, bool> findState(std::string stateName);
  // gets the next state given the name of the current state and a char c
  std::pair<std::string, bool> nextState(std::string currState, char c);
  // retrieves the initial state of a DFA
  std::pair<std::string, bool> getInitState() { return initState; }
};

/**************** Function Declarations ****************/

/* String Processing Functions */
// Checks if a string consists of exactly one character
bool isChar(std::string s);

// Checks if a string represents a valid character range (e.g. "a-z")
bool isRange(std::string s);

// Normalizes whitespace in a string by removing leading/trailing spaces
// and reducing multiple spaces to single spaces
std::string squish(std::string s);

/* Hex Conversion Functions */
// Converts a hex character (0-9, a-f, A-F) to its decimal value (0-15)
int hexToNum(char c);

// Converts a hexadecimal string to its binary representation
std::string hexToBin(std::string hex);

// Converts a decimal number (0-15) to its hex character representation
char numToHex(int d);

/* Character Encoding Functions */
// Processes escape sequences in a string (e.g. \n, \t) into actual characters
std::string escape(std::string s);

// Converts special characters into their escape sequence representation
std::string unescape(std::string s);

/* Token Processing Functions */
// Determines the specific token type for identifiers (e.g. if "int" -> "INT")
std::string getIDType(std::string s);

// Validates tokens against language constraints (e.g. number ranges)
void checkTokenRestriction(Token t);

// Converts an input string into a sequence of tokens using a DFA
std::vector<Token> tokenize(DFA *a, std::string in);

// Constructs a DFA from an input stream containing state and transition definitions
DFA createDFA(std::istream &in);

/****************Function Definitions****************/

DFA::DFA(std::map<std::string, bool> states,
         std::vector<
             std::pair<std::pair<std::string, std::string>, std::vector<char>>>
             transitions)
    : initState{std::make_pair<std::string, bool>("start", 0)} {

  for (auto it : transitions) {
    for (auto it2 : it.second)
      transitionMap.insert(
          std::pair<std::pair<std::string, char>, std::pair<std::string, bool>>(
              std::pair<std::string, char>(it.first.first, it2),
              std::make_pair(it.first.second, states[it.first.second])));
  }
}

DFA::~DFA() {}

std::pair<std::string, bool> DFA::nextState(std::string currState, char c) {
  // for each transition in 'transitions' check if the current state is the
  // starting state of the transition
  std::pair<std::string, char> key = std::pair<std::string, char>(currState, c);
  if (transitionMap.find(key) != transitionMap.end()) {
    return transitionMap[key];
  }
  throw std::runtime_error("NO TRANSITION TO NEXT STATE");
}

DFA createDFA(std::istream &in) {
  std::map<std::string, bool> states;
  std::vector<std::pair<std::pair<std::string, std::string>, std::vector<char>>>
      transitions;
  std::string s;

  // Read states
  while (true) {
    if (!(std::getline(in, s))) {
      throw std::runtime_error("Expected " + STATES +
                               ", but found end of input.");
    }
    s = squish(s);
    if (s == STATES) {
      break;
    }
    if (!s.empty()) {
      throw std::runtime_error("Expected " + STATES + ", but found: " + s);
    }
  }
  // Read transitions
  while (true) {
    if (!(in >> s)) {
      throw std::runtime_error(
          "Unexpected end of input while reading state set: " + TRANSITIONS +
          "not found.");
    }
    if (s == TRANSITIONS) {
      break;
    }
    bool accepting = false;
    if (s.back() == '!' && s.length() > 1) {
      accepting = true;
      states.insert(std::make_pair(s.substr(0, s.length() - 1), accepting));
    }
    states.insert(std::make_pair(s, accepting));
  }
  std::getline(in, s); // Consume newline after ".TRANSITIONS"

  //Complex section: Parses transition lines, handles character ranges and escape sequences.
  while (true) {
    if (!(std::getline(in, s))) {
      break;
    }
    s = squish(s);
    if (s == INPUT) {
      break;
    }
    std::string lineStr = s;
    std::stringstream line(lineStr);
    std::vector<std::string> lineVec;
    while (line >> s) {
      lineVec.push_back(s);
    }
    if (lineVec.empty()) {
      continue;
    }
    if (lineVec.size() < 3) {
      throw std::runtime_error("Incomplete transition line: " + lineStr);
    }
    std::string fromState = lineVec.front();
    std::string toState = lineVec.back();
    std::vector<char> charVec;
    for (int i = 1; i < lineVec.size() - 1; ++i) {
      std::string charOrRange = escape(lineVec[i]);
      if (isChar(charOrRange)) {
        char c = charOrRange[0];
        if (c < 0 || c > 127) {
          throw std::runtime_error(
              "Invalid (non-ASCII) character in transition line: " + lineStr +
              "\n" + "Character " + unescape(std::string(1, c)) +
              " is outside ASCII range");
        }
        charVec.push_back(c);
      } else if (isRange(charOrRange)) {
        for (char c = charOrRange[0];
             charOrRange[0] <= c && c <= charOrRange[2]; ++c) {
          charVec.push_back(c);
        }
      } else {
        throw std::runtime_error("Expected character or range, but found " +
                                 charOrRange +
                                 " in transition line: " + lineStr);
      }
    }
    transitions.push_back(
        std::make_pair(std::make_pair(fromState, toState), charVec));
  }

  return DFA{states, transitions};
}

bool isChar(std::string s) { return s.length() == 1; }

bool isRange(std::string s) { return s.length() == 3 && s[1] == '-'; }

std::string squish(std::string s) {
  std::stringstream ss(s);
  std::string token;
  std::string result;
  std::string space = "";
  while (ss >> token) {
    result += space;
    result += token;
    space = " ";
  }
  return result;
}

int hexToNum(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else if ('a' <= c && c <= 'f') {
    return 10 + (c - 'a');
  } else if ('A' <= c && c <= 'F') {
    return 10 + (c - 'A');
  }
  throw std::runtime_error("Invalid hex digit!");
}

std::string hexToBin(std::string hex) {
  std::string bin = "";
  for (auto it : hex) {
    char check = toupper(it);
    if (check == '0') {
      bin += "0000";
    } else if (check == '1') {
      bin += "0001";
    } else if (check == '2') {
      bin += "0010";
    } else if (check == '3') {
      bin += "0011";
    } else if (check == '4') {
      bin += "0100";
    } else if (check == '5') {
      bin += "0101";
    } else if (check == '6') {
      bin += "0110";
    } else if (check == '7') {
      bin += "0111";
    } else if (check == '8') {
      bin += "1000";
    } else if (check == '9') {
      bin += "1001";
    } else if (check == 'A') {
      bin += "1010";
    } else if (check == 'B') {
      bin += "1011";
    } else if (check == 'C') {
      bin += "1100";
    } else if (check == 'D') {
      bin += "1101";
    } else if (check == 'E') {
      bin += "1110";
    } else if (check == 'F') {
      bin += "1111";
    }
  }
  return bin;
}

char numToHex(int d) { return (d < 10 ? d + '0' : d - 10 + 'A'); }

std::string escape(std::string s) {
  std::string p;
  for (int i = 0; i < s.length(); ++i) {
    if (s[i] == '\\' && i + 1 < s.length()) {
      char c = s[i + 1];
      i = i + 1;
      if (c == 's') {
        p += ' ';
      } else if (c == 'n') {
        p += '\n';
      } else if (c == 'r') {
        p += '\r';
      } else if (c == 't') {
        p += '\t';
      } else if (c == 'x') {
        if (i + 2 < s.length() && isxdigit(s[i + 1]) && isxdigit(s[i + 2])) {
          if (hexToNum(s[i + 1]) > 8) {
            throw std::runtime_error("Invalid escape sequence \\x" +
                                     std::string(1, s[i + 1]) +
                                     std::string(1, s[i + 2]) +
                                     ": not in ASCII range (0x00 to 0x7F)");
          }
          char code = hexToNum(s[i + 1]) * 16 + hexToNum(s[i + 2]);
          p += code;
          i = i + 2;
        } else {
          p += c;
        }
      } else if (isgraph(c)) {
        p += c;
      } else {
        p += s[i];
      }
    } else {
      p += s[i];
    }
  }
  return p;
}

std::string unescape(std::string s) {
  std::string p;
  for (int i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == ' ') {
      p += "\\s";
    } else if (c == '\n') {
      p += "\\n";
    } else if (c == '\r') {
      p += "\\r";
    } else if (c == '\t') {
      p += "\\t";
    } else if (!isgraph(c)) {
      std::string hex = "\\x";
      p += hex + numToHex((unsigned char)c / 16) +
           numToHex((unsigned char)c % 16);
    } else {
      p += c;
    }
  }
  return p;
}

std::string getIDType(std::string s) {
  if (s == "int") {
    return "INT";
  } else if (s == "wain") {
    return "WAIN";
  } else if (s == "if") {
    return "IF";
  } else if (s == "else") {
    return "ELSE";
  } else if (s == "while") {
    return "WHILE";
  } else if (s == "println") {
    return "PRINTLN";
  } else if (s == "return") {
    return "RETURN";
  } else if (s == "new") {
    return "NEW";
  } else if (s == "delete") {
    return "DELETE";
  } else if (s == "NULL") {
    return "NULL";
  } else {
    return "ID";
  }
}

void checkTokenRestriction(Token t) {
  // looks for restrictions provided in instructions and throws an error if
  // anything is out-of-range
  if (t.type == "NUM") {
    try {
      long tempLong = std::stol(t.value);
      if (tempLong > 2147483647) {
        throw std::runtime_error("NUM OUT-OF-RANGE");
      }
    } catch (std::out_of_range err) {
      throw std::runtime_error("NUM OUT-OF-RANGE");
    }
  }
}

std::vector<Token> tokenize(DFA *a, std::string in) {
  // stores the current state (always the initial state at the start)
  std::pair<std::string, bool> currState = a->getInitState();
  // constructs a token to be two empty strings
  Token t = {"", ""};
  std::string tokenValue;
  // vector to store valid tokens
  std::vector<Token> vTokens;
  vTokens.reserve(in.length() / 2);

  int index = 0;

  // goes through each character in the given string and forms the token
  while (index < in.length()) {
    // stores the first character of the string
    char tempChar = in[index];
    try {
      // gets the next state and stores as a temporary state
      std::pair<std::string, bool> tempState =
          a->nextState(currState.first, tempChar);
      // sets the current state to be the temporary state
      currState = tempState;
      // adds the character to the value of the current token
      tokenValue += tempChar;
      index++;
      // if any errors are thrown, we catch it as there is no next state
    } catch (std::runtime_error &e) {
      // if the current state is an accepting state
      if (currState.second) {
        // we set the type and check for restrictions
        if (currState.first == "ID") {
          t.type = getIDType(tokenValue);
        } else {
          t.type = (currState.first == "ZERO" ? "NUM" : currState.first);
        }
        t.value = tokenValue;
        checkTokenRestriction(t);
        // if the type begins with '?' we discard the token, otherwise we add it
        // too the vector of valid tokens
        if (t.type[0] != '?') {
          vTokens.push_back(t);
        }
        // reset the state and the current token
        currState = a->getInitState();
        tokenValue.clear();
        t = {"", ""};
        // if the state is not accepting, we throw a scan failure
      } else {
        throw std::runtime_error("SCAN FAILURE");
      }
    }
  }
  if (currState.second) {
    t.type = (currState.first == "ZERO" ? "DECINT" : currState.first);
    t.value = tokenValue;
    checkTokenRestriction(t);
    if (t.type[0] != '?') {
      vTokens.push_back(t);
    }
  } else {
    throw std::runtime_error("SCAN FAILURE");
  }
  return vTokens;
}

bool validLine(std::vector<Token> tokensCheck) {
  if (tokensCheck[0].type == "DOTID" && tokensCheck[0].value == ".word" &&
      tokensCheck.size() == 2) {
    if (tokensCheck[1].type == "DECINT" || tokensCheck[1].type == "HEXINT" ||
        tokensCheck[1].type == "ID")
      return true;
  } else if (tokensCheck[0].type == "ID" &&
             (tokensCheck[0].value == "add" || tokensCheck[0].value == "sub" ||
              tokensCheck[0].value == "slt" || tokensCheck[0].value == "sltu" ||
              tokensCheck[0].value == "beq" || tokensCheck[0].value == "bne") &&
             tokensCheck.size() == 6) {
    if (tokensCheck[1].type == "REGISTER" && tokensCheck[2].type == "COMMA" &&
        tokensCheck[3].type == "REGISTER" && tokensCheck[4].type == "COMMA" &&
        tokensCheck[5].type == "REGISTER") {
      return true;
    } else if (tokensCheck[1].type == "REGISTER" &&
               tokensCheck[2].type == "COMMA" &&
               tokensCheck[3].type == "REGISTER" &&
               tokensCheck[4].type == "COMMA" &&
               (tokensCheck[5].type == "ID" ||
                tokensCheck[5].type == "DECINT" ||
                tokensCheck[5].type == "HEXINT")) {
      return true;
    }
  } else if (tokensCheck[0].type == "ID" &&
             (tokensCheck[0].value == "mult" ||
              tokensCheck[0].value == "multu" ||
              tokensCheck[0].value == "div" ||
              tokensCheck[0].value == "divu") &&
             tokensCheck.size() == 4) {
    if (tokensCheck[1].type == "REGISTER" && tokensCheck[2].type == "COMMA" &&
        tokensCheck[3].type == "REGISTER") {
      return true;
    }
  } else if (tokensCheck[0].type == "ID" &&
             (tokensCheck[0].value == "mfhi" ||
              tokensCheck[0].value == "mflo" || tokensCheck[0].value == "lis" ||
              tokensCheck[0].value == "jalr" || tokensCheck[0].value == "jr") &&
             tokensCheck.size() == 2) {
    if (tokensCheck[1].type == "REGISTER") {
      return true;
    }
  } else if (tokensCheck[0].type == "ID" &&
             (tokensCheck[0].value == "lw" || tokensCheck[0].value == "sw") &&
             tokensCheck.size() == 7) {
    if (tokensCheck[1].type == "REGISTER" && tokensCheck[2].type == "COMMA" &&
        (tokensCheck[3].type == "HEXINT" || tokensCheck[3].type == "DECINT") &&
        tokensCheck[4].type == "LPAREN" && tokensCheck[5].type == "REGISTER" &&
        tokensCheck[6].type == "RPAREN") {
      return true;
    }
  }
  return false;
}

int scan(std::vector<Token> &testVecToken) {
  std::istringstream iss{DFAstring};
  try {
    DFA newDFA = createDFA(iss);

    std::string testString = "";
    char tempC;
    while (true) {
      tempC = getchar();
      if (tempC == EOF) {
        break;
      }
      testString += tempC;
    }
    testVecToken = tokenize(&newDFA, testString);
  } catch (std::runtime_error &e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
  return 0;
}