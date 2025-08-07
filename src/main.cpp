#include <cgullLexer.h>
#include <cgullParser.h>
#include <cgullBaseListener.h>
#include <cgullListener.h>
#include <cgullBaseVisitor.h>
#include <cgullVisitor.h>
#include <cgullListener.h>
#include <antlr4-runtime.h>
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input-file>" << std::endl;
    return 1;
  }

  std::ifstream inputFile(argv[1]);
  if (!inputFile.is_open()) {
    std::cerr << "Failed to open input file: " << argv[1] << std::endl;
    return 1;
  }

  std::stringstream inputBuffer;
  inputBuffer << inputFile.rdbuf();
  inputFile.close();

  antlr4::ANTLRInputStream input(inputBuffer.str());

  cgullLexer lexer(&input);

  antlr4::CommonTokenStream tokens(&lexer);

  cgullParser parser(&tokens);

  antlr4::tree::ParseTree *tree = parser.program();

  if (parser.getNumberOfSyntaxErrors() > 0) {
    std::cerr << "Parsing failed with " << parser.getNumberOfSyntaxErrors()
              << " errors." << std::endl;
    return 1;
  }

  std::cout << "Parsing completed successfully!" << std::endl;
  std::cout << "Parse tree: \n" << tree->toStringTree(&parser, true) << std::endl;

  return 0;
}
