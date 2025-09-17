#include "astVisit.h"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include <ASM.hpp>
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
    funcnow = funnowptr;
    auto compoundRet =
        ac<std::expected<bool, error>>(visitCompoundStatement(ctx->compoundStatement()));
    if (!compoundRet)
    {
        funcnow = nullptr;
        return std::unexpected<error>(compoundRet.error());
    }
    else
    {
        funcnow = nullptr;
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
std::any astVisitor::visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx)
{
    if (auto ptr = ctx->blockItemList())
    {
        return visitBlockItemList(ctx->blockItemList());
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
    else if (ctx->labeledStatement())
    {
        return visitLabeledStatement(ctx->labeledStatement());
    }
}
std::any astVisitor::visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx)
{
    if (ctx->expression())
    {
        auto ret = ac<std::expected<bool, error>>(visitExpression(ctx->expression()));
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
        if (funcnow->asms.back() != "LC" && funcnow->asms.back() != "LI") // 不是左值
        {
            return std::unexpected<error>(error::expected_lvalue);
        }
        funcnow->asms.pop_back();
        if (ctx->assignmentOperator()->getText() == "=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::MOVE, "stack", "ax"});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH}); // 拷贝一份左值地址实现返回值
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
        }
        else
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::MOVE, "stack", "ax"});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
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
        }
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
        func = [&func, this](std::span<ComplierParser::AndExpressionContext*> in)
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
                                  int index)
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
        func = [&func, this, ctx](std::span<ComplierParser::ShiftExpressionContext*> in, int index)
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
        func =
            [&func, this, ctx](std::span<ComplierParser::AdditiveExpressionContext*> in, int index)
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
                                  int index)
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
        func = [&func, this, ctx](std::span<ComplierParser::CastExpressionContext*> in, int index)
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
            if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI")
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
        else if (ctx->unaryOperator()->getText() == "*")
        {
            auto cret = ac<std::expected<Type, error>>(visitCastExpression(ctx->castExpression()));
            if (!cret)
            {
                return std::unexpected<error>(cret.error());
            }
            type = cret.value();
            if (cret.value().getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (cret.value().getsize() ==
                     Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            type = Type{Type::Kind::Basic, Type::BasicType::Int};
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
            if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI")
            {
                funcnow->asms.pop_back();
            }
            else
            {
                return std::unexpected<error>(error::expected_lvalue);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::MOVE, "stack", "ax"});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
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
        }
        else if (ctx->children[i]->getText() == "--")
        {
            if (funcnow->asms.back() == "LC" || funcnow->asms.back() == "LI")
            {
                funcnow->asms.pop_back();
            }
            else
            {
                return std::unexpected<error>(error::expected_lvalue);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::MOVE, "stack", "ax"});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
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
        auto todo = ctx->children[end - 1];
        if (todo->getText() == "(") // func call
        {
            if (ctx->children[end]->getText() != ")") // 有experionlist
            {
                auto el = ac<std::expected<bool, error>>((visitArgumentExpressionList(
                    dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end]))));
                if (!el)
                {
                    return std::unexpected<error>(el.error());
                }
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
            }
            else if (funcaddr.value().kind == Type::Kind::Function) // 函数
            {
                rettype = *funcaddr.value().subType;
            }
            else
            {
                return std::unexpected<error>(error::expected_func_or_funcptr);
            }
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
            return std::expected<Type, error>(eleType);
        }
        else if (end == 0)
        {
            return ac<std::expected<Type, error>>(visitPrimaryExpression(
                dc<ComplierParser::PrimaryExpressionContext*>(ctx->children[0])));
        }
    };
    return func(0, ctx->children.size());
}
std::any astVisitor::visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx)
{
    return std::any();
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
    return std::expected<bool, error>(true);
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
    Type global_init_fun;
    global_init_fun.kind = Type::Kind::ID;
    global_init_fun.id = "__global_init" + name;
    global_init_fun.pushTop(Type{Type::Kind::Function, std::vector<Type>()});
    global_init_fun.pushTop(Type{Type::Kind::Basic, Type::BasicType::Void});
    obj.symbol_table.add_global_func_decl(global_init_fun);
    obj.symbol_table.add_global_func_def(global_init_fun);
    globalinitfun = obj.symbol_table.lookup_func_def("__global_init" + name);
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