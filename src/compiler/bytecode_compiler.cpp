#include "bytecode_compiler.h"
#include "listeners/bytecode_ir_generator_listener.h"
#include "primitive_wrapper_generator.h"
#include <ostream>

BytecodeCompiler::BytecodeCompiler(
    cgullParser::ProgramContext* programCtx,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>> scopeMap,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Type>> expressionTypes,
    std::unordered_set<antlr4::ParserRuleContext*> expectingStringConversion,
    std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>> constructorMap,
    std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<FunctionSymbol>> resolvedMethodSymbols)
    : programCtx(programCtx), scopeMap(scopeMap), expressionTypes(expressionTypes),
      expectingStringConversion(expectingStringConversion), constructorMap(constructorMap),
      resolvedMethodSymbols(resolvedMethodSymbols) {}

void BytecodeCompiler::compile() {
  // generate wrappers for primitive types as needed
  for (const auto& [ctx, type] : expressionTypes) {
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
    if (primitiveType) {
      if (primitiveType && primitiveType->getPrimitiveKind() != PrimitiveType::PrimitiveKind::VOID) {
        getOrCreatePrimitiveWrapper(primitiveType->getPrimitiveKind());
      }
    }
  }
  for (const auto& [kind, wrapper] : primitiveWrappers) {
    generatedClasses.push_back(wrapper);
  }

  // create a listener to generate the IR
  BytecodeIRGeneratorListener listener(errorReporter, scopeMap, expressionTypes, resolvedMethodSymbols,
                                       expectingStringConversion, primitiveWrappers, constructorMap);
  antlr4::tree::ParseTreeWalker walker;
  walker.walk(&listener, programCtx);

  // user defined classes
  for (const auto& irClass : listener.getClasses()) {
    generatedClasses.push_back(irClass);
  }
}

void BytecodeCompiler::generateBytecode(const std::string& outputDir) {
  // remove existing .jasm and .class files
  try {
    std::filesystem::remove_all(outputDir);
  } catch (const std::exception& e) {
  }
  // create output directory if it doesn't exist
  try {
    std::filesystem::create_directories(outputDir);
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to create output directory: " + outputDir + " (" + e.what() + ")");
  }

  for (const auto& irClass : generatedClasses) {
    std::string filePath = outputDir + "/" + irClass->name + ".jasm";
    std::ofstream outFile(filePath);

    if (!outFile.is_open()) {
      throw std::runtime_error("Failed to open output file: " + filePath);
    }

    generateClass(outFile, irClass);
    outFile.close();

    std::cout << "Generated class file: " << filePath << std::endl;
  }
}

// right now does not support user types
std::string BytecodeCompiler::typeToJVMType(const std::shared_ptr<Type>& type) {
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(type);
  if (primitiveType) {
    switch (primitiveType->getPrimitiveKind()) {
    case PrimitiveType::PrimitiveKind::INT:
      return "I";
    case PrimitiveType::PrimitiveKind::FLOAT:
      return "F";
    case PrimitiveType::PrimitiveKind::STRING:
      return "java/lang/String";
    case PrimitiveType::PrimitiveKind::BOOLEAN:
      return "Z";
    case PrimitiveType::PrimitiveKind::VOID:
      return "V";
    default:
      throw std::runtime_error("Unsupported right now");
    }
  }
  auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
  if (pointerType) {
    return pointerType->toString();
  }
  auto arrayType = std::dynamic_pointer_cast<ArrayType>(type);
  if (arrayType) {
    return "[" + typeToJVMType(arrayType->getElementType());
  }
  return type->toString();
}

void BytecodeCompiler::generateClass(std::basic_ostream<char>& out, const std::shared_ptr<IRClass>& irClass) {
  out << "public class " << irClass->name << " {\n";

  if (irClass->name.find("Reference") != std::string::npos &&
      (irClass->name == "IntReference" || irClass->name == "FloatReference" || irClass->name == "BoolReference" ||
       irClass->name == "StringReference")) {
    std::string fieldType;
    if (irClass->name == "IntReference")
      fieldType = "I";
    else if (irClass->name == "FloatReference")
      fieldType = "F";
    else if (irClass->name == "BoolReference")
      fieldType = "Z";
    else if (irClass->name == "StringReference")
      fieldType = "java/lang/String";

    out << "private value " << fieldType << "\n";
  }

  // fields
  for (const auto& variable : irClass->variables) {
    out << (variable->isPrivate ? "private " : "public ") << variable->name << " " << typeToJVMType(variable->dataType)
        << "\n";
  }

  for (const auto& method : irClass->methods) {
    // special case for main
    if (method->name == "main") {
      out << "public static main(";
    } else if (method->name == "<init>") {
      out << "public <init>(";
    } else {
      // static just for the main method, really
      out << "public " << (method->isStructMethod ? "" : "static ") << method->getMangledName() << "(";
    }
    // function parameters
    if (method->name == "main") {
      out << "[java/lang/String";
    } else {
      for (size_t i = 0; i < method->parameters.size(); ++i) {
        const auto& parameter = method->parameters[i];
        if (i > 0)
          out << ", ";
        out << typeToJVMType(parameter->dataType);
      }
    }
    out << ")";
    // function return type
    if (method->returnTypes.size() > 0 && method->name != "<init>") {
      out << typeToJVMType(method->returnTypes[0]);
    } else if (method->name == "<init>") {
      out << "V";
    }
    out << "{\n";

    for (const auto& instruction : method->instructions) {
      generateInstruction(out, instruction);
    }
    // implicit return for void functions, doesn't hurt to be redundant
    auto voidType = std::make_shared<PrimitiveType>(PrimitiveType::PrimitiveKind::VOID);
    if (method->returnTypes[0]->equals(voidType)) {
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

// to be continued in HW5
void BytecodeCompiler::generateCallInstruction(std::basic_ostream<char>& out,
                                               const std::shared_ptr<IRCallInstruction>& instruction) {
  if (instruction->function->name == "print" || instruction->function->name == "println") {
    // getstatic already added in enterFunction_call
    auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(instruction->function->parameters[0]->dataType);
    if (primitiveType->getPrimitiveKind() == PrimitiveType::PrimitiveKind::STRING) {
      out << "invokevirtual java/io/PrintStream." << instruction->function->name << "(java/lang/String)V\n";
    } else {
      // we need to call its toString first, or for primitives we can piggyback still
    }
    return;
  } else if (instruction->function->name == "readline" || instruction->function->name == "read") {
    // this doesnt take any arguments, so we can just put all the related instructions here
    out << "new java/util/Scanner\n";
    out << "dup\n";
    out << "getstatic java/lang/System.in java/io/InputStream\n";
    out << "invokespecial java/util/Scanner.<init>(java/io/InputStream)V\n";
    if (instruction->function->name == "readline") {
      out << "invokevirtual java/util/Scanner.nextLine()java/lang/String\n";
    } else {
      out << "invokevirtual java/util/Scanner.next()java/lang/String\n";
    }
    return;
  }

  if (instruction->function->name == "<init>") {
    // don't use mangled name for constructors
    out << "invokespecial " << instruction->function->returnTypes[0]->toString() << "." << instruction->function->name
        << "(";
  } else {
    // get the "this" from the function's scope
    auto thisVar = std::dynamic_pointer_cast<VariableSymbol>(instruction->function->scope->resolve("this"));
    if (thisVar) {
      out << "invokevirtual " << thisVar->dataType->toString() << "." << instruction->function->getMangledName() << "(";
    } else {
      out << "invokestatic Main." << instruction->function->getMangledName() << "(";
    }
  }

  for (int i = 0; i < instruction->function->parameters.size(); i++) {
    if (i > 0) {
      out << ", ";
    }
    out << typeToJVMType(instruction->function->parameters[i]->dataType);
  }
  out << ")";
  // return type
  if (instruction->function->returnTypes.size() > 0 && instruction->function->name != "<init>") {
    out << typeToJVMType(instruction->function->returnTypes[0]);
  } else {
    out << "V";
  }
  out << "\n";
}

std::shared_ptr<IRClass> BytecodeCompiler::getOrCreatePrimitiveWrapper(PrimitiveType::PrimitiveKind kind) {
  auto it = primitiveWrappers.find(kind);
  if (it != primitiveWrappers.end()) {
    return it->second;
  }

  auto wrapper = PrimitiveWrapperGenerator::generateWrapperClass(kind);
  primitiveWrappers[kind] = wrapper;
  return wrapper;
}

bool BytecodeCompiler::needsPrimitiveWrapper(const std::shared_ptr<Type>& type) {
  auto pointerType = std::dynamic_pointer_cast<PointerType>(type);
  if (!pointerType) {
    return false;
  }

  auto pointeeType = pointerType->getPointedType();
  auto primitiveType = std::dynamic_pointer_cast<PrimitiveType>(pointeeType);

  // needs wrapper if it's a pointer to a primitive (except void)
  return primitiveType && primitiveType->getPrimitiveKind() != PrimitiveType::PrimitiveKind::VOID;
}
