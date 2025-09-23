#include "astVisit.h"
#include "ASM.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "vm.h"
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
            visitDeclarationSpecifiers(ctx->declarationSpecifiers())); // 不带初始化的声明
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
    if (ctx->initDeclaratorList()) // 带初始化的参数
    {
        baseType = basetype;
        auto ret = ac<std::expected<std::vector<Type>, error>>(
            visitInitDeclaratorList(ctx->initDeclaratorList()));
        if (ret)
        {
            // vars.insert(vars.end(), ret.value().begin(), ret.value().end()); //
            // 已在initdecltor中添加
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
    Type basetype;
    auto ret = ac<std::expected<Type, error>>(visitDeclarator(ctx->declarator()));
    if (ret)
    {
        basetype = ret.value();
    }
    else
    {
        return std::unexpected<error>(ret.error());
    }
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
                        basetype.pushTop(obj.typedefs.find(id)->second);
                        flag = true;
                    }
                }
                if (flag)
                {
                    return std::unexpected<error>(error::double_type);
                }
                basetype.pushTop(
                    ac<std::expected<Type, error>>(visitTypeSpecifier(each->typeSpecifier()))
                        .value());
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    auto funnowptr = obj.symbol_table.add_global_func_def(basetype);
    if (!funnowptr)
    {
        return std::unexpected<error>(error::double_defined);
    }
    auto gfunptr = funcnow;
    funcnow = funnowptr;
    funcnow->asms.push_back("HOLD");
    auto compoundRet =
        ac<std::expected<bool, error>>(visitCompoundStatement(ctx->compoundStatement()));
    if (!compoundRet)
    {
        funcnow = gfunptr;
        return std::unexpected<error>(compoundRet.error());
    }
    else
    {
        auto align_up = [](int num, int align) -> int
        {
            if (num % align == 0)
            {
                return num;
            }
            else
            {
                return num + (align - num % align);
            }
        };
        funcnow->asms[0] =
            ASM{ASM::basic_asm::NVAR,
                align_up(funcnow->max_stack_size, VCPU<>::size_word) / VCPU<>::size_word};
        funcnow = gfunptr;
        return std::expected<bool, error>(true);
    }
}
std::any astVisitor::visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx)
{
    // 检查是否为函数定义
    if (ctx->functionDefinition())
    {
        // 在visitFunctionDefinition内部处理
        visitFunctionDefinition(ctx->functionDefinition());
        return {};
    }

    // 检查是否为变量定义
    // [TODO] 区分变量定义与声明
    else if (ctx->declaration())
    {
        auto ret =
            ac<std::expected<std::vector<Type>, error>>(visitDeclaration(ctx->declaration()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        for (auto& each : ret.value())
        {
            obj.symbol_table.add_global_var_def(each);
        }
    }
    else if (ctx->asmADDer()) // 拓展
    {
        visitAsmADDer(ctx->asmADDer());
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
        return std::unexpected<error>(ret.error());
    }
    ret.value().pushTop(baseType);
    if (funcnow->name != "__global_init" + obj.name) // 局部
    {
        varDef var;
        var.type = *ret.value().subType;
        var.name = ret.value().id;
        funcnow->add_var(var);
    }
    else
    {
        obj.symbol_table.add_global_var_def(ret.value());
    }
    std::function<std::expected<bool, error>(Type, ComplierParser::InitializerContext*)> func =
        [&func, this](Type arg,
                      ComplierParser::InitializerContext* init) -> std::expected<bool, error>
    {
        // addr通过运行时栈传递
        auto asmholder = funcnow;
        if (arg.kind == Type::Kind::ID)
        {
            if (funcnow->lookup_var(arg.id)) // 函数局部
            {
                funcnow->asms.push_back(
                    ASM{ASM::basic_asm::IMM, funcnow->lookup_var(arg.id)->addr});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LEA});
            }
            else if (obj.symbol_table.lookup_var_decl(arg.id))
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, "globalvar@" + arg.id});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LEAD});
            }
            arg = *arg.subType;
        }
        if (arg.kind == Type::Kind::Basic || arg.kind == Type::Kind::Pointer)
        {
            if (init->assignmentExpression()) // = expr
            {
                auto ret = ac<std::expected<Type, error>>(
                    visitAssignmentExpression(init->assignmentExpression()));
                if (!ret)
                {
                    return std::unexpected<error>(ret.error());
                }
                if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::SC});
                }
                else if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::SI});
                }
                else if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::SW});
                }
            }
        }
        else if (arg.kind == Type::Kind::Array)
        {
            size_t initListSize = 0;
            if (init->initializerList())
            {
                initListSize = init->initializerList()->initializer().size();
            }
            for (size_t i = 0;
                 i < std::min(static_cast<size_t>(arg.arr_or_ptr_num), initListSize) - 1; i++)
            {
                asmholder->asms.push_back(ASM{ASM::basic_asm::COPY});
            }
            for (size_t i = 0;
                 i < std::min(static_cast<size_t>(arg.arr_or_ptr_num), initListSize); i++)
            {
                if (i != 0)
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::IMM, arg.subType->getsize() * i});
                    asmholder->asms.push_back(ASM{ASM::basic_asm::ADD});
                }
                if (init->initializerList()->initializer(i))
                {
                    auto ret = func(*arg.subType, init->initializerList()->initializer(i));
                    if (!ret)
                    {
                        return std::unexpected<error>(ret.error());
                    }
                }
            }
        }
        return true;
    };
    if (ctx->initializer())
    {
        auto fret = func(ret.value(), ctx->initializer());
        if (!fret)
        {
            return std::unexpected<error>(fret.error());
        }
    }
    return std::expected<Type, error>(ret);
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
std::any astVisitor::visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx)
{
    if (auto ptr = ctx->blockItemList())
    {
        funcnow->enter_scope();
        auto ret = ac<std::expected<bool, error>>(visitBlockItemList(ctx->blockItemList()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        funcnow->exit_scope();
    }
    return std::expected<bool, error>(true);
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
std::any astVisitor::visitBlockItemList(ComplierParser::BlockItemListContext* ctx)
{
    for (auto each : ctx->blockItem())
    {
        if (auto ret = ac<std::expected<bool, error>>(visitBlockItem(each)); !ret)
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<bool, error>(true);
}
std::any astVisitor::visitStatement(ComplierParser::StatementContext* ctx)
{
    if (ctx->compoundStatement())
    {
        return visitCompoundStatement(ctx->compoundStatement());
    }
    else if (ctx->expressionStatement())
    {
        return visitExpressionStatement(ctx->expressionStatement());
    }
    else if (ctx->selectionStatement())
    {
        return visitSelectionStatement(ctx->selectionStatement());
    }
    else if (ctx->iterationStatement())
    {
        return visitIterationStatement(ctx->iterationStatement());
    }
    else if (ctx->jumpStatement())
    {
        return visitJumpStatement(ctx->jumpStatement());
    }
    // [TODO] lable system
    else if (ctx->labeledStatement())
    {
        return visitLabeledStatement(ctx->labeledStatement());
    }
}
std::any astVisitor::visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx)
{
    if (ctx->expression())
    {
        auto ret = ac<std::expected<Type, error>>(visitExpression(ctx->expression()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
    }
    return std::expected<bool, error>(true);
}
std::any astVisitor::visitExpression(ComplierParser::ExpressionContext* ctx)
{
    for (int i = 0; i < ctx->assignmentExpression().size(); i++)
    {
        auto ret = ac<std::expected<Type, error>>(
            visitAssignmentExpression(ctx->assignmentExpression()[i]));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        if (i != ctx->assignmentExpression().size() - 1)
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        }
        else
        {
            return std::expected<Type, error>(ret.value());
        }
    }
}
std::any astVisitor::visitAssignmentExpression(ComplierParser::AssignmentExpressionContext* ctx)
{
    // [TODO] DigitSequence
    if (ctx->conditionalExpression())
    {
        return visitConditionalExpression(ctx->conditionalExpression());
    }
    else if (ctx->assignmentOperator())
    {
        auto uret = ac<std::expected<Type, error>>(visitUnaryExpression(ctx->unaryExpression()));
        if (!uret)
        {
            return std::unexpected<error>(uret.error());
        }
        if (funcnow->asms.back() != "LC" && funcnow->asms.back() != "LI" &&
            funcnow->asms.back() != "LW") // 不是左值
        {
            return std::unexpected<error>(error::expected_lvalue);
        }
        funcnow->asms.pop_back();
        if (ctx->assignmentOperator()->getText() == "=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY}); // 拷贝一份左值地址实现返回值
            auto aret = ac<std::expected<Type, error>>(
                visitAssignmentExpression(ctx->assignmentExpression()));
            if (!aret)
            {
                return std::unexpected<error>(aret.error());
            }
            if (uret.value().getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (uret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
        }
        else
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            funcnow->asms.push_back(
                ASM{ASM::basic_asm::PUSH}); // 拷贝两份左值地址实现取值运算，存值，返回值
            if (uret.value().getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            auto aret = ac<std::expected<Type, error>>(
                visitAssignmentExpression(ctx->assignmentExpression()));
            if (!aret)
            {
                return std::unexpected<error>(aret.error());
            }

            if (ctx->assignmentOperator()->getText() == "+=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
            }
            else if (ctx->assignmentOperator()->getText() == "-=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
            }
            else if (ctx->assignmentOperator()->getText() == "*=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::MUL});
            }
            else if (ctx->assignmentOperator()->getText() == "/=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::DIV});
            }
            else if (ctx->assignmentOperator()->getText() == "%=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::MOD});
            }
            else if (ctx->assignmentOperator()->getText() == "<<=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LSHIFT});
            }
            else if (ctx->assignmentOperator()->getText() == ">>=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::RSHIFT});
            }
            else if (ctx->assignmentOperator()->getText() == "^=")
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::XOR});
            }
            // [TODO] bit operator asm

            if (uret.value().getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (uret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
        }
        return std::expected<Type, error>(uret.value());
    }
}
std::any astVisitor::visitConditionalExpression(ComplierParser::ConditionalExpressionContext* ctx)
{
    if (ctx->logicalOrExpression())
    {
        auto lret =
            ac<std::expected<Type, error>>(visitLogicalOrExpression(ctx->logicalOrExpression()));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        return std::expected<Type, error>(lret.value());
    }
    if (ctx->expression() && ctx->conditionalExpression())
    {
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD");
        auto eret = ac<std::expected<Type, error>>(visitExpression(ctx->expression()));
        if (!eret)
        {
            return std::unexpected<error>(eret.error());
        }
        funcnow->asms[pos] = ASM{ASM::basic_asm::JZ, funcnow->asms.size() + 1}; // 跳过JMP
        int pos2 = funcnow->asms.size();
        funcnow->asms.push_back("HOLD");
        auto cret = ac<std::expected<Type, error>>(
            visitConditionalExpression(ctx->conditionalExpression()));
        if (!cret)
        {
            return std::unexpected<error>(cret.error());
        }
        funcnow->asms[pos2] = ASM{ASM::basic_asm::JMP, funcnow->asms.size()};
        return std::expected<Type, error>(Type{Type::Kind::Basic, Type::BasicType::Char});
    }
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
std::any astVisitor::visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext* ctx)
{
    // 注意处理多个||
    std::function<std::expected<Type, error>(
        std::span<ComplierParser::LogicalAndExpressionContext*>)>
        func = [&func, this](std::span<ComplierParser::LogicalAndExpressionContext*> in)
        -> std::expected<Type, error>
    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitLogicalAndExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitLogicalAndExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1)));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        funcnow->asms[pos] = ASM{ASM::basic_asm::JNZ, funcnow->asms.size()};
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::LogicalAndExpressionContext*>(
        ctx->logicalAndExpression().data(), ctx->logicalAndExpression().size()));
}
std::any astVisitor::visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext* ctx)
{
    // 注意处理多个&&
    std::function<std::expected<Type, error>(
        std::span<ComplierParser::InclusiveOrExpressionContext*>)>
        func = [&func, this](std::span<ComplierParser::InclusiveOrExpressionContext*> in)
        -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitInclusiveOrExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitInclusiveOrExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1)));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        funcnow->asms[pos] = ASM{ASM::basic_asm::JZ, funcnow->asms.size()};
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::InclusiveOrExpressionContext*>(
        ctx->inclusiveOrExpression().data(), ctx->inclusiveOrExpression().size()));
}
std::any astVisitor::visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext* ctx)
{
    // 注意处理多个|
    std::function<std::expected<Type, error>(
        std::span<ComplierParser::ExclusiveOrExpressionContext*>)>
        func = [&func, this](std::span<ComplierParser::ExclusiveOrExpressionContext*> in)
        -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitExclusiveOrExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitExclusiveOrExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1)));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::OR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::ExclusiveOrExpressionContext*>(
        ctx->exclusiveOrExpression().data(), ctx->exclusiveOrExpression().size()));
}
std::any astVisitor::visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext* ctx)
{
    // 注意处理多个^
    std::function<std::expected<Type, error>(std::span<ComplierParser::AndExpressionContext*>)>
        func =
            [&func, this](
                std::span<ComplierParser::AndExpressionContext*> in) -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitAndExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitAndExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1)));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::AndExpressionContext*>(ctx->andExpression().data(),
                                                                 ctx->andExpression().size()));
}
std::any astVisitor::visitAndExpression(ComplierParser::AndExpressionContext* ctx)
{
    // 注意处理多个&
    std::function<std::expected<Type, error>(std::span<ComplierParser::EqualityExpressionContext*>)>
        func = [&func, this](std::span<ComplierParser::EqualityExpressionContext*> in)
        -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitEqualityExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitEqualityExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1)));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::EqualityExpressionContext*>(
        ctx->equalityExpression().data(), ctx->equalityExpression().size()));
}
std::any astVisitor::visitEqualityExpression(ComplierParser::EqualityExpressionContext* ctx)
{
    // 注意处理多个!= / ==
    std::function<std::expected<Type, error>(
        std::span<ComplierParser::RelationalExpressionContext*>, int)>
        func = [&func, this, ctx](std::span<ComplierParser::RelationalExpressionContext*> in,
                                  int index) -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitRelationalExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitRelationalExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1), index + 1));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        if (ctx->children[index * 2 + 1]->getText() == "==")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::CMP});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "!=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::CMPN});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::RelationalExpressionContext*>(
                    ctx->relationalExpression().data(), ctx->relationalExpression().size()),
                0);
}
std::any astVisitor::visitRelationalExpression(ComplierParser::RelationalExpressionContext* ctx)
{
    // 注意处理多个< > <= >=
    std::function<std::expected<Type, error>(std::span<ComplierParser::ShiftExpressionContext*>,
                                             int)>
        func = [&func, this, ctx](std::span<ComplierParser::ShiftExpressionContext*> in,
                                  int index) -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitShiftExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitShiftExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1), index + 1));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        if (ctx->children[index * 2 + 1]->getText() == "<")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::SMALL});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::BIG});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "<=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::SMALLE});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::BIGE});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::ShiftExpressionContext*>(ctx->shiftExpression().data(),
                                                                   ctx->shiftExpression().size()),
                0);
}
std::any astVisitor::visitShiftExpression(ComplierParser::ShiftExpressionContext* ctx)
{
    // 注意处理多个<< >>
    std::function<std::expected<Type, error>(std::span<ComplierParser::AdditiveExpressionContext*>,
                                             int)>
        func = [&func, this, ctx](std::span<ComplierParser::AdditiveExpressionContext*> in,
                                  int index) -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitAdditiveExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitAdditiveExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1), index + 1));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        if (ctx->children[index * 2 + 1]->getText() == "<<")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::LSHIFT});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">>")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::RSHIFT});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::AdditiveExpressionContext*>(
                    ctx->additiveExpression().data(), ctx->additiveExpression().size()),
                0);
}
std::any astVisitor::visitAdditiveExpression(ComplierParser::AdditiveExpressionContext* ctx)
{
    // 注意处理多个+ -
    std::function<std::expected<Type, error>(
        std::span<ComplierParser::MultiplicativeExpressionContext*>, int)>
        func = [&func, this, ctx](std::span<ComplierParser::MultiplicativeExpressionContext*> in,
                                  int index) -> std::expected<Type, error>

    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitMultiplicativeExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitMultiplicativeExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1), index + 1));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        if (ctx->children[index * 2 + 1]->getText() == "+")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "-")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::MultiplicativeExpressionContext*>(
                    ctx->multiplicativeExpression().data(), ctx->multiplicativeExpression().size()),
                0);
}
std::any astVisitor::visitMultiplicativeExpression(
    ComplierParser::MultiplicativeExpressionContext* ctx)
{
    // 注意处理多个* / %
    std::function<std::expected<Type, error>(std::span<ComplierParser::CastExpressionContext*>,
                                             int)>
        func = [&func, this, ctx](std::span<ComplierParser::CastExpressionContext*> in,
                                  int index) -> std::expected<Type, error>
    {
        if (in.size() == 1)
        {
            return ac<std::expected<Type, error>>(visitCastExpression(in[0]));
        }
        auto lret = ac<std::expected<Type, error>>(visitCastExpression(in[0]));
        if (!lret)
        {
            return std::unexpected<error>(lret.error());
        }
        // 计算右侧表达式
        auto rret = ac<std::expected<Type, error>>(func(in.subspan(1, in.size() - 1), index + 1));
        if (!rret)
        {
            return std::unexpected<error>(rret.error());
        }
        if (ctx->children[index * 2 + 1]->getText() == "*")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::MUL});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "/")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::DIV});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "%")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::MOD});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::CastExpressionContext*>(ctx->castExpression().data(),
                                                                  ctx->castExpression().size()),
                0);
}
std::any astVisitor::visitCastExpression(ComplierParser::CastExpressionContext* ctx)
{
    if (ctx->castExpression())
    {
        auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
        if (!cret)
        {
            return std::unexpected<error>(cret.error());
        }
        // [TODO] visitTypeName
        auto tret = ac<std::expected<Type, error>>(visitTypeName(ctx->typeName()));
        if (!tret)
        {
            return std::unexpected<error>(tret.error());
        }
        return std::expected<Type, error>(tret.value());
    }
    else if (ctx->unaryExpression())
    {
        return visitUnaryExpression(ctx->unaryExpression());
    }
    //[TODO] DigitSequence 何意义?
}

std::any astVisitor::visitUnaryExpression(ComplierParser::UnaryExpressionContext* ctx)
{
    Type type;
    if (ctx->postfixExpression())
    {
        auto pret =
            ac<std::expected<Type, error>>(visitPostfixExpression(ctx->postfixExpression()));
        if (!pret)
        {
            return std::unexpected<error>(pret.error());
        }
        type = pret.value();
    }
    else if (ctx->unaryOperator())
    {
        if (ctx->unaryOperator()->getText() == "&")
        {
            auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
            if (!cret)
            {
                return std::unexpected<error>(cret.error());
            }
            if (cret.value().kind == Type::Kind::Function ||
                cret.value().kind ==
                    Type::Kind::Array) // arr/function无LC/LI/LW,取地址与值相同，无需处理
            {
                type = Type{Type::Kind::Pointer, 1};
                type.pushTop(cret.value());
            }
            else
            {
                if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI" ||
                    funcnow->asms.back() == "LW")
                {
                    funcnow->asms.pop_back();
                }
                else
                {
                    return std::unexpected<error>(error::expected_lvalue);
                }
                if (cret.value().kind == Type::Kind::Pointer)
                {
                    type = cret.value();
                    type.arr_or_ptr_num++;
                }
                else
                {
                    type = Type{Type::Kind::Pointer, 1};
                    type.pushTop(cret.value());
                }
            }
        }
        else if (ctx->unaryOperator()->getText() == "*")
        {
            auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
            if (!cret)
            {
                return std::unexpected<error>(cret.error());
            }
            if (cret.value().kind != Type::Kind::Pointer)
            {
                return std::unexpected<error>(error::expected_ptr);
            }
            type = *cret.value().subType;
            if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
        }
        else if (ctx->unaryOperator()->getText() == "+") //+12
        {
            auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
            if (!cret)
            {
                return std::unexpected<error>(cret.error());
            }
            type = cret.value();
        }
        else if (ctx->unaryOperator()->getText() == "-") //-12
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
            if (!cret)
            {
                return std::unexpected<error>(cret.error());
            }
            type = cret.value();
            funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
        }
    }
    else if (ctx->typeName())
    {
        auto cret = ac<std::expected<Type, error>>(visitTypeName(ctx->typeName()));
        if (!cret)
        {
            return std::unexpected<error>(cret.error());
        }
        if ((ctx->typeName() - 1)->getText() == "sizeof")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, cret.value().getsize()});
            type = Type{Type::Kind::Basic, Type::BasicType::Int};
        }
        // [TODO] alignas
    }
    for (int i = ctx->children.size() - 1; i >= 0; i--)
    {
        if (ctx->children[i]->getText() == "++")
        {
            if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI" ||
                funcnow->asms.back() == "LW")
            {
                funcnow->asms.pop_back();
            }
            else
            {
                return std::unexpected<error>(error::expected_lvalue);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
            }
        }
        else if (ctx->children[i]->getText() == "--")
        {
            if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI" ||
                funcnow->asms.back() == "LW")
            {
                funcnow->asms.pop_back();
            }
            else
            {
                return std::unexpected<error>(error::expected_lvalue);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 1});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
            }
        }
        //[TODO] sizeof无副作用
        else if (ctx->children[i]->getText() == "sizeof" &&
                 ctx->children[i + 1]->getText() != "(") // 排除分支3的sizeof
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, type.getsize()});
            type = Type{Type::Kind::Basic, Type::BasicType::Int};
        }
    }
    return std::expected<Type, error>(type);
}
std::any astVisitor::visitPostfixExpression(ComplierParser::PostfixExpressionContext* ctx)
{
    // 注意处理多个后缀
    std::function<std::expected<Type, error>(int, int)> func =
        [&func, this, ctx](int start, int end) -> std::expected<Type, error>
    {
        if (end == 0)
        {
            auto str = ctx->children[0]->getText();
            return ac<std::expected<Type, error>>(visitPrimaryExpression(
                dc<ComplierParser::PrimaryExpressionContext*>(ctx->children[0])));
        }
        auto todo = ctx->children[end - 1];
        if (todo->getText() == "(") // func call
        {
            int args_num = 0;
            if (ctx->children[end]->getText() != ")") // 有experionlist
            {
                auto str = ctx->children[end]->getText();
                auto el = ac<std::expected<bool, error>>((visitArgumentExpressionList(
                    dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end]))));
                if (!el)
                {
                    return std::unexpected<error>(el.error());
                }
                args_num = dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end])
                               ->assignmentExpression()
                               .size();
            }
            auto funcaddr = ac<std::expected<Type, error>>(func(0, end - 1)); // 解析函数地址
            if (!funcaddr)
            {
                return std::unexpected<error>(funcaddr.error());
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::CALL});
            Type rettype = funcaddr.value();
            if (funcaddr.value().kind == Type::Kind::Pointer &&
                funcaddr.value().arr_or_ptr_num == 1 &&
                funcaddr.value().subType->kind == Type::Kind::Function) // 函数指针
            {
                rettype = *funcaddr.value().subType->subType;
                // args_num = funcaddr.value().subType->args.size();// 由实参决定以支持可变参
            }
            else if (funcaddr.value().kind == Type::Kind::Function) // 函数
            {
                rettype = *funcaddr.value().subType;
                // args_num = funcaddr.value().args.size();
            }
            else
            {
                return std::unexpected<error>(error::expected_func_or_funcptr);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::DARG, args_num});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
            return std::expected<Type, error>(rettype);
        }
        else if (todo->getText() == "[") // arr[] ptr[]
        {
            auto paret = ac<std::expected<Type, error>>(func(0, end - 1));
            if (!paret)
            {
                return std::unexpected<error>(paret.error());
            }
            Type eleType;
            if (paret.value().kind == Type::Kind::Pointer && paret.value().arr_or_ptr_num == 1)
            {
                eleType = *paret.value().subType;
            }
            else if (paret.value().kind == Type::Kind::Pointer && paret.value().arr_or_ptr_num > 1)
            {
                eleType = paret.value();
                eleType.arr_or_ptr_num--;
            }
            else if (paret.value().kind == Type::Kind::Array)
            {
                eleType = *paret.value().subType;
            }
            auto eret = ac<std::expected<Type, error>>(
                visitExpression(dc<ComplierParser::ExpressionContext*>(ctx->children[end])));
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, eleType.getsize()});
            funcnow->asms.push_back(ASM{ASM::basic_asm::MUL});
            funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
            if (eleType.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (eleType.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (eleType.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
            return std::expected<Type, error>(eleType);
        }
        else
        {
            return func(0, end - 1);
        }
    };
    return func(0, ctx->children.size());
}
std::any astVisitor::visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx)
{
    if (ctx->Identifier())
    {
        auto var = funcnow->lookup_var(ctx->Identifier()->getText());
        if (var) // 函数局部找到
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, var->addr});
            funcnow->asms.push_back(ASM{ASM::basic_asm::LEA});
            if (var->type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (var->type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (var->type.getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
            return std::expected<Type, error>(var->type);
        }
        auto varg = obj.symbol_table.lookup_var_decl(ctx->Identifier()->getText());
        if (varg) // 全局变量
        {
            funcnow->asms.push_back(
                ASM{ASM::basic_asm::IMM, "globalvar@" + ctx->Identifier()->getText()});
            funcnow->asms.push_back(ASM{ASM::basic_asm::LEAD});
            if (varg->getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (varg->getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (varg->getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
            return std::expected<Type, error>(*varg);
        }
        auto str = ctx->Identifier()->getText();
        auto funcret = obj.symbol_table.lookup_func_decl(ctx->Identifier()->getText());
        if (funcret)
        {
            funcnow->asms.push_back(
                ASM{ASM::basic_asm::IMM, "func@" + ctx->Identifier()->getText()});
            return std::expected<Type, error>(*funcret);
        }
        return std::unexpected<error>(error::undifined_id);
    }
    else if (ctx->Constant()) // 常量处理
    {
        std::string text = ctx->Constant()->getText();

        // 处理字符常量
        if (isCharacterConstant(text))
        {
            try
            {
                int value = parseCharacterConstant(text);
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, value});
                return std::expected<Type, error>(Type{Type::Kind::Basic, Type::BasicType::Char});
            }
            catch (...)
            {
                return std::unexpected<error>(error::invalid_constant);
            }
        }
        // 处理整型常量
        else if (isIntegerConstant(text))
        {
            try
            {
                int value = parseIntegerConstant(text);
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, value});
                return std::expected<Type, error>(Type{Type::Kind::Basic, Type::BasicType::Int});
            }
            catch (...)
            {
                return std::unexpected<error>(error::invalid_constant);
            }
        }
        // [TODO] 其他类型的常量（如浮点数）
        else
        {
            return std::unexpected<error>(error::unsurpported_num);
        }
    }
    else if (ctx->expression()) // (expr)
    {
        return visitExpression(ctx->expression());
    }
    else if (ctx->StringLiteral().size())
    {
        return 0;
    }
    else
    {
        auto str = ctx->getText();
        return std::unexpected<error>(error::invalid_constant);
    }
}
std::any astVisitor::visitTypeName(ComplierParser::TypeNameContext* ctx)
{
    // typeName由specifierQualifierList和可选的abstractDeclarator组成
}

std::any astVisitor::visitBlockItem(ComplierParser::BlockItemContext* ctx)
{
    if (ctx->statement())
    {
        return visitStatement(ctx->statement());
    }
    else if (ctx->declaration())
    {
        auto ret =
            ac<std::expected<std::vector<Type>, error>>(visitDeclaration(ctx->declaration()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        for (auto& each : ret.value())
        {
            varDef var;
            var.name = each.id;
            var.type = *each.subType;
            auto varptr = funcnow->add_var(var);
            if (!varptr)
            {
                return std::unexpected<error>(error::double_defined);
            }
        }
    }
    else if (ctx->asmADDer())
    {
        visitAsmADDer(ctx->asmADDer());
    }
    return std::expected<bool, error>(true);
}

std::any astVisitor::visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx)
{
}

std::any astVisitor::visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "if")
    {
        auto eret = ac<std::expected<Type, error>>(visitExpression(ctx->expression()));
        if (!eret)
        {
            return std::unexpected<error>(eret.error());
        }
        int after_expr = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // for jz
        auto fret = ac<std::expected<bool, error>>(visitStatement(ctx->statement()[0]));
        if (!fret)
        {
            return std::unexpected<error>(fret.error());
        }
        if (ctx->statement().size() == 2) // 有else
        {
            int after_id = funcnow->asms.size();
            funcnow->asms.push_back("HOLD"); // for 'if' statments jump through 'else' to end
            funcnow->asms[after_expr] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->asms.size())};

            auto elret = ac<std::expected<bool, error>>(visitStatement(ctx->statement()[1]));
            if (!elret)
            {
                return std::unexpected<error>(elret.error());
            }
            funcnow->asms[after_id] =
                ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(funcnow->asms.size())};
        }
        else // 无else
        {
            funcnow->asms[after_expr] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->asms.size())};
        }
    }
    // [TODO] switch
    return std::expected<bool, error>(true);
}

std::any astVisitor::visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext* ctx)
{
    // 实参从右向左入栈
    for (int i = ctx->assignmentExpression().size() - 1; i >= 0; i--)
    {
        auto ret = ac<std::expected<Type, error>>(
            visitAssignmentExpression(ctx->assignmentExpression()[i]));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<bool, error>(true);
}

std::any astVisitor::visitIterationStatement(ComplierParser::IterationStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "while")
    {
        int startppos = funcnow->asms.size();
        auto eret = ac<std::expected<Type, error>>(visitExpression(ctx->expression()));
        if (!eret)
        {
            return std::unexpected<error>(eret.error());
        }
        int after_expr = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // for jz end
        auto sret = ac<std::expected<bool, error>>(visitStatement(ctx->statement()));
        if (!sret)
        {
            return std::unexpected<error>(sret.error());
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(startppos)});
        funcnow->asms[after_expr] =
            ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->asms.size())};

        for (int i = startppos; i < funcnow->asms.size(); i++)
        {
            if (funcnow->asms[i] == "lable@break")
            {
                funcnow->asms[i] =
                    ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(funcnow->asms.size())};
            }
            else if (funcnow->asms[i] == "lable@continue")
            {
                funcnow->asms[i] = ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(startppos)};
            }
        }
        return std::expected<bool, error>(true);
    }
    // [TODO] 'for' statement
    return std::expected<bool, error>(true);
}

std::any astVisitor::visitJumpStatement(ComplierParser::JumpStatementContext* ctx)
{
    // [TODO] goto statement
    if (ctx->children[0]->getText() == "continue")
    {
        funcnow->asms.push_back("lable@continue");
        return std::expected<bool, error>(true);
    }
    else if (ctx->children[0]->getText() == "break")
    {
        funcnow->asms.push_back("lable@break");
        return std::expected<bool, error>(true);
    }
    else if (ctx->children[0]->getText() == "return")
    {
        if (ctx->expression())
        {
            auto eret = ac<std::expected<Type, error>>(visitExpression(ctx->expression()));
            if (!eret)
            {
                return std::unexpected<error>(eret.error());
            }
        }
        else
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 0});
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::RET});
        return std::expected<bool, error>(true);
    }
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
    // 递归下降求值，仅用于编译期整型常量表达式（如数组维度）
    // 支持：括号、整型/字符常量、单目 + - ~
    // !、*,/,%、+,-、<<,>>、<,<=,>,>=、==,!=、&,^,|、&&,||、?:、逗号表达式
    // 不支持：标识符、函数调用、下标、sizeof/_Alignof、赋值类运算等

    // 前置声明一组局部 lambda，互相递归
    std::function<long long(ComplierParser::PrimaryExpressionContext*)> evalPrimary;
    std::function<long long(ComplierParser::PostfixExpressionContext*)> evalPostfix;
    std::function<long long(ComplierParser::UnaryExpressionContext*)> evalUnary;
    std::function<long long(ComplierParser::CastExpressionContext*)> evalCast;
    std::function<long long(ComplierParser::MultiplicativeExpressionContext*)> evalMul;
    std::function<long long(ComplierParser::AdditiveExpressionContext*)> evalAdd;
    std::function<long long(ComplierParser::ShiftExpressionContext*)> evalShift;
    std::function<long long(ComplierParser::RelationalExpressionContext*)> evalRel;
    std::function<long long(ComplierParser::EqualityExpressionContext*)> evalEq;
    std::function<long long(ComplierParser::AndExpressionContext*)> evalBitAnd;
    std::function<long long(ComplierParser::ExclusiveOrExpressionContext*)> evalBitXor;
    std::function<long long(ComplierParser::InclusiveOrExpressionContext*)> evalBitOr;
    std::function<long long(ComplierParser::LogicalAndExpressionContext*)> evalLogAnd;
    std::function<long long(ComplierParser::LogicalOrExpressionContext*)> evalLogOr;
    std::function<long long(ComplierParser::ExpressionContext*)> evalExpr;
    std::function<long long(ComplierParser::ConditionalExpressionContext*)> evalCond;
    std::function<long long(ComplierParser::AssignmentExpressionContext*)> evalAssign;

    auto truth = [](long long v) -> long long { return v != 0 ? 1LL : 0LL; };

    evalPrimary = [this, &evalExpr](ComplierParser::PrimaryExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->Identifier())
        {
            // 常量表达式不允许普通标识符（尚未实现宏常量等）
            throw error::invalid_constant;
        }
        if (ctx->Constant())
        {
            const std::string t = ctx->Constant()->getText();
            if (isCharacterConstant(t))
                return parseCharacterConstant(t);
            if (isIntegerConstant(t))
                return parseIntegerConstant(t);
            throw error::invalid_constant;
        }
        if (ctx->expression())
        {
            // 括号表达式
            return evalExpr(ctx->expression());
        }
        // 其他扩展（如__builtin等）不支持
        throw error::invalid_constant;
    };

    evalPostfix = [&evalPrimary](ComplierParser::PostfixExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 仅接受“纯 primary”的情况；带 [], (), ., ->, ++/-- 等均不支持
        if (ctx->primaryExpression() && ctx->children.size() == 1)
            return evalPrimary(ctx->primaryExpression());
        // GNU 扩展、聚合初始化等均不在常量表达式支持范围
        throw error::invalid_constant;
    };

    evalCast = [&evalCast, &evalUnary](ComplierParser::CastExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 若为显式类型转换 '(' typeName ')' castExpression -> 忽略类型，直接求右侧值
        if (ctx->castExpression())
            return evalCast(ctx->castExpression());
        // 其余分支：unary 或 DigitSequence（for 场景）
        if (ctx->unaryExpression())
            return evalUnary(ctx->unaryExpression());
        if (ctx->DigitSequence())
        {
            // 纯数字序列，十进制
            return std::stoll(ctx->DigitSequence()->getText(), nullptr, 10);
        }
        throw error::invalid_constant;
    };

    evalUnary = [&evalPostfix, &evalCast](ComplierParser::UnaryExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->postfixExpression())
            return evalPostfix(ctx->postfixExpression());

        // 处理一元运算符：+ - ~ !
        if (ctx->unaryOperator() && ctx->castExpression())
        {
            const std::string op = ctx->unaryOperator()->getText();
            long long v = evalCast(ctx->castExpression());
            if (op == "+")
                return +v;
            if (op == "-")
                return -v;
            if (op == "~")
                return ~v;
            if (op == "!")
                return (v == 0) ? 1LL : 0LL;
            throw error::invalid_constant;
        }

        // sizeof/_Alignof等未实现
        throw error::invalid_constant;
    };

    auto evalBinaryByChildren =
        [](antlr4::ParserRuleContext* ctx, auto evalLhs,
           const std::function<long long(antlr4::tree::ParseTree*)>& evalRhs,
           const std::function<long long(long long, const std::string&, long long)>& apply)
        -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        const auto& kids = ctx->children;
        if (kids.empty())
            throw error::invalid_constant;

        // 第一个子节点是一个子表达式上下文
        long long acc = evalLhs(kids[0]);
        // 之后按 [op, rhs, op, rhs, ...] 交替
        for (size_t i = 1; i + 1 < kids.size(); i += 2)
        {
            std::string op = kids[i]->getText();
            long long rhs = evalRhs(kids[i + 1]);
            acc = apply(acc, op, rhs);
        }
        return acc;
    };

    // 以下 evalXxx 采用 children 轮询的通用策略，避免依赖 ANTLR 生成的 vector API 差异
    evalMul = [&evalBinaryByChildren,
               &evalCast](ComplierParser::MultiplicativeExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalCast](antlr4::tree::ParseTree* n)
        { return evalCast(dynamic_cast<ComplierParser::CastExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "*")
                return a * b;
            if (op == "/")
            {
                if (b == 0)
                    throw error::invalid_constant;
                return a / b;
            }
            if (op == "%")
            {
                if (b == 0)
                    throw error::invalid_constant;
                return a % b;
            }
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalAdd = [&evalBinaryByChildren,
               &evalMul](ComplierParser::AdditiveExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalMul](antlr4::tree::ParseTree* n)
        { return evalMul(dynamic_cast<ComplierParser::MultiplicativeExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "+")
                return a + b;
            if (op == "-")
                return a - b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalShift = [&evalBinaryByChildren,
                 &evalAdd](ComplierParser::ShiftExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalAdd](antlr4::tree::ParseTree* n)
        { return evalAdd(dynamic_cast<ComplierParser::AdditiveExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (b < 0)
                throw error::invalid_constant;
            if (op == "<<")
                return a << b;
            if (op == ">>")
                return a >> b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalRel = [&evalBinaryByChildren, &evalShift,
               &truth](ComplierParser::RelationalExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalShift](antlr4::tree::ParseTree* n)
        { return evalShift(dynamic_cast<ComplierParser::ShiftExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "<")
                return truth(a < b);
            if (op == "<=")
                return truth(a <= b);
            if (op == ">")
                return truth(a > b);
            if (op == ">=")
                return truth(a >= b);
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalEq = [&evalBinaryByChildren, &evalRel,
              &truth](ComplierParser::EqualityExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalRel](antlr4::tree::ParseTree* n)
        { return evalRel(dynamic_cast<ComplierParser::RelationalExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "==")
                return truth(a == b);
            if (op == "!=")
                return truth(a != b);
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitAnd = [&evalBinaryByChildren,
                  &evalEq](ComplierParser::AndExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalEq](antlr4::tree::ParseTree* n)
        { return evalEq(dynamic_cast<ComplierParser::EqualityExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "&")
                return a & b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitXor = [&evalBinaryByChildren,
                  &evalBitAnd](ComplierParser::ExclusiveOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitAnd](antlr4::tree::ParseTree* n)
        { return evalBitAnd(dynamic_cast<ComplierParser::AndExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "^")
                return a ^ b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitOr = [&evalBinaryByChildren,
                 &evalBitXor](ComplierParser::InclusiveOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitXor](antlr4::tree::ParseTree* n)
        { return evalBitXor(dynamic_cast<ComplierParser::ExclusiveOrExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "|")
                return a | b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalLogAnd = [&evalBinaryByChildren, &evalBitOr,
                  &truth](ComplierParser::LogicalAndExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitOr](antlr4::tree::ParseTree* n)
        { return evalBitOr(dynamic_cast<ComplierParser::InclusiveOrExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "&&")
                return truth(truth(a) && truth(b));
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalLogOr = [&evalBinaryByChildren, &evalLogAnd,
                 &truth](ComplierParser::LogicalOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalLogAnd](antlr4::tree::ParseTree* n)
        { return evalLogAnd(dynamic_cast<ComplierParser::LogicalAndExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "||")
                return truth(truth(a) || truth(b));
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalExpr = [this](ComplierParser::ExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 逗号表达式的值为最后一个 assignmentExpression
        long long val = 0;
        for (auto* ae : ctx->assignmentExpression())
        {
            // 递归调用 parseConstexpr 以复用逻辑
            val = this->parseConstexpr(ae);
        }
        return val;
    };

    evalCond = [&evalLogOr, &evalExpr, &evalCond,
                &truth](ComplierParser::ConditionalExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        long long c = evalLogOr(ctx->logicalOrExpression());
        if (ctx->expression() && ctx->conditionalExpression())
        {
            if (truth(c))
                return evalExpr(ctx->expression());
            else
                return evalCond(ctx->conditionalExpression());
        }
        return c;
    };

    evalAssign = [&evalCond](ComplierParser::AssignmentExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->conditionalExpression())
            return evalCond(ctx->conditionalExpression());
        // 赋值类表达式不属于常量表达式
        throw error::invalid_constant;
    };

    return evalAssign(expr);
};
astVisitor::astVisitor(std::string name, OBJ& ob) : obj(ob)
{
    Type global_init_fun;
    global_init_fun.kind = Type::Kind::ID;
    global_init_fun.id = "__global_init" + name;
    global_init_fun.pushTop(Type{Type::Kind::Function, std::vector<Type>()});
    global_init_fun.pushTop(Type{Type::Kind::Basic, Type::BasicType::Void});
    obj.symbol_table.add_global_func_def(global_init_fun);
    funcnow = obj.symbol_table.lookup_func_def("__global_init" + name);
}
std::any astVisitor::visitAsmADDer(ComplierParser::AsmADDerContext* ctx)
{
    auto str = ctx->StringLiteral()->getText();
    std::string text = ctx->StringLiteral()->getText();
    std::string content;
    size_t first_quote = text.find('"');
    size_t last_quote = text.rfind('"');
    if (first_quote != std::string::npos && last_quote != std::string::npos &&
        last_quote > first_quote)
    {
        std::string raw = text.substr(first_quote + 1, last_quote - first_quote - 1);
        content.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i)
        {
            char c = raw[i];
            if (c == '\\' && i + 1 < raw.size())
            {
                char esc = raw[++i];
                if (esc == 'n')
                    content.push_back('\n');
                else if (esc == 't')
                    content.push_back('\t');
                else if (esc == 'r')
                    content.push_back('\r');
                else if (esc == '\\')
                    content.push_back('\\');
                else if (esc == '\'')
                    content.push_back('\'');
                else if (esc == '\"')
                    content.push_back('\"');
                else if (esc == '0')
                    content.push_back('\0');
                else if (esc == 'x') // hex escape: \xhh...
                {
                    int val = 0;
                    int cnt = 0;
                    while (i + 1 < raw.size() &&
                           std::isxdigit(static_cast<unsigned char>(raw[i + 1])) && cnt < 2)
                    {
                        ++i;
                        char hx = raw[i];
                        val = val * 16 +
                              (std::isdigit(static_cast<unsigned char>(hx))
                                   ? hx - '0'
                                   : (std::toupper(static_cast<unsigned char>(hx)) - 'A' + 10));
                        ++cnt;
                    }
                    content.push_back(static_cast<char>(val));
                }
                else if (esc >= '0' && esc <= '7') // octal \nnn
                {
                    int val = esc - '0';
                    int cnt = 1;
                    while (i + 1 < raw.size() && raw[i + 1] >= '0' && raw[i + 1] <= '7' && cnt < 3)
                    {
                        ++i;
                        val = val * 8 + (raw[i] - '0');
                        ++cnt;
                    }
                    content.push_back(static_cast<char>(val));
                }
                else
                {
                    // unknown escape, keep the character itself
                    content.push_back(esc);
                }
            }
            else
            {
                content.push_back(c);
            }
        }
    }
    else
    {
        // Fallback: no surrounding quotes found, use the raw token
        content = text;
    }
    funcnow->asms.push_back(content);
    return {};
}
std::any astVisitor::visitByTypeIndex(antlr4::ParserRuleContext* ctx)
{
    switch (ctx->getRuleIndex())
    {
    case ComplierParser::RuleCompilationUnit:
        return visitCompilationUnit(
            dynamic_cast<ComplierParser::ComplierParser::CompilationUnitContext*>(ctx));
        break;
    case ComplierParser::RuleTranslationUnit:
        return visitTranslationUnit(
            dynamic_cast<ComplierParser::ComplierParser::TranslationUnitContext*>(ctx));
        break;
    case ComplierParser::RuleExternalDeclaration:
        return visitExternalDeclaration(
            dynamic_cast<ComplierParser::ExternalDeclarationContext*>(ctx));
        break;
    case ComplierParser::RuleDeclaration:
        return visitDeclaration(
            dynamic_cast<ComplierParser::ComplierParser::DeclarationContext*>(ctx));
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
// 处理整型常量，支持十进制、十六进制、八进制格式
int astVisitor::parseIntegerConstant(const std::string& text)
{
    try
    {
        // 检查十六进制格式 (0x 或 0X 前缀)
        if (text.size() > 2 && (text.substr(0, 2) == "0x" || text.substr(0, 2) == "0X"))
        {
            return std::stoi(text, nullptr, 16);
        }
        // 检查八进制格式 (0 前缀)
        else if (text.size() > 0 && text[0] == '0')
        {
            return std::stoi(text, nullptr, 8);
        }
        // 默认十进制
        else
        {
            return std::stoi(text, nullptr, 10);
        }
    }
    catch (const std::exception& e)
    {
        throw error::invalid_constant;
    }
}

// 处理字符常量，支持转义序列
int astVisitor::parseCharacterConstant(const std::string& text)
{
    // 移除前后的单引号
    std::string content = text.substr(1, text.size() - 2);

    // 处理转义序列
    if (content.size() > 0 && content[0] == '\\')
    {
        if (content.size() < 2)
        {
            throw error::invalid_constant;
        }

        switch (content[1])
        {
        case 'n':
            return '\n'; // 换行
        case 't':
            return '\t'; // 制表符
        case 'r':
            return '\r'; // 回车
        case '0':
            return '\0'; // 空字符
        case '\\':
            return '\\'; // 反斜杠
        case '\'':
            return '\''; // 单引号
        case '\"':
            return '\"'; // 双引号
        case 'x':
        { // 十六进制表示 \xhh
            if (content.size() < 4)
            {
                throw error::invalid_constant;
            }
            std::string hexValue = content.substr(2, 2);
            try
            {
                return std::stoi(hexValue, nullptr, 16);
            }
            catch (...)
            {
                throw error::invalid_constant;
            }
        }
        default:
            throw error::invalid_constant;
        }
    }
    else if (content.size() == 1)
    {
        // 普通字符
        return static_cast<int>(content[0]);
    }

    throw error::invalid_constant;
}

// 检查是否为整型常量
bool astVisitor::isIntegerConstant(const std::string& text)
{
    // 判断首字符是否为数字或负号
    if (text.empty() || (text[0] != '-' && text[0] != '+' && !std::isdigit(text[0])))
    {
        return false;
    }

    // 检查是否包含单引号（字符常量特征）
    if (text[0] == '\'' || text.find('\'') != std::string::npos)
    {
        return false;
    }

    return true;
}

// 检查是否为字符常量
bool astVisitor::isCharacterConstant(const std::string& text)
{
    // 字符常量格式: 'x' 或 '\x'
    return text.size() >= 3 && text[0] == '\'' && text[text.size() - 1] == '\'';
}