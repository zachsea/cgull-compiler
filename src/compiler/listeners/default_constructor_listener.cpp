#include "default_constructor_listener.h"

DefaultConstructorListener::DefaultConstructorListener(
    ErrorReporter& errorReporter, const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes)
    : errorReporter(errorReporter), scopes(scopes) {}

std::unordered_map<std::string, std::shared_ptr<FunctionSymbol>> DefaultConstructorListener::getConstructorMap() {
  return constructorMap;
}

void DefaultConstructorListener::enterStruct_definition(cgullParser::Struct_definitionContext* ctx) {
  auto structScope = scopes.at(ctx);
  auto structSymbol = std::dynamic_pointer_cast<TypeSymbol>(structScope->resolve(ctx->IDENTIFIER()->getText()));
  if (!structSymbol) {
    errorReporter.reportError(ErrorType::UNRESOLVED_REFERENCE, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(), "unresolved reference to struct");
    return;
  }

  auto constructorSymbol = std::make_shared<FunctionSymbol>(structSymbol->name, ctx->getStart()->getLine(),
                                                            ctx->getStart()->getCharPositionInLine(), structScope);

  constructorSymbol->isStructMethod = true;
  // get all the member fields of the struct
  std::vector<std::shared_ptr<VariableSymbol>> memberFields;
  for (const auto& member : structScope->symbols) {
    auto memberSymbol = std::dynamic_pointer_cast<VariableSymbol>(member.second);
    if (memberSymbol && !memberSymbol->isPrivate) {
      memberFields.push_back(memberSymbol);
    }
  }
  // sort the member fields by their line number
  std::sort(memberFields.begin(), memberFields.end(),
            [](const std::shared_ptr<Symbol>& a, const std::shared_ptr<Symbol>& b) {
              return a->definedAtLine < b->definedAtLine;
            });
  // add the member fields as parameters to the constructor
  for (const auto& field : memberFields) {
    auto paramSymbol =
        std::make_shared<VariableSymbol>(field->name, field->definedAtLine, field->definedAtColumn, structScope);
    paramSymbol->dataType = field->dataType;
    if (field->isDefined) {
      paramSymbol->hasDefaultValue = true;
    }
    constructorSymbol->parameters.push_back(paramSymbol);
  }
  constructorSymbol->returnTypes.push_back(structSymbol->typeRepresentation);
  constructorSymbol->isDefined = true;
  constructorSymbol->isPrivate = false;
  constructorSymbol->definedAtLine = ctx->getStart()->getLine();
  constructorSymbol->definedAtColumn = ctx->getStart()->getCharPositionInLine();
  constructorMap[structSymbol->name] = constructorSymbol;
  if (!structScope->parent->add(constructorSymbol)) {
    // user defined a function at the program level with the same name, not allowed
    auto conflictSymbol = structScope->parent->resolve(constructorSymbol->name);
    errorReporter.reportError(ErrorType::REDEFINITION, ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine(),
                              "redefinition of function '" + constructorSymbol->name + "' + " + conflictSymbol->name +
                                  " at line " + std::to_string(conflictSymbol->definedAtLine) + " column " +
                                  std::to_string(conflictSymbol->definedAtColumn));
  }
};
