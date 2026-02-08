#include "CParserBase.h"

// Generated from /home/toucher/vscoderope/mycomplier/grammar/CParser.g4 by ANTLR 4.13.1

#pragma once


#include "antlr4-runtime.h"
#include "CParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by CParser.
 */
class  CParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by CParser.
   */
    virtual std::any visitCompilationUnit(CParser::CompilationUnitContext *context) = 0;

    virtual std::any visitConstant(CParser::ConstantContext *context) = 0;

    virtual std::any visitEnumerationConstant(CParser::EnumerationConstantContext *context) = 0;

    virtual std::any visitPredefinedConstant(CParser::PredefinedConstantContext *context) = 0;

    virtual std::any visitPrimaryExpression(CParser::PrimaryExpressionContext *context) = 0;

    virtual std::any visitExprList(CParser::ExprListContext *context) = 0;

    virtual std::any visitGenericSelection(CParser::GenericSelectionContext *context) = 0;

    virtual std::any visitGenericAssocList(CParser::GenericAssocListContext *context) = 0;

    virtual std::any visitGenericAssociation(CParser::GenericAssociationContext *context) = 0;

    virtual std::any visitPostfixExpression(CParser::PostfixExpressionContext *context) = 0;

    virtual std::any visitArgumentExpressionList(CParser::ArgumentExpressionListContext *context) = 0;

    virtual std::any visitUnaryExpression(CParser::UnaryExpressionContext *context) = 0;

    virtual std::any visitCastExpression(CParser::CastExpressionContext *context) = 0;

    virtual std::any visitMultiplicativeExpression(CParser::MultiplicativeExpressionContext *context) = 0;

    virtual std::any visitAdditiveExpression(CParser::AdditiveExpressionContext *context) = 0;

    virtual std::any visitShiftExpression(CParser::ShiftExpressionContext *context) = 0;

    virtual std::any visitRelationalExpression(CParser::RelationalExpressionContext *context) = 0;

    virtual std::any visitEqualityExpression(CParser::EqualityExpressionContext *context) = 0;

    virtual std::any visitAndExpression(CParser::AndExpressionContext *context) = 0;

    virtual std::any visitExclusiveOrExpression(CParser::ExclusiveOrExpressionContext *context) = 0;

    virtual std::any visitInclusiveOrExpression(CParser::InclusiveOrExpressionContext *context) = 0;

    virtual std::any visitLogicalAndExpression(CParser::LogicalAndExpressionContext *context) = 0;

    virtual std::any visitLogicalOrExpression(CParser::LogicalOrExpressionContext *context) = 0;

    virtual std::any visitConditionalExpression(CParser::ConditionalExpressionContext *context) = 0;

    virtual std::any visitAssignmentExpression(CParser::AssignmentExpressionContext *context) = 0;

    virtual std::any visitExpression(CParser::ExpressionContext *context) = 0;

    virtual std::any visitConstantExpression(CParser::ConstantExpressionContext *context) = 0;

    virtual std::any visitDeclaration(CParser::DeclarationContext *context) = 0;

    virtual std::any visitDeclarationSpecifiers(CParser::DeclarationSpecifiersContext *context) = 0;

    virtual std::any visitDeclarationSpecifier(CParser::DeclarationSpecifierContext *context) = 0;

    virtual std::any visitInitDeclaratorList(CParser::InitDeclaratorListContext *context) = 0;

    virtual std::any visitInitDeclarator(CParser::InitDeclaratorContext *context) = 0;

    virtual std::any visitAttributeDeclaration(CParser::AttributeDeclarationContext *context) = 0;

    virtual std::any visitStorageClassSpecifier(CParser::StorageClassSpecifierContext *context) = 0;

    virtual std::any visitTypeSpecifier(CParser::TypeSpecifierContext *context) = 0;

    virtual std::any visitStructOrUnionSpecifier(CParser::StructOrUnionSpecifierContext *context) = 0;

    virtual std::any visitStructOrUnion(CParser::StructOrUnionContext *context) = 0;

    virtual std::any visitMemberDeclarationList(CParser::MemberDeclarationListContext *context) = 0;

    virtual std::any visitMemberDeclaration(CParser::MemberDeclarationContext *context) = 0;

    virtual std::any visitSpecifierQualifierList(CParser::SpecifierQualifierListContext *context) = 0;

    virtual std::any visitTypeSpecifierQualifier(CParser::TypeSpecifierQualifierContext *context) = 0;

    virtual std::any visitMemberDeclaratorList(CParser::MemberDeclaratorListContext *context) = 0;

    virtual std::any visitMemberDeclarator(CParser::MemberDeclaratorContext *context) = 0;

    virtual std::any visitEnumSpecifier(CParser::EnumSpecifierContext *context) = 0;

    virtual std::any visitEnumeratorList(CParser::EnumeratorListContext *context) = 0;

    virtual std::any visitEnumerator(CParser::EnumeratorContext *context) = 0;

    virtual std::any visitEnumTypeSpecifier(CParser::EnumTypeSpecifierContext *context) = 0;

    virtual std::any visitAtomicTypeSpecifier(CParser::AtomicTypeSpecifierContext *context) = 0;

    virtual std::any visitTypeofSpecifier(CParser::TypeofSpecifierContext *context) = 0;

    virtual std::any visitTypeofSpecifierArgument(CParser::TypeofSpecifierArgumentContext *context) = 0;

    virtual std::any visitTypeQualifier(CParser::TypeQualifierContext *context) = 0;

    virtual std::any visitFunctionSpecifier(CParser::FunctionSpecifierContext *context) = 0;

    virtual std::any visitAlignmentSpecifier(CParser::AlignmentSpecifierContext *context) = 0;

    virtual std::any visitDeclarator(CParser::DeclaratorContext *context) = 0;

    virtual std::any visitDirectDeclarator(CParser::DirectDeclaratorContext *context) = 0;

    virtual std::any visitPointer(CParser::PointerContext *context) = 0;

    virtual std::any visitTypeQualifierList(CParser::TypeQualifierListContext *context) = 0;

    virtual std::any visitParameterTypeList(CParser::ParameterTypeListContext *context) = 0;

    virtual std::any visitParameterList(CParser::ParameterListContext *context) = 0;

    virtual std::any visitParameterDeclaration(CParser::ParameterDeclarationContext *context) = 0;

    virtual std::any visitTypeName(CParser::TypeNameContext *context) = 0;

    virtual std::any visitAbstractDeclarator(CParser::AbstractDeclaratorContext *context) = 0;

    virtual std::any visitDirectAbstractDeclarator(CParser::DirectAbstractDeclaratorContext *context) = 0;

    virtual std::any visitTypedefName(CParser::TypedefNameContext *context) = 0;

    virtual std::any visitInitializer(CParser::InitializerContext *context) = 0;

    virtual std::any visitInitializerList(CParser::InitializerListContext *context) = 0;

    virtual std::any visitDesignation(CParser::DesignationContext *context) = 0;

    virtual std::any visitDesignatorList(CParser::DesignatorListContext *context) = 0;

    virtual std::any visitDesignator(CParser::DesignatorContext *context) = 0;

    virtual std::any visitStaticAssertDeclaration(CParser::StaticAssertDeclarationContext *context) = 0;

    virtual std::any visitAttributeSpecifierSequence(CParser::AttributeSpecifierSequenceContext *context) = 0;

    virtual std::any visitAttributeSpecifier(CParser::AttributeSpecifierContext *context) = 0;

    virtual std::any visitAttributeList(CParser::AttributeListContext *context) = 0;

    virtual std::any visitAttribute(CParser::AttributeContext *context) = 0;

    virtual std::any visitAttributeToken(CParser::AttributeTokenContext *context) = 0;

    virtual std::any visitAttributeArgumentClause(CParser::AttributeArgumentClauseContext *context) = 0;

    virtual std::any visitBalancedTokenSequence(CParser::BalancedTokenSequenceContext *context) = 0;

    virtual std::any visitBalancedToken(CParser::BalancedTokenContext *context) = 0;

    virtual std::any visitStatement(CParser::StatementContext *context) = 0;

    virtual std::any visitLabeledStatement(CParser::LabeledStatementContext *context) = 0;

    virtual std::any visitCompoundStatement(CParser::CompoundStatementContext *context) = 0;

    virtual std::any visitBlockItemList(CParser::BlockItemListContext *context) = 0;

    virtual std::any visitBlockItem(CParser::BlockItemContext *context) = 0;

    virtual std::any visitExpressionStatement(CParser::ExpressionStatementContext *context) = 0;

    virtual std::any visitSelectionStatement(CParser::SelectionStatementContext *context) = 0;

    virtual std::any visitIterationStatement(CParser::IterationStatementContext *context) = 0;

    virtual std::any visitForCondition(CParser::ForConditionContext *context) = 0;

    virtual std::any visitForDeclaration(CParser::ForDeclarationContext *context) = 0;

    virtual std::any visitForExpression(CParser::ForExpressionContext *context) = 0;

    virtual std::any visitJumpStatement(CParser::JumpStatementContext *context) = 0;

    virtual std::any visitTranslationUnit(CParser::TranslationUnitContext *context) = 0;

    virtual std::any visitExternalDeclaration(CParser::ExternalDeclarationContext *context) = 0;

    virtual std::any visitFunctionDefinition(CParser::FunctionDefinitionContext *context) = 0;

    virtual std::any visitDeclarationList(CParser::DeclarationListContext *context) = 0;

    virtual std::any visitFunctionBody(CParser::FunctionBodyContext *context) = 0;

    virtual std::any visitIdentifierList(CParser::IdentifierListContext *context) = 0;

    virtual std::any visitGnuArrayDesignator(CParser::GnuArrayDesignatorContext *context) = 0;

    virtual std::any visitGnuIdentifier(CParser::GnuIdentifierContext *context) = 0;

    virtual std::any visitAsmArgument(CParser::AsmArgumentContext *context) = 0;

    virtual std::any visitAsmClobbers(CParser::AsmClobbersContext *context) = 0;

    virtual std::any visitAsmDefinition(CParser::AsmDefinitionContext *context) = 0;

    virtual std::any visitToplevelAsmArgument(CParser::ToplevelAsmArgumentContext *context) = 0;

    virtual std::any visitAsmOperand(CParser::AsmOperandContext *context) = 0;

    virtual std::any visitAsmOperands(CParser::AsmOperandsContext *context) = 0;

    virtual std::any visitAsmQualifier(CParser::AsmQualifierContext *context) = 0;

    virtual std::any visitAsmQualifierList(CParser::AsmQualifierListContext *context) = 0;

    virtual std::any visitAsmStatement(CParser::AsmStatementContext *context) = 0;

    virtual std::any visitAsmStringLiteral(CParser::AsmStringLiteralContext *context) = 0;

    virtual std::any visitGccDeclaratorExtension(CParser::GccDeclaratorExtensionContext *context) = 0;

    virtual std::any visitGnuAttribute(CParser::GnuAttributeContext *context) = 0;

    virtual std::any visitGnuAttributeList(CParser::GnuAttributeListContext *context) = 0;

    virtual std::any visitGnuAttributes(CParser::GnuAttributesContext *context) = 0;

    virtual std::any visitGnuSingleAttribute(CParser::GnuSingleAttributeContext *context) = 0;

    virtual std::any visitSimpleAsmExpr(CParser::SimpleAsmExprContext *context) = 0;

    virtual std::any visitVcSpecificModifer(CParser::VcSpecificModiferContext *context) = 0;


};

