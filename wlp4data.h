#ifndef WLP4DATA_H
#define WLP4DATA_H

#include <string>

// Context-free grammar rules for WLP4
extern const std::string WLP4_CFG;

// DFA state transition table
extern const std::string WLP4_TRANSITIONS;

// Reduction rules for parser
extern const std::string WLP4_REDUCTIONS;

// Complete DFA specification
extern const std::string WLP4_DFA;

// Combined grammar and DFA data
extern const std::string WLP4_COMBINED;

#endif