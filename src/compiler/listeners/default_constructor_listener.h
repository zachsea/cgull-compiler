#ifndef DEFAULT_CONSTRUCTOR_LISTENER_H
#define DEFAULT_CONSTRUCTOR_LISTENER_H

#include "../errors/error_reporter.h"
#include "../symbols/symbol.h"
#include <cgullBaseListener.h>

class DefaultConstructorListener : public cgullBaseListener {
public:
  DefaultConstructorListener(ErrorReporter& errorReporter,
                             const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes);

private:
  ErrorReporter& errorReporter;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;

  void enterStruct_definition(cgullParser::Struct_definitionContext* ctx) override;
};

#endif // DEFAULT_CONSTRUCTOR_LISTENER_H
