#include "bytecode_compiler.h"
#include "listeners/bytecode_ir_generator_listener.h"
#include <ostream>

BytecodeCompiler::BytecodeCompiler(
    cgullParser::ProgramContext* programCtx,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes)
    : programCtx(programCtx), scopeMap(scopeMap), expressionTypes(expressionTypes) {}

void BytecodeCompiler::compile() {
  // create a listener to generate the IR
  BytecodeIRGeneratorListener listener(errorReporter, scopeMap, expressionTypes);
  antlr4::tree::ParseTreeWalker walker;
  walker.walk(&listener, programCtx);

  // get the generated classes
  auto classes = listener.getClasses();
  for (const auto& irClass : classes) {
    std::cout << "Class: " << irClass->name << "\n";
    for (const auto& method : irClass->methods) {
      std::cout << "  Method: " << method->name << "\n";
      for (const auto& instruction : method->instructions) {
        std::cout << "    Instruction: " << instruction->toString() << "\n";
      }
    }
  }
  generatedClasses = classes;
}

void BytecodeCompiler::generateBytecode(std::basic_ostream<char>& out) {
  for (const auto& irClass : generatedClasses) {
    generateClass(out, irClass);
  }
}

// right now does not support user types
std::string BytecodeCompiler::typeToJVMType(const std::shared_ptr<Type>& type) {
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
  if (primitiveType) {
    switch (primitiveType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::INT:
      return "I";
    case PrimitiveType::PrimitiveKind::SHORT:
      return "S";
    case PrimitiveType::PrimitiveKind::LONG:
      return "J";
    case PrimitiveType::PrimitiveKind::FLOAT:
      return "F";
    case PrimitiveType::PrimitiveKind::CHAR:
      return "C";
    case PrimitiveType::PrimitiveKind::STRING:
      return "java/lang/String";
    case PrimitiveType::PrimitiveKind::VOID:
      return "V";
    default:
      throw std::runtime_error("Unsupported right now");
    }
  }
  return type->toString();
}

void BytecodeCompiler::generateClass(std::basic_ostream<char>& out, const std::shared_ptr<IRClass>& irClass) {
  out << "public class " << irClass->name << " {\n";
  for (const auto& method : irClass->methods) {
    // special case for main

    // function name
    if (method->name == "main") {
      out << "public static main(";
    } else {
      out << "public static " << method->getMangledName() << "(";
    }
    // function parameters
    if (method->name == "main") {
      out << "[java/lang/String";
    } else {
      for (const auto& parameter : method->parameters) {
        out << typeToJVMType(parameter->dataType) << " " << parameter->name << ", ";
      }
    }
    out << ")";
    // function return type
    if (method->returnTypes.size() > 0) {
      out << typeToJVMType(method->returnTypes[0]);
    }
    out << "{\n";

    for (const auto& instruction : method->instructions) {
      std::cout << "Generating instruction: " << instruction->toString() << "\n";
      generateInstruction(out, instruction);
    }
    // implicit return for main, if not already specified
    if (method->name == "main") {
      out << "return\n";
    }
    out << "}\n";
  }
  out << "}\n";
}

void BytecodeCompiler::generateInstruction(std::basic_ostream<char>& out,
                                           const std::shared_ptr<IRInstruction>& instruction) {
  auto callInstruction = std::dynamic_pointer_cast<IRCallInstruction>(instruction);
  if (callInstruction) {
    generateCallInstruction(out, callInstruction);
    return;
  }
  auto rawInstruction = std::dynamic_pointer_cast<IRRawInstruction>(instruction);
  if (rawInstruction) {
    out << rawInstruction->instruction << "\n";
    return;
  }
}

void BytecodeCompiler::generateCallInstruction(std::basic_ostream<char>& out,
                                               const std::shared_ptr<IRCallInstruction>& instruction) {
  if (instruction->function->name == "println") {
    // getstatic already added in enterFunction_call
    out << "ldc \"Hello, World\"\n";
    out << "invokevirtual java/io/PrintStream.println(java/lang/String)V\n";
    return;
  }
  out << "invokevirtual " << instruction->function->getMangledName() << "(";
  for (const auto& parameter : instruction->function->parameters) {
    out << typeToJVMType(parameter->dataType);
  }
  out << ")";
  // return type
  if (instruction->function->returnTypes.size() > 0) {
    out << typeToJVMType(instruction->function->returnTypes[0]);
  }
  out << "V\n";
}
