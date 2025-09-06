#include "astVisit.h"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
std::any ComplierVisitor::visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx)
{
    ctx->getRuleIndex() == ComplierParser::RuleCompilationUnit;
    return visitTranslationUnit(ctx->translationUnit());
}

std::any ComplierVisitor::visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx)
{
    ctx->externalDeclaration();
}
