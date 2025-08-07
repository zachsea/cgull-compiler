#include "compiler/errors/error_reporter.h"
#include "compiler/listeners/collecting_error_listener.h"
#include "compiler/semantic_analyzer.h"
#include <antlr4-runtime.h>
#include <cgullBaseListener.h>
#include <cgullBaseVisitor.h>
#include <cgullLexer.h>
#include <cgullListener.h>
#include <cgullParser.h>
#include <cgullVisitor.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

enum StopStage {
  NONE,
  LEXING,
  PARSING,
};

void printTokens(const cgullLexer& lexer, antlr4::CommonTokenStream& tokens) {
  for (size_t i = 0; i < tokens.size(); ++i) {
    auto token = tokens.get(i);
    std::string tokenName = std::string(lexer.getVocabulary().getSymbolicName(token->getType()));
    std::cout << "Token: " << tokenName << ", Text: '" << token->getText() << "'"
              << ", Start: " << token->getStartIndex() << ", End: " << token->getStopIndex()
              << ", Line: " << token->getLine() << std::endl;
  }
}

void printErrors(const std::string& label, const CollectingErrorListener& listener) {
  if (!listener.errors.empty()) {
    std::cerr << "\n" << label << " errors:\n";
    for (const auto& err : listener.errors) {
      std::cerr << err << std::endl;
    }
    std::cerr << label << " failed with " << listener.errors.size() << " errors." << std::endl;
  }
}

bool hasAnyErrors(const CollectingErrorListener& lexerListener, const CollectingErrorListener& parserListener) {
  return !lexerListener.errors.empty() || !parserListener.errors.empty();
}

int main(int argc, char* argv[]) {
  StopStage stopStage = NONE;
  // TODO: better argument parsing
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input-file> [--lexer | --parser]" << std::endl;
    return 1;
  }
  if (argc == 3) {
    stopStage = ((std::string)argv[2] == "--lexer") ? LEXING : ((std::string)argv[2] == "--parser") ? PARSING : NONE;
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

  CollectingErrorListener lexerErrorListener;
  lexer.removeErrorListeners();
  lexer.addErrorListener(&lexerErrorListener);

  antlr4::CommonTokenStream tokens(&lexer);
  tokens.fill();

  if (stopStage == LEXING) {
    printTokens(lexer, tokens);
    printErrors("Lexer", lexerErrorListener);
    if (!lexerErrorListener.errors.empty()) {
      return 1;
    }
    std::cout << "Lexing completed successfully!" << std::endl;
    return 0;
  }

  cgullParser parser(&tokens);

  CollectingErrorListener parserErrorListener;
  parser.removeErrorListeners();
  parser.addErrorListener(&parserErrorListener);

  cgullParser::ProgramContext* tree = parser.program();

  if (stopStage == PARSING) {
    std::cout << "Parse tree: \n" << tree->toStringTree(&parser, true) << std::endl;
    printErrors("Lexer", lexerErrorListener);
    printErrors("Parser", parserErrorListener);
    if (hasAnyErrors(lexerErrorListener, parserErrorListener)) {
      std::cerr << "Lexing and/or parsing failed with errors." << std::endl;
      return 1;
    }
    std::cout << "Parsing completed successfully!" << std::endl;
    return 0;
  }

  printErrors("Lexer", lexerErrorListener);
  printErrors("Parser", parserErrorListener);

  if (hasAnyErrors(lexerErrorListener, parserErrorListener)) {
    std::cerr << "Lexing and/or parsing failed with errors. Semantic analysis will not be performed." << std::endl;
    return 1;
  }

  SemanticAnalyzer semanticAnalyzer;
  semanticAnalyzer.analyze(tree);
  semanticAnalyzer.printSymbolsAsJson(std::cout);

  if (semanticAnalyzer.getErrorReporter().hasErrors()) {
    std::cerr << "Semantic analysis failed with errors." << std::endl;
    semanticAnalyzer.getErrorReporter().displayErrors();
    return 1;
  }
}
