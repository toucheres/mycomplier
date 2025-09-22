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
    funcDef* funcnow = nullptr;
    // funcDef* globalinitfun = nullptr;
    // "__global_init" + name
    // 辅助函数
    Type baseType; // just for args, wait to modifiy
    long long parseConstexpr(ComplierParser::AssignmentExpressionContext* expr);
    int parseIntegerConstant(const std::string& text);
    int parseCharacterConstant(const std::string& text);
    bool isIntegerConstant(const std::string& text);
    bool isCharacterConstant(const std::string& text);

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
    std::any visitParameterList(ComplierParser::ParameterListContext* ctx) override;
    std::any visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx) override;
    std::any visitBlockItemList(ComplierParser::BlockItemListContext* ctx) override;
    std::any visitStatement(ComplierParser::StatementContext* ctx) override;
    std::any visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx) override;
    std::any visitExpression(ComplierParser::ExpressionContext* ctx) override;
    std::any visitAssignmentExpression(ComplierParser::AssignmentExpressionContext* ctx) override;
    std::any visitConditionalExpression(ComplierParser::ConditionalExpressionContext* ctx) override;
    std::any visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext* ctx) override;
    std::any visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext* ctx) override;
    std::any visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext* ctx) override;
    std::any visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext* ctx) override;
    std::any visitAndExpression(ComplierParser::AndExpressionContext* ctx) override;
    std::any visitEqualityExpression(ComplierParser::EqualityExpressionContext* ctx) override;
    std::any visitRelationalExpression(ComplierParser::RelationalExpressionContext* ctx) override;
    std::any visitShiftExpression(ComplierParser::ShiftExpressionContext* ctx) override;
    std::any visitAdditiveExpression(ComplierParser::AdditiveExpressionContext* ctx) override;
    std::any visitMultiplicativeExpression(
        ComplierParser::MultiplicativeExpressionContext* ctx) override;
    std::any visitCastExpression(ComplierParser::CastExpressionContext* ctx) override;
    std::any visitUnaryExpression(ComplierParser::UnaryExpressionContext* ctx) override;
    std::any visitPostfixExpression(ComplierParser::PostfixExpressionContext* ctx) override;
    std::any visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx) override;
    std::any visitDeclarationSpecifiers2(
        ComplierParser::DeclarationSpecifiers2Context* ctx) override;
    std::any visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx) override;
    std::any visitDirectAbstractDeclarator(
        ComplierParser::DirectAbstractDeclaratorContext* ctx) override;
    std::any visitTypeName(ComplierParser::TypeNameContext* ctx) override;
    std::any visitBlockItem(ComplierParser::BlockItemContext* ctx) override;
    std::any visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx) override;
    std::any visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx) override;
    std::any visitSpecifierQualifierList(
        ComplierParser::SpecifierQualifierListContext* ctx) override;
    std::any visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx) override;
    std::any visitArgumentExpressionList(
        ComplierParser::ArgumentExpressionListContext* ctx) override;
    std::any visitIterationStatement(ComplierParser::IterationStatementContext* ctx) override;
    std::any visitJumpStatement(ComplierParser::JumpStatementContext* ctx) override;
};