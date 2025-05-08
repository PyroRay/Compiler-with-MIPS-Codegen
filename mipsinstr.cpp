#include "mipsinstr.h"
#include <iostream>
#include <string>

// MIPS Assembly Instruction Implementations
// Each function outputs the corresponding MIPS assembly instruction
// Register parameters are referenced as $d, $s, $t in the output
// All instructions are output followed by a newline

// Arithmetic Instructions
void Add(int d, int s, int t) {
  std::cout << "add $" << d << ", $" << s << ", $" << t << "\n";
}

void Subtract(int d, int s, int t) {
  std::cout << "sub $" << d << ", $" << s << ", $" << t << "\n";
}

void Multiply(int s, int t) {
  std::cout << "mult $" << s << ", $" << t << "\n";
}

void MultiplyU(int s, int t) {
  std::cout << "multu $" << s << ", $" << t << "\n";
}

void Divide(int s, int t) { std::cout << "div $" << s << ", $" << t << "\n"; }

void DivideU(int s, int t) { std::cout << "divu $" << s << ", $" << t << "\n"; }

void Mfhi(int d) { std::cout << "mfhi $" << d << "\n"; }

void Mflo(int d) { std::cout << "mflo $" << d << "\n"; }

void Lis(int d) { std::cout << "lis $" << d << "\n"; }

void Slt(int d, int s, int t) {
  std::cout << "slt $" << d << ", $" << s << ", $" << t << "\n";
}

void Sltu(int d, int s, int t) {
  std::cout << "sltu $" << d << ", $" << s << ", $" << t << "\n";
}

void Jr(int s) { std::cout << "jr $" << s << "\n"; }

void Jalr(int s) { std::cout << "jalr $" << s << "\n"; }

void Beq(int s, int t, std::string label) {
  std::cout << "beq $" << s << ", $" << t << ", " + label + "\n";
}

void Bne(int s, int t, std::string label) {
  std::cout << "bne $" << s << ", $" << t << ", " + label + "\n";
}

void Beq(int s, int t, int i) {
  std::cout << "beq $" << s << ", $" << t << ", " << i << "\n";
}

void Bne(int s, int t, int i) {
  std::cout << "bne $" << s << ", $" << t << ", " << i << "\n";
}

void Load(int t, int s, int i) {
  std::cout << "lw $" << t << ", " << i << "($" << s << ")"
            << "\n";
}

void Store(int t, int s, int i) {
  std::cout << "sw $" << t << ", " << i << "($" << s << ")"
            << "\n";
}

void Word(int i) { std::cout << ".word " << i << "\n"; }

void Word(std::string label) { std::cout << ".word " + label + "\n"; }

void Label(std::string name) { std::cout << name + ":\n"; }

void push(int s) {
  Store(s, 30, -4);
  Subtract(30, 30, 4);
}

void pop(int d) {
  Add(30, 30, 4);
  Load(d, 30, -4);
}

void pop() { Add(30, 30, 4); }