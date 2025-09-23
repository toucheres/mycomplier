
// Generated from /home/toucher/vscoderope/mycomplier/grammar/Complier.g4 by ANTLR 4.13.1

#pragma once


#include "antlr4-runtime.h"
#include "ComplierVisitor.h"


/**
 * This class provides an empty implementation of ComplierVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  ComplierBaseVisitor : public ComplierVisitor {
public:

  virtual std::any visitPrimaryExpression(ComplierParser::PrimaryExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericSelection(ComplierParser::GenericSelectionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericAssocList(ComplierParser::GenericAssocListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericAssociation(ComplierParser::GenericAssociationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfixExpression(ComplierParser::PostfixExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryExpression(ComplierParser::UnaryExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryOperator(ComplierParser::UnaryOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpression(ComplierParser::CastExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMultiplicativeExpression(ComplierParser::MultiplicativeExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAdditiveExpression(ComplierParser::AdditiveExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShiftExpression(ComplierParser::ShiftExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRelationalExpression(ComplierParser::RelationalExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEqualityExpression(ComplierParser::EqualityExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAndExpression(ComplierParser::AndExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConditionalExpression(ComplierParser::ConditionalExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentExpression(ComplierParser::AssignmentExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentOperator(ComplierParser::AssignmentOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpression(ComplierParser::ExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConstantExpression(ComplierParser::ConstantExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclaration(ComplierParser::DeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitDeclarator(ComplierParser::InitDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStorageClassSpecifier(ComplierParser::StorageClassSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpecifier(ComplierParser::TypeSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructOrUnionSpecifier(ComplierParser::StructOrUnionSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructOrUnion(ComplierParser::StructOrUnionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDeclarationList(ComplierParser::StructDeclarationListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDeclaration(ComplierParser::StructDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDeclaratorList(ComplierParser::StructDeclaratorListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDeclarator(ComplierParser::StructDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumSpecifier(ComplierParser::EnumSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumeratorList(ComplierParser::EnumeratorListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumerator(ComplierParser::EnumeratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumerationConstant(ComplierParser::EnumerationConstantContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAtomicTypeSpecifier(ComplierParser::AtomicTypeSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeQualifier(ComplierParser::TypeQualifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionSpecifier(ComplierParser::FunctionSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAlignmentSpecifier(ComplierParser::AlignmentSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclarator(ComplierParser::DeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDirectDeclarator(ComplierParser::DirectDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVcSpecificModifer(ComplierParser::VcSpecificModiferContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGccDeclaratorExtension(ComplierParser::GccDeclaratorExtensionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGccAttributeSpecifier(ComplierParser::GccAttributeSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGccAttributeList(ComplierParser::GccAttributeListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGccAttribute(ComplierParser::GccAttributeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNestedParenthesesBlock(ComplierParser::NestedParenthesesBlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPointer(ComplierParser::PointerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeQualifierList(ComplierParser::TypeQualifierListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParameterTypeList(ComplierParser::ParameterTypeListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParameterList(ComplierParser::ParameterListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParameterDeclaration(ComplierParser::ParameterDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdentifierList(ComplierParser::IdentifierListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeName(ComplierParser::TypeNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypedefName(ComplierParser::TypedefNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitializer(ComplierParser::InitializerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitializerList(ComplierParser::InitializerListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDesignation(ComplierParser::DesignationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDesignatorList(ComplierParser::DesignatorListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDesignator(ComplierParser::DesignatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStaticAssertDeclaration(ComplierParser::StaticAssertDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(ComplierParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLabeledStatement(ComplierParser::LabeledStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundStatement(ComplierParser::CompoundStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockItemList(ComplierParser::BlockItemListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAsmADDer(ComplierParser::AsmADDerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockItem(ComplierParser::BlockItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpressionStatement(ComplierParser::ExpressionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSelectionStatement(ComplierParser::SelectionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIterationStatement(ComplierParser::IterationStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForCondition(ComplierParser::ForConditionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForDeclaration(ComplierParser::ForDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForExpression(ComplierParser::ForExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitJumpStatement(ComplierParser::JumpStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompilationUnit(ComplierParser::CompilationUnitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTranslationUnit(ComplierParser::TranslationUnitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternalDeclaration(ComplierParser::ExternalDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionDefinition(ComplierParser::FunctionDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeclarationList(ComplierParser::DeclarationListContext *ctx) override {
    return visitChildren(ctx);
  }


};

