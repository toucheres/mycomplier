#pragma once
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "error.hpp"
#include "obj.h"
#include <antlr4-runtime/antlr4-runtime.h>
#include <tree/ParseTree.h>
struct astVisitor : public ComplierBaseVisitor
{
  private:
    funcDef* funcnow;
    funcDef* globalinitfun;
    
    // 辅助函数：递归收集多维数组的维度信息
    std::vector<int> collectArrayDimensions(ComplierParser::DirectDeclaratorContext* ddCtx);

  public:
    astVisitor(std::string name);
    OBJ obj;
    std::any visitByTypeIndex(antlr4::ParserRuleContext* ctx);
    std::any visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx) override;
    std::any visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx) override;
    std::any visitDeclaration(ComplierParser::DeclarationContext* ctx) override;
    std::any visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx) override;
    std::any visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx) override;
    std::any visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx) override;
    std::any visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx) override;
    std::any visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx) override;
};