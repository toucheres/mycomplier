#include "astVisit.h"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include <tree/TerminalNode.h>
template <class CAST> CAST ac(auto&& in)
{
    return std::any_cast<CAST>(in);
}
template <class CAST> CAST dc(auto&& in)
{
    return dynamic_cast<CAST>(in);
}
std::any astVisitor::visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx)
{
    // ctx->getRuleIndex() == ComplierParser::RuleCompilationUnit;
    return visitTranslationUnit(ctx->translationUnit());
}
std::any astVisitor::visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx)
{
    for (auto each : ctx->externalDeclaration())
    {
        if (ac<bool>(visitByTypeIndex(each)))
        {
        }
    }
    return {};
}
std::any astVisitor::visitDeclaration(ComplierParser::DeclarationContext* ctx)
{

    if (auto ret = varDef::makeByNode(ctx))
    {
        obj.symbol_table.add_global_symbol(ret.value());
        funcnow = obj.symbol_table.lookup_fun(ret.value().name);
    }
    else if (auto ret2 = funcDef::makeByNode(ctx))
    {
        obj.symbol_table.add_global_symbol(ret2.value());
        funcnow = obj.symbol_table.lookup_fun(ret2.value().name);
    }
    return true;
    // [TODO] 初始化
    // ctx->initDeclaratorList();
}
std::any astVisitor::visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx)
{
    return std::any();
}
std::any astVisitor::visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx)
{
    // 检查是否为函数定义
    if (ctx->functionDefinition())
    {
        return visitFunctionDefinition(ctx->functionDefinition());
    }

    // 检查是否为变量声明
    else if (ctx->declaration())
    {
        return visitDeclaration(ctx->declaration());
    }

    // 检查是否为单独的分号（空语句）
    else if (ctx->children.size() == 1 && ctx->children[0]->getText() == ";")
    {
        // 空语句不需要特别处理
        return {};
    }
    std::cerr << "警告: 未识别的外部声明类型" << std::endl;
    return {};
}
std::any astVisitor::visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx)
{
    std::vector<varDef> vars;
    for (auto each : ctx->declarationSpecifier())
    {

        auto ret = ac<std::expected<varDef, error>>(visitDeclarationSpecifier(each));
        if (ret)
        {
            vars.push_back(ret.value());
        }
        else
        {
            return std::unexpected(ret.error());
        }
    }
    return std::expected<std::vector<varDef>, error>(vars);
}
std::any astVisitor::visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx)
{
    // [TODO] storageClassSpecifier
    ctx;
}
std::any astVisitor::visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx)
{
    auto typeSpec = ctx;
    Type::BasicType ty;
    // 判断基本类型
    if (typeSpec->getText() == "int")
    {
        ty = Type::BasicType::Int;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "char")
    {
        ty = Type::BasicType::Char;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "void")
    {
        ty = Type::BasicType::Void;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "float")
    {
        ty = Type::BasicType::Float;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "double")
    {
        ty = Type::BasicType::Double;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "long")
    {
        ty = Type::BasicType::Long;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "short")
    {
        ty = Type::BasicType::Short;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "unsigned")
    {
        ty = Type::BasicType::Unsigned;
        return std::expected<Type::BasicType, error>(ty);
    }
    else if (typeSpec->getText() == "signed")
    {
        ty = Type::BasicType::Signed;
        return std::expected<Type::BasicType, error>(ty);
    }
    return std::unexpected(error::unsurpport_basictype);
}
astVisitor::astVisitor(std::string name)
{
    funcDef fun;
    fun.name = "__global_init" + name;
    obj.symbol_table.add_global_symbol(fun);
    globalinitfun = obj.symbol_table.lookup_fun("__global_init" + name);
}
std::any astVisitor::visitByTypeIndex(antlr4::ParserRuleContext* ctx)
{
    switch (ctx->getRuleIndex())
    {
    case ComplierParser::RuleCompilationUnit:
        return visitCompilationUnit(dynamic_cast<ComplierParser::CompilationUnitContext*>(ctx));
        break;
    case ComplierParser::RuleTranslationUnit:
        return visitTranslationUnit(dynamic_cast<ComplierParser::TranslationUnitContext*>(ctx));
        break;
    case ComplierParser::RuleExternalDeclaration:
        return visitExternalDeclaration(
            dynamic_cast<ComplierParser::ExternalDeclarationContext*>(ctx));
        break;
    // case ComplierParser::RuleX:
    //     return visitX(dynamic_cast<ComplierParser::XContext*>(ctx));
    //     break;
    default:
        break;
    }
}