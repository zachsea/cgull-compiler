#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include "errors/error_reporter.h"
#include "instructions/ir_instruction.h"
#include "symbols/symbol.h"
#include <antlr4-runtime.h>
#include <cgullParser.h>
#include <unordered_map>

class BytecodeCompiler {
public:
  BytecodeCompiler(cgullParser::ProgramContext* programCtx,
                   const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
                   const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes);

  void compile();
  void generateBytecode(std::basic_ostream<char>& out);
  std::vector<IRInstruction> getInstructions() const;
  ErrorReporter& getErrorReporter() { return errorReporter; }

private:
  ErrorReporter errorReporter;
  cgullParser::ProgramContext* programCtx;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;
  std::vector<IRInstruction> instructions;
};

#endif // BYTECODE_COMPILER_H