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
    // size_t alignas_num = 8;
    IDdef* funcnow = nullptr;
    IDdef* gfuncptr = nullptr;
    IDdef* record_ID_decl(const IDdef& iddef, IDdef* func_ctx = nullptr,
                          std::optional<std::size_t> arg_index = std::nullopt);
    IDdef* record_ID_decl(const std::string& name, const Type& type,
                          StorageClassSpecifier storageClassSpecifier, IDdef* func_ctx = nullptr,
                          std::optional<std::size_t> arg_index = std::nullopt);
    IDdef* lookup_ID_decl(const std::string& name, StorageClassSpecifier storageClassSpecifier =
                                                       StorageClassSpecifier::VarDef);
    std::string global_label(const IDdef& v) const;
    // funcDef* globalinitfun = nullptr;
    // "__global_init" + name
    // 辅助函数
    // Type baseType; // just for args, wait to modifiy
    long long parseConstexpr(ComplierParser::AssignmentExpressionContext* expr);
    int parseIntegerConstant(const std::string& text);
    int parseCharacterConstant(const std::string& text);
    bool isIntegerConstant(const std::string& text);
    bool isCharacterConstant(const std::string& text);
    bool stackTopIsLvalue();
    bool madeTopIsLvalueAddr();
    Type load_var_or_func(std::string name);
    Type loadStackTopAddrByType(Type size);
    void saveStackTopAddrValueByType(Type type);
    void loadStackTopAddrBySize(size_t size);
    void SaveStackTopValueToAddr(size_t size);
    std::vector<IDdef> lowerDeclaration(ComplierParser::DeclarationSpecifiersContext* specs,
                                        ComplierParser::InitDeclaratorListContext* initList);
    std::tuple<IDdef*, std::string> madeConstString(std::vector<antlr4::tree::TerminalNode*> toks);
    std::optional<std::vector<int>> decodeStringLiteral(
        ComplierParser::AssignmentExpressionContext* expr);
    std::optional<Type> tryVisitType(std::function<Type()> expr);
    // varDef* madeConstString(std::string origin);

  public:
    // std::vector<Type> addDeclarations(std::vector<Type> vars);
    astVisitor(std::string name, OBJ& obj);
    OBJ& obj;
    void visitAsmADDer(ComplierParser::AsmADDerContext* ctx);
    // void visitByTypeIndex(antlr4::ParserRuleContext* ctx);
    void visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx);
    void visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx);
    std::vector<IDdef> visitDeclaration(ComplierParser::DeclarationContext* ctx);
    void visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx);
    void visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx);
    std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>>
    visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx);
    Type visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx);
    Type visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx);
    std::vector<IDdef> visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx,
                                               Type basetype,
                                               StorageClassSpecifier storageClassSpecifier);
    // Type visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx);
    IDdef visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx, Type basetype,
                              StorageClassSpecifier storageClassSpecifier);
    IDdef visitDeclarator(ComplierParser::DeclaratorContext* ctx);
    IDdef visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx);
    std::vector<IDdef> visitParameterList(ComplierParser::ParameterListContext* ctx);
    IDdef visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx);
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
    std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>>
    visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context* ctx);
    Type visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx);
    Type visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext* ctx);
    Type visitTypeName(ComplierParser::TypeNameContext* ctx);
    void visitBlockItem(ComplierParser::BlockItemContext* ctx);
    std::vector<IDdef> visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx);
    void visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx);
    std::tuple<std::vector<Type::TypeQualifier>, std::optional<Type>> visitSpecifierQualifierList(
        ComplierParser::SpecifierQualifierListContext* ctx,
        std::vector<Type::TypeQualifier> typeQualifiers = std::vector<Type::TypeQualifier>{});
    void visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx);
    size_t visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext* ctx);
    void visitIterationStatement(ComplierParser::IterationStatementContext* ctx);
    void visitJumpStatement(ComplierParser::JumpStatementContext* ctx);
    std::vector<std::pair<std::string, IDdef>> visitStructDeclarationList(
        ComplierParser::StructDeclarationListContext* ctx);
};