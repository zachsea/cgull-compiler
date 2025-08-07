#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include "errors/error_reporter.h"
#include "instructions/ir_class.h"
#include "symbols/symbol.h"
#include <antlr4-runtime.h>
#include <cgullParser.h>
#include <memory>
#include <unordered_map>
#include <vector>

class BytecodeCompiler {
public:
  BytecodeCompiler(cgullParser::ProgramContext* programCtx,
                   std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
                   std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes);

  void compile();
  void generateBytecode(std::basic_ostream<char>& out);
  ErrorReporter& getErrorReporter() { return errorReporter; }

private:
  ErrorReporter errorReporter;
  cgullParser::ProgramContext* programCtx;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;
  std::vector<std::shared_ptr<IRClass>> generatedClasses;

  void generateClass(std::basic_ostream<char>& out, const std::shared_ptr<IRClass>& irClass);
  std::string typeToJVMType(const std::shared_ptr<Type>& type);

  void generateInstruction(std::basic_ostream<char>& out, const std::shared_ptr<IRInstruction>& instruction);
  void generateCallInstruction(std::basic_ostream<char>& out, const std::shared_ptr<IRCallInstruction>& instruction);
};

#endif // BYTECODE_COMPILER_H