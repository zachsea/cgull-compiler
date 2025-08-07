#ifndef BYTECODE_COMPILER_H
#define BYTECODE_COMPILER_H

#include "errors/error_reporter.h"
#include "instructions/ir_class.h"
#include <cgullParser.h>

class BytecodeCompiler {
public:
  BytecodeCompiler(cgullParser::ProgramContext* programCtx,
                   std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
                   std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes,
                   std::unordered_set<antlr4::ParserRuleContext*> expectingStringConversion,
                   std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>> constructorMap);

  void compile();
  void generateBytecode(const std::string& outputDir);
  ErrorReporter& getErrorReporter() { return errorReporter; }

  static std::string typeToJVMType(const std::shared_ptr<Type>& type);

private:
  ErrorReporter errorReporter;
  cgullParser::ProgramContext* programCtx;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap;
  std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes;
  std::unordered_set<antlr4::ParserRuleContext*> expectingStringConversion;
  std::vector<std::shared_ptr<IRClass>> generatedClasses;
  std::unordered_map<PrimitiveType::PrimitiveKind, std::shared_ptr<IRClass>> primitiveWrappers;
  std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>> constructorMap;

  void generateClass(std::basic_ostream<char>& out, const std::shared_ptr<IRClass>& irClass);
  void generateInstruction(std::basic_ostream<char>& out, const std::shared_ptr<IRInstruction>& instruction);
  void generateCallInstruction(std::basic_ostream<char>& out, const std::shared_ptr<IRCallInstruction>& instruction);

  std::shared_ptr<IRClass> getOrCreatePrimitiveWrapper(PrimitiveType::PrimitiveKind kind);
  bool needsPrimitiveWrapper(const std::shared_ptr<Type>& type);
};

#endif // BYTECODE_COMPILER_H
