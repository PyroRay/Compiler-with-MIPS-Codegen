
#include "codegen.h"
#include "scanner.h"
#include <iostream>

int main() {
  std::vector<Token> testVecToken;
  scan(testVecToken);

  std::cout << "Tokenized:" << std::endl;
  for (auto t : testVecToken) {
    std::cout << t.type << " " << (t.value == "\n" ? "" : t.value) << std::endl;
  }

  generateCode(testVecToken);
}
