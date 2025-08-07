#ifndef SPECIAL_METHODS_LISTENER_H
#define SPECIAL_METHODS_LISTENER_H

#include "../errors/error_reporter.h"
#include "../symbols/symbol.h"
#include <cgullBaseListener.h>

class SpecialMethodsListener : public cgullBaseListener {
public:
  SpecialMethodsListener(ErrorReporter& errorReporter,
                         const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes);

private:
  ErrorReporter& errorReporter;
  const std::unordered_map<antlr4::ParserRuleContext*, std::shared_ptr<Scope>>& scopes;

  void enterStruct_definition(cgullParser::Struct_definitionContext* ctx) override;

  void validateToStringMethod(const std::shared_ptr<Scope>& structScope, const std::string& structName, int line,
                              int column);
  void validateNoUnsupportedSpecialMethods(const std::shared_ptr<Scope>& structScope, const std::string& structName,
                                           int line, int column);
  void addDefaultToStringMethod(const std::shared_ptr<Scope>& structScope, const std::string& structName);
};

#endif // SPECIAL_METHODS_LISTENER_H
