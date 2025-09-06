#pragma once
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "error.hpp"
#include "obj.h"
#include <antlr4-runtime/antlr4-runtime.h>
struct ComplierVisitor : public ComplierBaseVisitor
{
    OBJ obj;
    std::any visitCompilationUnit(ComplierParser::CompilationUnitContext * ctx) override;
    std::any visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx) override;
};