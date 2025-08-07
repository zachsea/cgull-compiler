#include "bytecode_compiler.h"
#include "listeners/bytecode_ir_generator_listener.h"
#include <ostream>

BytecodeCompiler::BytecodeCompiler(
    cgullParser::ProgramContext* programCtx,
    const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
    const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes)
    : programCtx(programCtx), scopeMap(scopeMap), expressionTypes(expressionTypes) {}

void BytecodeCompiler::compile() {
  // FIRST PASS: generate IR instructions
  BytecodeIRGeneratorListener irListener(errorReporter, scopeMap, expressionTypes);
  antlr4::tree::ParseTreeWalker walker;
  walker.walk(&irListener, programCtx);
  instructions = irListener.getInstructions();

  for (const auto& instruction : instructions) {
    std::cout << "Instruction: " << instruction.opCode << " Operands: ";
    for (const auto& operand : instruction.operands) {
      std::cout << operand << " ";
    }
    std::cout << std::endl;
  }
}

void BytecodeCompiler::generateBytecode(std::basic_ostream<char>& out) {
  // this will generate JASM IR bytecode, let's just make an empty main for now
  out << "public class Main {\n";
  out << "    public static main([java/lang/String)V {\n";
  out << "        getstatic java/lang/System.out java/io/PrintStream\n";
  out << "        ldc \"Hello, World!\"\n";
  out << "        invokevirtual java/io/PrintStream.println(java/lang/String)V\n";
  out << "        return\n";
  out << "    }\n";
  out << "}\n";
}

std::vector<IRInstruction> BytecodeCompiler::getInstructions() const { return instructions; }