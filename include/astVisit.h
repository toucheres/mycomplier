#pragma once
#include "CLexer.h"
#include "CParser.h"
#include "CParserBaseVisitor.h"
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
    long long parseConstexpr(CParser::AssignmentExpressionContext* expr);
    long long parseConstexpr(CParser::ConditionalExpressionContext* expr);
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
    std::vector<IDdef> lowerDeclaration(CParser::DeclarationSpecifiersContext* specs,
                                        CParser::InitDeclaratorListContext* initList);
    std::tuple<IDdef*, std::string> madeConstString(std::vector<antlr4::tree::TerminalNode*> toks);
    std::optional<std::vector<int>> decodeStringLiteral(CParser::AssignmentExpressionContext* expr);
    std::optional<Type> tryVisitType(std::function<Type()> expr);
    // varDef* madeConstString(std::string origin);

  public:
    // std::vector<Type> addDeclarations(std::vector<Type> vars);
    astVisitor(std::string name, OBJ& obj);
    OBJ& obj;
    void visitAsmADDer(CParser::AsmADDerContext* ctx);
    // void visitByTypeIndex(antlr4::ParserRuleContext* ctx);
    void visitCompilationUnit(CParser::CompilationUnitContext* ctx);
    void visitTranslationUnit(CParser::TranslationUnitContext* ctx);
    std::vector<IDdef> visitDeclaration(CParser::DeclarationContext* ctx);
    void visitFunctionDefinition(CParser::FunctionDefinitionContext* ctx);
    void visitExternalDeclaration(CParser::ExternalDeclarationContext* ctx);
    std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>,
               std::vector<Type::TypeQualifier>>
    visitDeclarationSpecifiers(CParser::DeclarationSpecifiersContext* ctx);
    // Type visitDeclarationSpecifier(CParser::DeclarationSpecifierContext* ctx);
    std::variant<Type, Type::StorageClassSpecifier, Type::TypeQualifier> visitDeclarationSpecifier(
        CParser::DeclarationSpecifierContext* ctx);
    Type visitTypeSpecifier(CParser::TypeSpecifierContext* ctx);
    std::vector<IDdef> visitInitDeclaratorList(CParser::InitDeclaratorListContext* ctx,
                                               Type basetype,
                                               StorageClassSpecifier storageClassSpecifier);
    // Type visitInitDeclarator(CParser::InitDeclaratorContext* ctx);
    IDdef visitInitDeclarator(CParser::InitDeclaratorContext* ctx, Type basetype,
                              StorageClassSpecifier storageClassSpecifier);
    IDdef visitDeclarator(CParser::DeclaratorContext* ctx);
    IDdef visitDirectDeclarator(CParser::DirectDeclaratorContext* ctx);
    std::vector<IDdef> visitParameterList(CParser::ParameterListContext* ctx);
    IDdef visitParameterDeclaration(CParser::ParameterDeclarationContext* ctx);
    void visitBlockItemList(CParser::BlockItemListContext* ctx);
    void visitStatement(CParser::StatementContext* ctx);
    void visitExpressionStatement(CParser::ExpressionStatementContext* ctx);
    Type visitExpression(CParser::ExpressionContext* ctx);
    Type visitAssignmentExpression(CParser::AssignmentExpressionContext* ctx);
    Type visitConditionalExpression(CParser::ConditionalExpressionContext* ctx);
    Type visitLogicalOrExpression(CParser::LogicalOrExpressionContext* ctx);
    Type visitLogicalAndExpression(CParser::LogicalAndExpressionContext* ctx);
    Type visitInclusiveOrExpression(CParser::InclusiveOrExpressionContext* ctx);
    Type visitExclusiveOrExpression(CParser::ExclusiveOrExpressionContext* ctx);
    Type visitAndExpression(CParser::AndExpressionContext* ctx);
    Type visitEqualityExpression(CParser::EqualityExpressionContext* ctx);
    Type visitRelationalExpression(CParser::RelationalExpressionContext* ctx);
    Type visitShiftExpression(CParser::ShiftExpressionContext* ctx);
    Type visitAdditiveExpression(CParser::AdditiveExpressionContext* ctx);
    Type visitMultiplicativeExpression(CParser::MultiplicativeExpressionContext* ctx);
    Type visitCastExpression(CParser::CastExpressionContext* ctx);
    Type visitUnaryExpression(CParser::UnaryExpressionContext* ctx);
    Type visitPostfixExpression(CParser::PostfixExpressionContext* ctx);
    Type visitPrimaryExpression(CParser::PrimaryExpressionContext* ctx);
    Type visitAbstractDeclarator(CParser::AbstractDeclaratorContext* ctx);
    Type visitDirectAbstractDeclarator(CParser::DirectAbstractDeclaratorContext* ctx);
    Type visitTypeName(CParser::TypeNameContext* ctx);
    Type visitTypeofSpecifier(CParser::TypeofSpecifierContext* ctx);
    void visitBlockItem(CParser::BlockItemContext* ctx);
    std::vector<IDdef> visitParameterTypeList(CParser::ParameterTypeListContext* ctx);
    void visitCompoundStatement(CParser::CompoundStatementContext* ctx);
    std::tuple<std::vector<Type::TypeQualifier>, std::optional<Type>> visitSpecifierQualifierList(
        CParser::SpecifierQualifierListContext* ctx,
        std::vector<Type::TypeQualifier> typeQualifiers = std::vector<Type::TypeQualifier>{});
    void visitSelectionStatement(CParser::SelectionStatementContext* ctx);
    size_t visitArgumentExpressionList(CParser::ArgumentExpressionListContext* ctx);
    void visitIterationStatement(CParser::IterationStatementContext* ctx);
    void visitJumpStatement(CParser::JumpStatementContext* ctx);
    std::vector<std::pair<std::string, IDdef>> visitMemberDeclarationList(
        CParser::MemberDeclarationListContext* ctx);
};