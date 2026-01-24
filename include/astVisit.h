#pragma once
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "error.hpp"
#include "obj.h"
#include <antlr4-runtime/antlr4-runtime.h>
#include <tree/ParseTree.h>
struct astVisitor
{
  private:
    funcDef* funcnow = nullptr;
    funcDef* gfuncptr = nullptr;
    // funcDef* globalinitfun = nullptr;
    // "__global_init" + name
    // 辅助函数
    // Type baseType; // just for args, wait to modifiy
    long long parseConstexpr(ComplierParser::AssignmentExpressionContext* expr);
    int parseIntegerConstant(const std::string& text);
    int parseCharacterConstant(const std::string& text);
    bool isIntegerConstant(const std::string& text);
    bool isCharacterConstant(const std::string& text);
    std::vector<Type> lowerDeclaration(ComplierParser::DeclarationSpecifiersContext* specs,
                                       ComplierParser::InitDeclaratorListContext* initList);

  public:
    std::vector<Type> addDeclarations(std::vector<Type> vars);
    astVisitor(std::string name, OBJ& obj);
    OBJ& obj;
    void visitAsmADDer(ComplierParser::AsmADDerContext* ctx);
    // void visitByTypeIndex(antlr4::ParserRuleContext* ctx);
    void visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx);
    void visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx);
    std::vector<Type> visitDeclaration(ComplierParser::DeclarationContext* ctx);
    void visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx);
    void visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx);
    std::vector<Type> visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx);
    Type visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx);
    Type visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx);
    // std::vector<Type> visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx);
    std::vector<Type> visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx,
                                              Type basetype,
                                              Type::StorageClassSpecifier storageClassSpecifier);
    // Type visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx);
    Type visitInitDeclarator(
        ComplierParser::InitDeclaratorContext* ctx, Type basetype,
        Type::StorageClassSpecifier storageClassSpecifier);
    Type visitInitDeclarator_(ComplierParser::InitDeclaratorContext* ctx);
    Type visitDeclarator(ComplierParser::DeclaratorContext* ctx);
    Type visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx);
    std::vector<Type> visitParameterList(ComplierParser::ParameterListContext* ctx);
    Type visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx);
    void visitBlockItemList(ComplierParser::BlockItemListContext* ctx);
    void visitStatement(ComplierParser::StatementContext* ctx);
    void visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx);
    Type visitExpression(ComplierParser::ExpressionContext* ctx);
    Type visitAssignmentExpression(ComplierParser::AssignmentExpressionContext* ctx);
    Type visitConditionalExpression(ComplierParser::ConditionalExpressionContext* ctx);
    Type visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext* ctx);
    Type visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext* ctx);
    Type visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext* ctx);
    Type visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext* ctx);
    Type visitAndExpression(ComplierParser::AndExpressionContext* ctx);
    Type visitEqualityExpression(ComplierParser::EqualityExpressionContext* ctx);
    Type visitRelationalExpression(ComplierParser::RelationalExpressionContext* ctx);
    Type visitShiftExpression(ComplierParser::ShiftExpressionContext* ctx);
    Type visitAdditiveExpression(ComplierParser::AdditiveExpressionContext* ctx);
    Type visitMultiplicativeExpression(ComplierParser::MultiplicativeExpressionContext* ctx);
    Type visitCastExpression(ComplierParser::CastExpressionContext* ctx);
    Type visitUnaryExpression(ComplierParser::UnaryExpressionContext* ctx);
    Type visitPostfixExpression(ComplierParser::PostfixExpressionContext* ctx);
    Type visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx);
    std::vector<Type> visitDeclarationSpecifiers2(
        ComplierParser::DeclarationSpecifiers2Context* ctx);
    Type visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx);
    Type visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext* ctx);
    Type visitTypeName(ComplierParser::TypeNameContext* ctx);
    void visitBlockItem(ComplierParser::BlockItemContext* ctx);
    std::vector<Type> visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx);
    void visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx);
    void visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx);
    void visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx);
    void visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext* ctx);
    void visitIterationStatement(ComplierParser::IterationStatementContext* ctx);
    void visitJumpStatement(ComplierParser::JumpStatementContext* ctx);
};