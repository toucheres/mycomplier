
// Generated from /home/toucher/vscoderope/mycomplier/grammar/Complier.g4 by ANTLR 4.13.1

#pragma once


#include "antlr4-runtime.h"
#include "ComplierParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by ComplierParser.
 */
class  ComplierVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by ComplierParser.
   */
    virtual std::any visitPrimaryExpression(ComplierParser::PrimaryExpressionContext *context) = 0;

    virtual std::any visitGenericSelection(ComplierParser::GenericSelectionContext *context) = 0;

    virtual std::any visitGenericAssocList(ComplierParser::GenericAssocListContext *context) = 0;

    virtual std::any visitGenericAssociation(ComplierParser::GenericAssociationContext *context) = 0;

    virtual std::any visitPostfixExpression(ComplierParser::PostfixExpressionContext *context) = 0;

    virtual std::any visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext *context) = 0;

    virtual std::any visitUnaryExpression(ComplierParser::UnaryExpressionContext *context) = 0;

    virtual std::any visitUnaryOperator(ComplierParser::UnaryOperatorContext *context) = 0;

    virtual std::any visitCastExpression(ComplierParser::CastExpressionContext *context) = 0;

    virtual std::any visitMultiplicativeExpression(ComplierParser::MultiplicativeExpressionContext *context) = 0;

    virtual std::any visitAdditiveExpression(ComplierParser::AdditiveExpressionContext *context) = 0;

    virtual std::any visitShiftExpression(ComplierParser::ShiftExpressionContext *context) = 0;

    virtual std::any visitRelationalExpression(ComplierParser::RelationalExpressionContext *context) = 0;

    virtual std::any visitEqualityExpression(ComplierParser::EqualityExpressionContext *context) = 0;

    virtual std::any visitAndExpression(ComplierParser::AndExpressionContext *context) = 0;

    virtual std::any visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext *context) = 0;

    virtual std::any visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext *context) = 0;

    virtual std::any visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext *context) = 0;

    virtual std::any visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext *context) = 0;

    virtual std::any visitConditionalExpression(ComplierParser::ConditionalExpressionContext *context) = 0;

    virtual std::any visitAssignmentExpression(ComplierParser::AssignmentExpressionContext *context) = 0;

    virtual std::any visitAssignmentOperator(ComplierParser::AssignmentOperatorContext *context) = 0;

    virtual std::any visitExpression(ComplierParser::ExpressionContext *context) = 0;

    virtual std::any visitConstantExpression(ComplierParser::ConstantExpressionContext *context) = 0;

    virtual std::any visitDeclaration(ComplierParser::DeclarationContext *context) = 0;

    virtual std::any visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext *context) = 0;

    virtual std::any visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context *context) = 0;

    virtual std::any visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext *context) = 0;

    virtual std::any visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext *context) = 0;

    virtual std::any visitInitDeclarator(ComplierParser::InitDeclaratorContext *context) = 0;

    virtual std::any visitStorageClassSpecifier(ComplierParser::StorageClassSpecifierContext *context) = 0;

    virtual std::any visitTypeSpecifier(ComplierParser::TypeSpecifierContext *context) = 0;

    virtual std::any visitStructOrUnionSpecifier(ComplierParser::StructOrUnionSpecifierContext *context) = 0;

    virtual std::any visitStructOrUnion(ComplierParser::StructOrUnionContext *context) = 0;

    virtual std::any visitStructDeclarationList(ComplierParser::StructDeclarationListContext *context) = 0;

    virtual std::any visitStructDeclaration(ComplierParser::StructDeclarationContext *context) = 0;

    virtual std::any visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext *context) = 0;

    virtual std::any visitStructDeclaratorList(ComplierParser::StructDeclaratorListContext *context) = 0;

    virtual std::any visitStructDeclarator(ComplierParser::StructDeclaratorContext *context) = 0;

    virtual std::any visitEnumSpecifier(ComplierParser::EnumSpecifierContext *context) = 0;

    virtual std::any visitEnumeratorList(ComplierParser::EnumeratorListContext *context) = 0;

    virtual std::any visitEnumerator(ComplierParser::EnumeratorContext *context) = 0;

    virtual std::any visitEnumerationConstant(ComplierParser::EnumerationConstantContext *context) = 0;

    virtual std::any visitAtomicTypeSpecifier(ComplierParser::AtomicTypeSpecifierContext *context) = 0;

    virtual std::any visitTypeQualifier(ComplierParser::TypeQualifierContext *context) = 0;

    virtual std::any visitFunctionSpecifier(ComplierParser::FunctionSpecifierContext *context) = 0;

    virtual std::any visitAlignmentSpecifier(ComplierParser::AlignmentSpecifierContext *context) = 0;

    virtual std::any visitDeclarator(ComplierParser::DeclaratorContext *context) = 0;

    virtual std::any visitDirectDeclarator(ComplierParser::DirectDeclaratorContext *context) = 0;

    virtual std::any visitVcSpecificModifer(ComplierParser::VcSpecificModiferContext *context) = 0;

    virtual std::any visitGccDeclaratorExtension(ComplierParser::GccDeclaratorExtensionContext *context) = 0;

    virtual std::any visitGccAttributeSpecifier(ComplierParser::GccAttributeSpecifierContext *context) = 0;

    virtual std::any visitGccAttributeList(ComplierParser::GccAttributeListContext *context) = 0;

    virtual std::any visitGccAttribute(ComplierParser::GccAttributeContext *context) = 0;

    virtual std::any visitNestedParenthesesBlock(ComplierParser::NestedParenthesesBlockContext *context) = 0;

    virtual std::any visitPointer(ComplierParser::PointerContext *context) = 0;

    virtual std::any visitTypeQualifierList(ComplierParser::TypeQualifierListContext *context) = 0;

    virtual std::any visitParameterTypeList(ComplierParser::ParameterTypeListContext *context) = 0;

    virtual std::any visitParameterList(ComplierParser::ParameterListContext *context) = 0;

    virtual std::any visitParameterDeclaration(ComplierParser::ParameterDeclarationContext *context) = 0;

    virtual std::any visitIdentifierList(ComplierParser::IdentifierListContext *context) = 0;

    virtual std::any visitTypeName(ComplierParser::TypeNameContext *context) = 0;

    virtual std::any visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext *context) = 0;

    virtual std::any visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext *context) = 0;

    virtual std::any visitTypedefName(ComplierParser::TypedefNameContext *context) = 0;

    virtual std::any visitInitializer(ComplierParser::InitializerContext *context) = 0;

    virtual std::any visitInitializerList(ComplierParser::InitializerListContext *context) = 0;

    virtual std::any visitDesignation(ComplierParser::DesignationContext *context) = 0;

    virtual std::any visitDesignatorList(ComplierParser::DesignatorListContext *context) = 0;

    virtual std::any visitDesignator(ComplierParser::DesignatorContext *context) = 0;

    virtual std::any visitStaticAssertDeclaration(ComplierParser::StaticAssertDeclarationContext *context) = 0;

    virtual std::any visitStatement(ComplierParser::StatementContext *context) = 0;

    virtual std::any visitLabeledStatement(ComplierParser::LabeledStatementContext *context) = 0;

    virtual std::any visitCompoundStatement(ComplierParser::CompoundStatementContext *context) = 0;

    virtual std::any visitBlockItemList(ComplierParser::BlockItemListContext *context) = 0;

    virtual std::any visitBlockItem(ComplierParser::BlockItemContext *context) = 0;

    virtual std::any visitExpressionStatement(ComplierParser::ExpressionStatementContext *context) = 0;

    virtual std::any visitSelectionStatement(ComplierParser::SelectionStatementContext *context) = 0;

    virtual std::any visitIterationStatement(ComplierParser::IterationStatementContext *context) = 0;

    virtual std::any visitForCondition(ComplierParser::ForConditionContext *context) = 0;

    virtual std::any visitForDeclaration(ComplierParser::ForDeclarationContext *context) = 0;

    virtual std::any visitForExpression(ComplierParser::ForExpressionContext *context) = 0;

    virtual std::any visitJumpStatement(ComplierParser::JumpStatementContext *context) = 0;

    virtual std::any visitCompilationUnit(ComplierParser::CompilationUnitContext *context) = 0;

    virtual std::any visitTranslationUnit(ComplierParser::TranslationUnitContext *context) = 0;

    virtual std::any visitExternalDeclaration(ComplierParser::ExternalDeclarationContext *context) = 0;

    virtual std::any visitFunctionDefinition(ComplierParser::FunctionDefinitionContext *context) = 0;

    virtual std::any visitDeclarationList(ComplierParser::DeclarationListContext *context) = 0;


};

