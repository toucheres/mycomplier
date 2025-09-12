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
    astVisitor(std::string name, OBJ& obj);
    OBJ& obj;
    std::any visitByTypeIndex(antlr4::ParserRuleContext* ctx);
    std::any visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx) override;
    std::any visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx) override;
    std::any visitDeclaration(ComplierParser::DeclarationContext* ctx) override;
    std::any visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx) override;
    std::any visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx) override;
    std::any visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx) override;
    std::any visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx) override;
    std::any visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx) override;
    std::any visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx) override;
    std::any visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx) override;
    std::any visitDeclarator(ComplierParser::DeclaratorContext* ctx) override;
    std::any visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx) override;
    std::any visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx) override;
    std::any visitParameterList(ComplierParser::ParameterListContext* ctx) override;
    std::any visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx) override;
    std::any visitDeclarationSpecifiers2(
        ComplierParser::DeclarationSpecifiers2Context* ctx) override;
    std::any visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx) override;
    std::any visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext* ctx) override;
    std::any visitTypeName(ComplierParser::TypeNameContext* ctx) override;
    std::any visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx) override;
};