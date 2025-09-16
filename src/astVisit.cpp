#include "astVisit.h"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include <functional>
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
        visitExternalDeclaration(each);
    }
    return {};
}
std::any astVisitor::visitDeclaration(ComplierParser::DeclarationContext* ctx)
{
    Type basetype;
    std::vector<Type> vars;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto ret = ac<std::expected<std::vector<Type>, error>>(
            visitDeclarationSpecifiers(ctx->declarationSpecifiers()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        auto& declarationSpecifiers = ret.value();
        bool flag = false;

        if (declarationSpecifiers.back().id != "") // 可能是typedef或变量名
        {
            auto id = declarationSpecifiers.back().id;
            if (obj.typedefs.find(id) == obj.typedefs.end()) // 不是typedef,是id
            {
                declarationSpecifiers.pop_back();
                ctx->declarationSpecifiers()->children.pop_back();
                Type var;
                var.id = id;
                var.kind = Type::Kind::ID;
                vars.push_back(var);
            }
            else // 是type
            {
                basetype = obj.typedefs.find(id)->second;
                flag = true;
            }
        }

        for (auto each : ctx->declarationSpecifiers()->declarationSpecifier())
        {
            if (each->typeSpecifier())
            {
                if (each->typeSpecifier()->typedefName())
                {
                    auto id = each->typeSpecifier()->typedefName()->toString();
                    if (obj.typedefs.find(id) == obj.typedefs.end()) // 不是typedef,是id,忽略
                    {
                        continue;
                    }
                    else
                    {
                        if (flag)
                        {
                            return std::unexpected<error>(error::double_type);
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    return std::unexpected<error>(error::double_type);
                }
                basetype = ac<std::expected<Type, error>>(visitTypeSpecifier(each->typeSpecifier()))
                               .value();
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    if (ctx->initDeclaratorList()) // 数组/函数/指针的组合s
    {
        auto ret = ac<std::expected<std::vector<Type>, error>>(
            visitInitDeclaratorList(ctx->initDeclaratorList()));
        if (ret)
        {
            vars.insert(vars.end(), ret.value().begin(), ret.value().end());
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
    }
    for (auto& each : vars)
    {
        each.pushTop(basetype);
    }
    return std::expected<std::vector<Type>, error>(vars);
    // [TODO] 处理初始化器
    // 如果有初始化器，需要处理 ctx->initDeclaratorList()
}
std::any astVisitor::visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx)
{
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
    return {};
}
std::any astVisitor::visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx)
{
    std::vector<Type> Types;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        std::any result = visitDeclarationSpecifier(each);
        auto ret = std::any_cast<std::expected<Type, error>>(result);
        if (ret)
        {
            Types.push_back(ret.value());
        }
        else
        {
            // 返回错误
            return std::unexpected(ret.error());
        }
    }
    return std::expected<std::vector<Type>, error>(Types);
}
std::any astVisitor::visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx)
{
    // 检查是否为类型说明符
    if (ctx->typeSpecifier())
    {
        // 直接返回typeSpecifier的结果
        return visitTypeSpecifier(ctx->typeSpecifier());
    }
    // // [TODO] 处理存储类说明符
    // else if (ctx->storageClassSpecifier())
    // {
    //     // 暂时返回默认类型
    //     return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    // }
    // // [TODO] 处理类型限定符
    // else if (ctx->typeQualifier())
    // {
    //     // 暂时返回默认类型
    //     return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    // }
    // // [TODO] 处理函数说明符
    // else if (ctx->functionSpecifier())
    // {
    //     // 暂时返回默认类型
    //     return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    // }
    // // 处理对齐说明符
    // else if (ctx->alignmentSpecifier())
    // {
    //     // 暂时返回默认类型
    //     return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    // }
    // 如果无法识别说明符类型，返回错误
    return std::unexpected(error::unsurpport_basictype);
}
std::any astVisitor::visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx)
{
    Type rettype;
    // varDef rettpe;
    if (ctx->typedefName()) // 语法阶段无法判断是类型别名还是id, 均以typedefName表示
    {
        if (obj.typedefs.find(ctx->typedefName()->getText()) != obj.typedefs.end()) // 是类型别名
        {
            rettype = obj.typedefs.find(ctx->typedefName()->getText())->second;
            return std::expected<Type, error>(rettype);
        }
        else // 是id
        {
            rettype.id = ctx->typedefName()->getText();
            rettype.kind = Type::Kind::ID;
            return std::expected<Type, error>(rettype);
        }
    }
    // 判断基本类型
    rettype.kind = Type::Kind::Basic;
    if (ctx->getText() == "int")
    {
        rettype.basic_type = Type::BasicType::Int;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "char")
    {
        rettype.basic_type = Type::BasicType::Char;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "void")
    {
        rettype.basic_type = Type::BasicType::Void;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "float")
    {
        rettype.basic_type = Type::BasicType::Float;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "double")
    {
        rettype.basic_type = Type::BasicType::Double;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "long")
    {
        rettype.basic_type = Type::BasicType::Long;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "short")
    {
        rettype.basic_type = Type::BasicType::Short;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "unsigned")
    {
        rettype.basic_type = Type::BasicType::Unsigned;
        return std::expected<Type, error>(rettype);
    }
    else if (ctx->getText() == "signed")
    {
        rettype.basic_type = Type::BasicType::Signed;
        return std::expected<Type, error>(rettype);
    }
    return std::unexpected(error::unsurpport_basictype);
}
std::any astVisitor::visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx)
{
    std::vector<Type> vars;
    for (auto& each : ctx->initDeclarator())
    {
        auto ret = ac<std::expected<Type, error>>(visitInitDeclarator(each));
        if (ret)
        {
            vars.push_back(ret.value());
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<std::vector<Type>, error>(vars);
}
std::any astVisitor::visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx)
{
    auto ret = ac<std::expected<Type, error>>(visitDeclarator(ctx->declarator()));
    if (!ret)
    {
        return ret;
    }
    return ret;
    // [TODO]初始化ctx->initializer
}
// 后序递归生成
std::any astVisitor::visitDeclarator(ComplierParser::DeclaratorContext* ctx)
{
    auto ret = ac<std::expected<Type, error>>(visitDirectDeclarator(ctx->directDeclarator()));
    if (!ret)
    {
        return std::unexpected<error>(ret.error());
    }
    Type var = ret.value();
    if (ctx->pointer()) // 有ptr
    {
        auto str = ctx->pointer()->getText();
        auto tp =
            Type{Type::Kind::Pointer, static_cast<int>(std::count(str.begin(), str.end(), '*'))};
        var.pushTop(tp);
    }
    return std::expected<Type, error>(var);
}
// [REWRITE] ctx->getAltNumber()不是分支标识
std::any astVisitor::visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx)
{
    Type var;
    if (ctx->Identifier() && ctx->DigitSequence()) // 位域
    {
        return std::expected<Type, error>(var);
    }
    else if (ctx->Identifier()) // 变量名
    {
        var.kind = Type::Kind::ID;
        var.id = ctx->Identifier()->getText();
        return std::expected<Type, error>(var);
    }
    else if (ctx->LeftParen() && ctx->directDeclarator()) // 函数声明
    {
        auto ret = ac<std::expected<Type, error>>(visitDirectDeclarator(ctx->directDeclarator()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        var = ret.value();
        std::vector<Type> args;
        if (ctx->parameterTypeList())
        {
            auto ret = ac<std::expected<std::vector<Type>, error>>(
                visitParameterTypeList(ctx->parameterTypeList()));
            if (!ret)
            {
                return ret.error();
            }
            args = ret.value();
        }
        var.pushTop(Type{Type::Kind::Function, args});
        return std::expected<Type, error>(var);
    }
    else if (ctx->directDeclarator() && ctx->LeftBracket() &&
             ctx->assignmentExpression()) // base+ 数组
    {
        auto tpret = ac<std::expected<Type, error>>(visitDirectDeclarator(ctx->directDeclarator()));
        if (!tpret)
        {
            return std::unexpected<error>(tpret.error());
        }
        var = tpret.value();
        var.pushTop(Type{Type::Kind::Array, parseConstexpr(ctx->assignmentExpression())});
        return std::expected<Type, error>(var);
    }
    else if (ctx->declarator()) // (dec)
    {
        return visitDeclarator(ctx->declarator());
    }
    return std::unexpected<error>(error::unsurpport_directDeclarator);
}
std::any astVisitor::visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx)
{
    return visitParameterList(ctx->parameterList());
}
std::any astVisitor::visitParameterList(ComplierParser::ParameterListContext* ctx)
{
    std::vector<Type> vars;
    for (auto& each : ctx->parameterDeclaration())
    {
        auto ret = ac<std::expected<Type, error>>(visitParameterDeclaration(each));
        if (ret)
        {
            vars.push_back(ret.value());
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<std::vector<Type>, error>(vars);
}
std::any astVisitor::visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx)
{
    Type basetype;
    Type vars;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto ret = ac<std::expected<std::vector<Type>, error>>(
            visitDeclarationSpecifiers(ctx->declarationSpecifiers()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        auto& declarationSpecifiers = ret.value();
        bool flag = false;

        if (declarationSpecifiers.back().id != "") // 可能是typedef或变量名
        {
            auto id = declarationSpecifiers.back().id;
            if (obj.typedefs.find(id) ==
                obj.typedefs.end()) // 不是typedef,是id(在函数参数申明中应该不会发生)
            {
                return std::unexpected<error>(error::undifined_type);
            }
            else // 是type
            {
                basetype = obj.typedefs.find(id)->second;
                flag = true;
            }
        }

        for (auto each : ctx->declarationSpecifiers()->declarationSpecifier())
        {
            if (each->typeSpecifier())
            {
                if (each->typeSpecifier()->typedefName())
                {
                    auto id = each->typeSpecifier()->typedefName()->toString();
                    if (obj.typedefs.find(id) == obj.typedefs.end()) // 不是typedef,是id,忽略
                    {
                        continue;
                    }
                    else
                    {
                        if (flag)
                        {
                            return std::unexpected<error>(error::double_type);
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    return std::unexpected<error>(error::double_type);
                }
                basetype = ac<std::expected<Type, error>>(visitTypeSpecifier(each->typeSpecifier()))
                               .value();
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    else if (ctx->declarationSpecifiers2()) // 与declarationSpecifiers一致
    {
        auto ret = ac<std::expected<std::vector<Type>, error>>(
            visitDeclarationSpecifiers2(ctx->declarationSpecifiers2()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        auto& declarationSpecifiers = ret.value();
        bool flag = false;

        if (declarationSpecifiers.back().id != "") // 可能是typedef或变量名
        {
            auto id = declarationSpecifiers.back().id;
            if (obj.typedefs.find(id) ==
                obj.typedefs.end()) // 不是typedef,是id(在函数参数申明中应该不会发生)
            {
                return std::unexpected<error>(error::undifined_type);
            }
            else // 是type
            {
                basetype = obj.typedefs.find(id)->second;
                flag = true;
            }
        }

        for (auto each : ctx->declarationSpecifiers2()->declarationSpecifier())
        {
            if (each->typeSpecifier())
            {
                if (each->typeSpecifier()->typedefName())
                {
                    auto id = each->typeSpecifier()->typedefName()->toString();
                    if (obj.typedefs.find(id) == obj.typedefs.end()) // 不是typedef,是id,忽略
                    {
                        continue;
                    }
                    else
                    {
                        if (flag)
                        {
                            return std::unexpected<error>(error::double_type);
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    return std::unexpected<error>(error::double_type);
                }
                basetype = ac<std::expected<Type, error>>(visitTypeSpecifier(each->typeSpecifier()))
                               .value();
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    if (ctx->declarator()) // 数组/函数/指针的组合s
    {
        auto ret = ac<std::expected<Type, error>>(visitDeclarator(ctx->declarator()));
        if (ret)
        {
            vars = ret.value();
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
        vars.pushTop(basetype);
        return std::expected<Type, error>(vars);
    }
    else if (ctx->abstractDeclarator())
    {
        auto ret =
            ac<std::expected<Type, error>>(visitAbstractDeclarator(ctx->abstractDeclarator()));
        if (ret)
        {
            vars = ret.value();
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
        vars.pushTop(basetype);
        return std::expected<Type, error>(vars);
    }
    else
    {
        return std::expected<Type, error>(basetype);
    }
    return std::expected<Type, error>(vars);
}
std::any astVisitor::visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context* ctx)
{
    std::vector<Type> Types;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        std::any result = visitDeclarationSpecifier(each);
        auto ret = std::any_cast<std::expected<Type, error>>(result);
        if (ret)
        {
            Types.push_back(ret.value());
        }
        else
        {
            // 返回错误
            return std::unexpected(ret.error());
        }
    }
    return std::expected<std::vector<Type>, error>(Types);
}

std::any astVisitor::visitTypeName(ComplierParser::TypeNameContext* ctx)
{
    // typeName由specifierQualifierList和可选的abstractDeclarator组成
}

std::any astVisitor::visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx)
{
}

std::any astVisitor::visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx)
{
    Type rettype;
    if (ctx->directAbstractDeclarator())
    {
        auto ret = ac<std::expected<Type, error>>(
            visitDirectAbstractDeclarator(ctx->directAbstractDeclarator()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        rettype = ret.value();
    }
    if (ctx->pointer())
    {
        rettype.pushTop(
            Type{Type::Kind::Pointer, std::count(ctx->pointer()->getText().begin(),
                                                 ctx->pointer()->getText().end(), '*')});
    }
    return std::expected<Type, error>(rettype);
}

std::any astVisitor::visitDirectAbstractDeclarator(
    ComplierParser::DirectAbstractDeclaratorContext* ctx)
{
    Type basetype;
    if (ctx->LeftParen() && ctx->abstractDeclarator()) // (abst)
    {
        auto ret =
            ac<std::expected<Type, error>>(visitAbstractDeclarator(ctx->abstractDeclarator()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        basetype = ret.value();
        return std::expected<Type, error>(basetype);
    }
    if (ctx->directAbstractDeclarator())
    {
        auto ret = ac<std::expected<Type, error>>(
            visitDirectAbstractDeclarator(ctx->directAbstractDeclarator()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        basetype = ret.value();
    }
    if (ctx->LeftBracket() && ctx->assignmentExpression()) // arr
    {
        basetype.pushTop(Type{Type::Kind::Array, parseConstexpr(ctx->assignmentExpression())});
        return std::expected<Type, error>(basetype);
    }
    else if (ctx->LeftParen()) // func
    {
        std::vector<Type> args;
        if (ctx->parameterTypeList())
        {
            auto ret = ac<std::expected<std::vector<Type>, error>>(
                visitParameterTypeList(ctx->parameterTypeList()));
            if (!ret)
            {
                return std::unexpected<error>(ret.error());
            }
            args = ret.value();
        }
        basetype.pushTop(Type{Type::Kind::Function, args});
        return basetype;
    }
    return std::unexpected<error>(error::unsurpport_abstractDeclarator);
}
long long astVisitor::parseConstexpr(ComplierParser::AssignmentExpressionContext* expr)
{
    return 42;
};
astVisitor::astVisitor(std::string name, OBJ& ob) : obj(ob)
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
    case ComplierParser::RuleDeclaration:
        return visitDeclaration(dynamic_cast<ComplierParser::DeclarationContext*>(ctx));
        break;
    case ComplierParser::RuleFunctionDefinition:
        return visitFunctionDefinition(
            dynamic_cast<ComplierParser::FunctionDefinitionContext*>(ctx));
        break;
    case ComplierParser::RuleDeclarator:
        // 处理声明符，包括数组和函数指针等
        // 这里暂不实现
        break;
    case ComplierParser::RuleDirectDeclarator:
        // 处理直接声明符，包括数组维度等
        // 这里暂不实现
        break;
    // 更多类型的处理...
    default:
        // 对于未明确处理的节点类型，返回默认值
        return true;
    }

    return true; // 默认返回成功
}