#include <string>

void Add(int d, int s, int t);

void Subtract(int d, int s, int t);

void Multiply(int s, int t);

void MultiplyU(int s, int t);

void Divide(int s, int t);

void DivideU(int s, int t);

void Mfhi(int d);

void Mflo(int d);

void Lis(int d);

void Slt(int d, int s, int t);

void Sltu(int d, int s, int t);

void Jr(int s);

void Jalr(int s);

void Beq(int s, int t, std::string label);

void Bne(int s, int t, std::string label);

void Beq(int s, int t, int i);

void Bne(int s, int t, int i);

void Load(int s, int t, int i = 0);

void Store(int s, int t, int i = 0);

void Word(int i);

void Word(std::string label);

void Label(std::string name);

void push(int s);

void pop(int d);

void pop();