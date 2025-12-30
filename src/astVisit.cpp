#include "astVisit.h"
#include "ASM.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "error.hpp"
#include "type_utils.hpp"
#include "vm.h"
#include <functional>
#include <tree/TerminalNode.h>
#include <utility>
template <class T, class IN>
    requires requires(IN in) { dynamic_cast<T>(in); }
auto dc(IN&& in)
{
    return dynamic_cast<T>(in);
}
void astVisitor::visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx)
{
    // ctx->getRuleIndex() == ComplierParser::RuleCompilationUnit;
    if (!ctx)
        return;
    auto tu = ctx->translationUnit();
    if (!tu)
        return;
    return visitTranslationUnit(tu);
}
void astVisitor::visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx)
{
    if (!ctx)
        return;
    for (auto each : ctx->externalDeclaration())
    {
        if (!each)
            continue;
        visitExternalDeclaration(each);
    }
}
std::vector<Type> astVisitor::visitDeclaration(ComplierParser::DeclarationContext* ctx)
{
    Type basetype;
    std::vector<Type> vars;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto declarationSpecifiers = visitDeclarationSpecifiers(ctx->declarationSpecifiers());
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
                            THROW_ERR(error::double_type, each->typeSpecifier());
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    THROW_ERR(error::double_type, each->typeSpecifier());
                }
                basetype = (visitTypeSpecifier(each->typeSpecifier()));
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    if (ctx->initDeclaratorList()) // 带初始化的参数
    {
        baseType = basetype;
        visitInitDeclaratorList(ctx->initDeclaratorList());
    }
    for (auto& each : vars)
    {
        each.pushTop(basetype);
    }
    return vars;
    // [TODO] 处理初始化器
    // 如果有初始化器，需要处理 ctx->initDeclaratorList()
}
void astVisitor::visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx)
{
    Type basetype;
    basetype = (visitDeclarator(ctx->declarator()));
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto declarationSpecifiers = visitDeclarationSpecifiers(ctx->declarationSpecifiers());
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
                            THROW_ERR(error::double_type, each->typeSpecifier());
                        }
                        basetype.pushTop(obj.typedefs.find(id)->second);
                        flag = true;
                    }
                }
                if (flag)
                {
                    THROW_ERR(error::double_type, each->typeSpecifier());
                }
                basetype.pushTop((visitTypeSpecifier(each->typeSpecifier())));
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    auto funnowptr = obj.symbol_table.add_global_func_def(basetype);
    if (!funnowptr)
    {
        // THROW_ERR(error::double_defined, ctx->declarator());
        // 先忽略func_double define error 防止头文件多次包含
        funnowptr = obj.symbol_table.lookup_func_def(basetype.id);
    }
    auto gfunptr = funcnow;
    funcnow = funnowptr;
    funcnow->asms.push_back("HOLD");
    visitCompoundStatement(ctx->compoundStatement());
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
}
void astVisitor::visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx)
{
    // 检查是否为函数定义
    if (ctx->functionDefinition())
    {
        // 在visitFunctionDefinition内部处理
        visitFunctionDefinition(ctx->functionDefinition());
        return;
    }

    // 检查是否为变量定义
    // [TODO] 区分变量定义与声明
    else if (ctx->declaration())
    {
        auto vars = visitDeclaration(ctx->declaration());
        for (auto& each : vars)
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
        return;
    }
    return;
}
std::vector<Type> astVisitor::visitDeclarationSpecifiers(
    ComplierParser::DeclarationSpecifiersContext* ctx)
{
    std::vector<Type> Types;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        Type result = visitDeclarationSpecifier(each);
        Types.push_back((result));
    }
    return Types;
}
Type astVisitor::visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx)
{
    // 检查是否为类型说明符
    if (ctx->typeSpecifier())
    {
        // 直接返回typeSpecifier的结果
        return visitTypeSpecifier(ctx->typeSpecifier());
    }
    THROW_ERR(error::unsurpport_basictype, ctx);
}
Type astVisitor::visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx)
{
    Type rettype;
    // varDef rettpe;
    if (ctx->typedefName()) // 语法阶段无法判断是类型别名还是id, 均以typedefName表示
    {
        if (obj.typedefs.find(ctx->typedefName()->getText()) != obj.typedefs.end()) // 是类型别名
        {
            rettype = obj.typedefs.find(ctx->typedefName()->getText())->second;
            return rettype;
        }
        else // 是id
        {
            rettype.id = ctx->typedefName()->getText();
            rettype.kind = Type::Kind::ID;
            return rettype;
        }
    }
    // 判断基本类型
    rettype.kind = Type::Kind::Basic;
    if (ctx->getText() == "int")
    {
        rettype.basic_type = Type::BasicType::Int;
        return rettype;
    }
    else if (ctx->getText() == "char")
    {
        rettype.basic_type = Type::BasicType::Char;
        return rettype;
    }
    else if (ctx->getText() == "void")
    {
        rettype.basic_type = Type::BasicType::Void;
        return rettype;
    }
    else if (ctx->getText() == "float")
    {
        rettype.basic_type = Type::BasicType::Float;
        return rettype;
    }
    else if (ctx->getText() == "double")
    {
        rettype.basic_type = Type::BasicType::Double;
        return rettype;
    }
    else if (ctx->getText() == "long")
    {
        rettype.basic_type = Type::BasicType::Long;
        return rettype;
    }
    else if (ctx->getText() == "short")
    {
        rettype.basic_type = Type::BasicType::Short;
        return rettype;
    }
    else if (ctx->getText() == "unsigned")
    {
        rettype.basic_type = Type::BasicType::Unsigned;
        return rettype;
    }
    else if (ctx->getText() == "signed")
    {
        rettype.basic_type = Type::BasicType::Signed;
        return rettype;
    }
    THROW_ERR(error::unsurpport_basictype, ctx);
}
std::vector<Type> astVisitor::visitInitDeclaratorList(
    ComplierParser::InitDeclaratorListContext* ctx)
{
    std::vector<Type> vars;
    for (auto& each : ctx->initDeclarator())
    {
        vars.push_back((visitInitDeclarator(each)));
    }
    return vars;
}
Type astVisitor::visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx)
{
    auto ret = (visitDeclarator(ctx->declarator()));
    ret.pushTop(baseType);
    if (funcnow->name != "__global_init" + obj.name) // 局部
    {
        varDef var;
        var.type = *ret.subType;
        var.name = ret.id;
        funcnow->add_var(var);
    }
    else
    {
        obj.symbol_table.add_global_var_def(ret);
    }
    auto decodeStringLiteral =
        [this](ComplierParser::AssignmentExpressionContext* expr) -> std::optional<std::vector<int>>
    {
        if (!expr)
        {
            return std::nullopt;
        }
        std::string text;
        try
        {
            text = expr->getText();
        }
        catch (...)
        {
            return std::nullopt;
        }
        if (text.empty())
        {
            return std::nullopt;
        }
        auto hexValue = [](char ch) -> int
        {
            if (ch >= '0' && ch <= '9')
            {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f')
            {
                return ch - 'a' + 10;
            }
            if (ch >= 'A' && ch <= 'F')
            {
                return ch - 'A' + 10;
            }
            return -1;
        };
        size_t pos = 0;
        std::vector<int> result;
        while (pos < text.size())
        {
            if (text.compare(pos, 2, "u8") == 0 && pos + 2 < text.size() && text[pos + 2] == '"')
            {
                pos += 2;
            }
            else if ((text[pos] == 'u' || text[pos] == 'U' || text[pos] == 'L') &&
                     pos + 1 < text.size() && text[pos + 1] == '"')
            {
                pos += 1;
            }
            if (pos >= text.size() || text[pos] != '"')
            {
                return std::nullopt;
            }
            pos++;
            while (pos < text.size() && text[pos] != '"')
            {
                if (text[pos] == '\\')
                {
                    pos++;
                    if (pos >= text.size())
                    {
                        return std::nullopt;
                    }
                    char esc = text[pos];
                    switch (esc)
                    {
                    case 'n':
                        result.push_back('\n');
                        pos++;
                        break;
                    case 't':
                        result.push_back('\t');
                        pos++;
                        break;
                    case 'r':
                        result.push_back('\r');
                        pos++;
                        break;
                    case 'a':
                        result.push_back('\a');
                        pos++;
                        break;
                    case 'b':
                        result.push_back('\b');
                        pos++;
                        break;
                    case 'f':
                        result.push_back('\f');
                        pos++;
                        break;
                    case 'v':
                        result.push_back('\v');
                        pos++;
                        break;
                    case '\\':
                        result.push_back('\\');
                        pos++;
                        break;
                    case '\'':
                        result.push_back('\'');
                        pos++;
                        break;
                    case '"':
                        result.push_back('"');
                        pos++;
                        break;
                    case 'x':
                    {
                        pos++;
                        int value = 0;
                        bool hasDigit = false;
                        while (pos < text.size())
                        {
                            int hv = hexValue(text[pos]);
                            if (hv < 0)
                            {
                                break;
                            }
                            hasDigit = true;
                            value = (value << 4) + hv;
                            pos++;
                        }
                        if (!hasDigit)
                        {
                            return std::nullopt;
                        }
                        result.push_back(value & 0xFF);
                        break;
                    }
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    {
                        int value = esc - '0';
                        pos++;
                        int count = 1;
                        while (count < 3 && pos < text.size() && text[pos] >= '0' &&
                               text[pos] <= '7')
                        {
                            value = value * 8 + (text[pos] - '0');
                            pos++;
                            count++;
                        }
                        result.push_back(value & 0xFF);
                        break;
                    }
                    default:
                        result.push_back(static_cast<unsigned char>(esc));
                        pos++;
                        break;
                    }
                }
                else
                {
                    result.push_back(static_cast<unsigned char>(text[pos]));
                    pos++;
                }
            }
            if (pos >= text.size())
            {
                return std::nullopt;
            }
            pos++;
        }
        result.push_back(0);
        return result;
    };
    std::function<bool(Type, ComplierParser::InitializerContext*)> func =
        [&func, this, &decodeStringLiteral](Type arg,
                                            ComplierParser::InitializerContext* init) -> bool
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
                (void)(visitAssignmentExpression(init->assignmentExpression()));
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
            if (init->initializerList())
            {
                size_t initListSize = init->initializerList()->initializer().size();
                size_t limit = std::min(static_cast<size_t>(arg.arr_or_ptr_num), initListSize);
                if (limit == 0)
                {
                }
                for (size_t i = 0; i + 1 < limit; i++)
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::COPY});
                }
                for (size_t i = 0; i < limit; i++)
                {
                    if (i != 0)
                    {
                        asmholder->asms.push_back(
                            ASM{ASM::basic_asm::IMM, arg.subType->getsize() * i});
                        asmholder->asms.push_back(ASM{ASM::basic_asm::ADD});
                    }
                    if (init->initializerList()->initializer(i))
                    {
                        func(*arg.subType, init->initializerList()->initializer(i));
                    }
                }
            }
            else if (auto assign = init->assignmentExpression())
            {
                auto literal = decodeStringLiteral(assign);
                if (!literal)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                if (!arg.subType || arg.subType->kind != Type::Kind::Basic ||
                    arg.subType->basic_type != Type::BasicType::Char)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                if (arg.arr_or_ptr_num < 0)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                auto data = *literal;
                size_t arrLen = static_cast<size_t>(arg.arr_or_ptr_num);
                if (data.size() > arrLen)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                data.resize(arrLen, 0);
                if (arrLen == 0)
                {
                }
                for (size_t i = 0; i + 1 < arrLen; ++i)
                {
                    asmholder->asms.push_back(ASM{ASM::basic_asm::COPY});
                }
                size_t elemSize = arg.subType->getsize();
                Type charType{Type::Kind::Basic, Type::BasicType::Char};
                size_t charSize = charType.getsize();
                size_t intSize = Type{Type::Kind::Basic, Type::BasicType::Int}.getsize();
                size_t longSize = Type{Type::Kind::Basic, Type::BasicType::Long}.getsize();
                for (size_t i = 0; i < arrLen; ++i)
                {
                    if (i != 0)
                    {
                        asmholder->asms.push_back(
                            ASM{ASM::basic_asm::IMM, static_cast<int>(elemSize * i)});
                        asmholder->asms.push_back(ASM{ASM::basic_asm::ADD});
                    }
                    asmholder->asms.push_back(
                        ASM{ASM::basic_asm::IMM,
                            static_cast<int>(static_cast<unsigned char>(data[i]))});
                    if (elemSize == charSize)
                    {
                        asmholder->asms.push_back(ASM{ASM::basic_asm::SC});
                    }
                    else if (elemSize == intSize)
                    {
                        asmholder->asms.push_back(ASM{ASM::basic_asm::SI});
                    }
                    else if (elemSize == longSize)
                    {
                        asmholder->asms.push_back(ASM{ASM::basic_asm::SW});
                    }
                    else
                    {
                        THROW_ERR(error::expected_arr_initor, assign);
                    }
                }
            }
            else
            {
                THROW_ERR(error::expected_arr_initor, init);
            }
        }
        return false;
    };
    if (ctx->initializer())
    {
        func(ret, ctx->initializer());
    }
    return ret;
    throw;
}
// 后序递归生成
Type astVisitor::visitDeclarator(ComplierParser::DeclaratorContext* ctx)
{
    Type var = (visitDirectDeclarator(ctx->directDeclarator()));
    if (ctx->pointer()) // 有ptr
    {
        auto str = ctx->pointer()->getText();
        auto tp =
            Type{Type::Kind::Pointer, static_cast<int>(std::count(str.begin(), str.end(), '*'))};
        var.pushTop(tp);
    }
    return var;
}
// [REWRITE] ctx->getAltNumber()不是分支标识
Type astVisitor::visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx)
{
    Type var;
    if (ctx->Identifier() && ctx->DigitSequence()) // 位域
    {
        return var;
    }
    else if (ctx->Identifier()) // 变量名
    {
        var.kind = Type::Kind::ID;
        var.id = ctx->Identifier()->getText();
        return var;
    }
    else if (ctx->LeftParen() && ctx->directDeclarator()) // 函数声明
    {
        var = (visitDirectDeclarator(ctx->directDeclarator()));
        std::vector<Type> args;
        if (ctx->parameterTypeList())
        {
            args = visitParameterTypeList(ctx->parameterTypeList());
        }
        var.pushTop(Type{Type::Kind::Function, args});
        return var;
    }
    else if (ctx->directDeclarator() && ctx->LeftBracket() &&
             ctx->assignmentExpression()) // base+ 数组
    {
        var = (visitDirectDeclarator(ctx->directDeclarator()));
        // parseConstexpr 返回 long long, 这里数组维度内部使用 int, 显式窄化避免警告
        var.pushTop(
            Type(Type::Kind::Array, static_cast<int>(parseConstexpr(ctx->assignmentExpression()))));
        return var;
    }
    else if (ctx->declarator()) // (dec)
    {
        return visitDeclarator(ctx->declarator());
    }
    THROW_ERR(error::unsurpport_directDeclarator, ctx);
}
std::vector<Type> astVisitor::visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx)
{
    return visitParameterList(ctx->parameterList());
}
void astVisitor::visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx)
{
    if (auto ptr = ctx->blockItemList())
    {
        funcnow->enter_scope();
        visitBlockItemList(ctx->blockItemList());
        funcnow->exit_scope();
    }
}
std::vector<Type> astVisitor::visitParameterList(ComplierParser::ParameterListContext* ctx)
{
    std::vector<Type> vars;
    for (auto& each : ctx->parameterDeclaration())
    {
        vars.push_back((visitParameterDeclaration(each)));
    }
    return vars;
}
Type astVisitor::visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx)
{
    Type basetype;
    Type vars;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto declarationSpecifiers = visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        bool flag = false;

        if (declarationSpecifiers.back().id != "") // 可能是typedef或变量名
        {
            auto id = declarationSpecifiers.back().id;
            if (obj.typedefs.find(id) ==
                obj.typedefs.end()) // 不是typedef,是id(在函数参数申明中应该不会发生)
            {
                THROW_ERR(error::undifined_type, ctx->declarationSpecifiers());
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
                            THROW_ERR(error::double_type, each->typeSpecifier());
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    THROW_ERR(error::double_type, each->typeSpecifier());
                }
                basetype = (visitTypeSpecifier(each->typeSpecifier()));
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    else if (ctx->declarationSpecifiers2()) // 与declarationSpecifiers一致
    {
        auto declarationSpecifiers = visitDeclarationSpecifiers2(ctx->declarationSpecifiers2());
        bool flag = false;

        if (declarationSpecifiers.back().id != "") // 可能是typedef或变量名
        {
            auto id = declarationSpecifiers.back().id;
            if (obj.typedefs.find(id) ==
                obj.typedefs.end()) // 不是typedef,是id(在函数参数申明中应该不会发生)
            {
                THROW_ERR(error::undifined_type, ctx->declarationSpecifiers2());
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
                            THROW_ERR(error::double_type, each->typeSpecifier());
                        }
                        basetype = obj.typedefs.find(id)->second;
                        flag = true;
                    }
                }
                if (flag)
                {
                    THROW_ERR(error::double_type, each->typeSpecifier());
                }
                basetype = (visitTypeSpecifier(each->typeSpecifier()));
                flag = true;

            } // [TODO] 考虑修饰符
        }
    }
    if (ctx->declarator()) // 数组/函数/指针的组合s
    {
        vars = (visitDeclarator(ctx->declarator()));
        vars.pushTop(basetype);
        return vars;
    }
    else if (ctx->abstractDeclarator())
    {
        vars = (visitAbstractDeclarator(ctx->abstractDeclarator()));
        vars.pushTop(basetype);
        return vars;
    }
    else
    {
        return basetype;
    }
    return vars;
}
void astVisitor::visitBlockItemList(ComplierParser::BlockItemListContext* ctx)
{
    for (auto each : ctx->blockItem())
    {
        visitBlockItem(each);
    }
}
void astVisitor::visitStatement(ComplierParser::StatementContext* ctx)
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
    // else if (ctx->labeledStatement())
    // {
    //     return visitLabeledStatement(ctx->labeledStatement());
    // }
}
void astVisitor::visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx)
{
    if (ctx->expression())
    {
        (void)(visitExpression(ctx->expression()));
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
    }
}
Type astVisitor::visitExpression(ComplierParser::ExpressionContext* ctx)
{
    for (int i = 0; i < ctx->assignmentExpression().size(); i++)
    {
        auto ret = (visitAssignmentExpression(ctx->assignmentExpression()[i]));
        if (i != ctx->assignmentExpression().size() - 1)
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        }
        else
        {
            return ret;
        }
    }
    throw;
}
Type astVisitor::visitAssignmentExpression(ComplierParser::AssignmentExpressionContext* ctx)
{
    // [TODO] DigitSequence
    if (ctx->conditionalExpression())
    {
        return visitConditionalExpression(ctx->conditionalExpression());
    }
    else if (ctx->assignmentOperator())
    {
        auto uret = (visitUnaryExpression(ctx->unaryExpression()));
        if (funcnow->asms.back() != "LC" && funcnow->asms.back() != "LI" &&
            funcnow->asms.back() != "LW") // 不是左值
        {
            THROW_ERR(error::expected_lvalue, ctx->unaryExpression());
        }
        funcnow->asms.pop_back();
        if (ctx->assignmentOperator()->getText() == "=")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY}); // 拷贝一份左值地址实现返回值
            (void)(visitAssignmentExpression(ctx->assignmentExpression()));
            if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
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
            if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            (void)(visitAssignmentExpression(ctx->assignmentExpression()));

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

            if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SC});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LC});
            }
            else if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SI});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LI});
            }
            else if (uret.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
            }
        }
        return uret;
    }
    throw;
}
Type astVisitor::visitConditionalExpression(ComplierParser::ConditionalExpressionContext* ctx)
{
    if (ctx->logicalOrExpression())
    {
        auto lret = (visitLogicalOrExpression(ctx->logicalOrExpression()));
        return lret;
    }
    if (ctx->expression() && ctx->conditionalExpression())
    {
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD");
        (void)(visitExpression(ctx->expression()));
        funcnow->asms[pos] = ASM{ASM::basic_asm::JZ, funcnow->asms.size() + 1}; // 跳过JMP
        int pos2 = funcnow->asms.size();
        funcnow->asms.push_back("HOLD");
        (void)(visitConditionalExpression(ctx->conditionalExpression()));
        funcnow->asms[pos2] = ASM{ASM::basic_asm::JMP, funcnow->asms.size()};
        return Type{Type::Kind::Basic, Type::BasicType::Char};
    }
    throw;
}
std::vector<Type> astVisitor::visitDeclarationSpecifiers2(
    ComplierParser::DeclarationSpecifiers2Context* ctx)
{
    std::vector<Type> Types;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        auto result = visitDeclarationSpecifier(each);
        Types.push_back((result));
    }
    return Types;
}
Type astVisitor::visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext* ctx)
{
    // 注意处理多个||
    std::function<Type(std::span<ComplierParser::LogicalAndExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::LogicalAndExpressionContext*> in) -> Type
    {
        if (in.size() == 1)
        {
            return (visitLogicalAndExpression(in[0]));
        }
        auto lret = (visitLogicalAndExpression(in[0]));
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->asms[pos] = ASM{ASM::basic_asm::JNZ, funcnow->asms.size()};
        // 返回计算结果类型
        return rret; // 语义上应该是整型，保持右值类型沿用
    };
    return func(std::span<ComplierParser::LogicalAndExpressionContext*>(
        ctx->logicalAndExpression().data(), ctx->logicalAndExpression().size()));
}
Type astVisitor::visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext* ctx)
{
    // 注意处理多个&&
    std::function<Type(std::span<ComplierParser::InclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::InclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitInclusiveOrExpression(in[0]));
        }
        auto lret = (visitInclusiveOrExpression(in[0]));
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->asms[pos] = ASM{ASM::basic_asm::JZ, funcnow->asms.size()};
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::InclusiveOrExpressionContext*>(
        ctx->inclusiveOrExpression().data(), ctx->inclusiveOrExpression().size()));
}
Type astVisitor::visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext* ctx)
{
    // 注意处理多个|
    std::function<Type(std::span<ComplierParser::ExclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::ExclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitExclusiveOrExpression(in[0]));
        }
        auto lret = (visitExclusiveOrExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->asms.push_back(ASM{ASM::basic_asm::OR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::ExclusiveOrExpressionContext*>(
        ctx->exclusiveOrExpression().data(), ctx->exclusiveOrExpression().size()));
}
Type astVisitor::visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext* ctx)
{
    // 注意处理多个^
    std::function<Type(std::span<ComplierParser::AndExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::AndExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitAndExpression(in[0]));
        }
        auto lret = (visitAndExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::AndExpressionContext*>(ctx->andExpression().data(),
                                                                 ctx->andExpression().size()));
}
Type astVisitor::visitAndExpression(ComplierParser::AndExpressionContext* ctx)
{
    // 注意处理多个&
    std::function<Type(std::span<ComplierParser::EqualityExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::EqualityExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitEqualityExpression(in[0]));
        }
        auto lret = (visitEqualityExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::EqualityExpressionContext*>(
        ctx->equalityExpression().data(), ctx->equalityExpression().size()));
}
Type astVisitor::visitEqualityExpression(ComplierParser::EqualityExpressionContext* ctx)
{
    // 注意处理多个!= / ==
    std::function<Type(std::span<ComplierParser::RelationalExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::RelationalExpressionContext*> in,
                           int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitRelationalExpression(in[0]));
        }
        auto lret = (visitRelationalExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
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
Type astVisitor::visitRelationalExpression(ComplierParser::RelationalExpressionContext* ctx)
{
    // 注意处理多个< > <= >=
    std::function<Type(std::span<ComplierParser::ShiftExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::ShiftExpressionContext*> in, int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitShiftExpression(in[0]));
        }
        auto lret = (visitShiftExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
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
Type astVisitor::visitShiftExpression(ComplierParser::ShiftExpressionContext* ctx)
{
    // 注意处理多个<< >>
    std::function<Type(std::span<ComplierParser::AdditiveExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::AdditiveExpressionContext*> in,
                           int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitAdditiveExpression(in[0]));
        }
        auto lret = (visitAdditiveExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
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
Type astVisitor::visitAdditiveExpression(ComplierParser::AdditiveExpressionContext* ctx)
{
    std::function<Type(std::span<ComplierParser::MultiplicativeExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::MultiplicativeExpressionContext*> in,
                           int index) -> Type
    {
        if (in.size() == 1)
        {
            return (visitMultiplicativeExpression(in[0]));
        }
        auto posnow = funcnow->asms.size();
        auto lret = (visitMultiplicativeExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        funcnow->asms.resize(posnow); // 之前只是为了拿到类型
        if (lret.kind == Type::Kind::Pointer && rret.kind == Type::Kind::Basic &&
            (rret.basic_type == Type::BasicType::Int || rret.basic_type == Type::BasicType::Char ||
             rret.basic_type == Type::BasicType::Long))
        {
            // 左指针右整形
            auto lret = (visitMultiplicativeExpression(in[0]));
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, lret.subType->getsize()});
            funcnow->asms.push_back(ASM{ASM::basic_asm::MUL});
        }
        else if (rret.kind == Type::Kind::Pointer && lret.kind == Type::Kind::Basic &&
                 (lret.basic_type == Type::BasicType::Int ||
                  lret.basic_type == Type::BasicType::Char ||
                  lret.basic_type == Type::BasicType::Long))
        {
            // 左整形右指针
            auto lret = (visitMultiplicativeExpression(in[0]));
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, rret.subType->getsize()});
            funcnow->asms.push_back(ASM{ASM::basic_asm::MUL});
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        else // 其他类型不做特殊处理
        {
            auto lret = (visitMultiplicativeExpression(in[0]));
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        std::string op_token = ctx->children[index * 2 + 1]->getText();
        if (op_token == "+")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
            auto res = deduce_binary_type(lret, rret, BinOp::Add);
            return res;
        }
        else if (op_token == "-")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
            auto res = deduce_binary_type(lret, rret, BinOp::Sub);
            return res;
        }
        THROW_ERR(error::unsurpported_op, ctx);
    };
    return func(std::span<ComplierParser::MultiplicativeExpressionContext*>(
                    ctx->multiplicativeExpression().data(), ctx->multiplicativeExpression().size()),
                0);
}
Type astVisitor::visitMultiplicativeExpression(ComplierParser::MultiplicativeExpressionContext* ctx)
{
    // 注意处理多个* / %
    std::function<Type(std::span<ComplierParser::CastExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::CastExpressionContext*> in, int index) -> Type
    {
        if (in.size() == 1)
        {
            return (visitCastExpression(in[0]));
        }
        auto lret = (visitCastExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
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
Type astVisitor::visitCastExpression(ComplierParser::CastExpressionContext* ctx)
{
    if (ctx->castExpression())
    {
        auto cret = (visitCastExpression(ctx->castExpression()));
        // [TODO] visitTypeName
        auto tret = (visitTypeName(ctx->typeName()));
        return tret;
    }
    else if (ctx->unaryExpression())
    {
        return visitUnaryExpression(ctx->unaryExpression());
    }
    //[TODO] DigitSequence 何意义?
    throw;
}

Type astVisitor::visitUnaryExpression(ComplierParser::UnaryExpressionContext* ctx)
{
    Type type;
    if (ctx->postfixExpression())
    {
        auto pret = (visitPostfixExpression(ctx->postfixExpression()));
        type = pret;
    }
    else if (ctx->unaryOperator())
    {
        if (ctx->unaryOperator()->getText() == "&")
        {
            auto cret = (visitCastExpression(ctx->castExpression()));
            if (cret.kind == Type::Kind::Function ||
                cret.kind == Type::Kind::Array) // arr/function无LC/LI/LW,取地址与值相同，无需处理
            {
                type = Type{Type::Kind::Pointer, 1};
                type.pushTop(cret);
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
                    THROW_ERR(error::expected_lvalue, ctx->castExpression());
                }
                if (cret.kind == Type::Kind::Pointer)
                {
                    type = cret;
                    type.arr_or_ptr_num++;
                }
                else
                {
                    type = Type{Type::Kind::Pointer, 1};
                    type.pushTop(cret);
                }
            }
        }
        else if (ctx->unaryOperator()->getText() == "*")
        {
            auto cret = (visitCastExpression(ctx->castExpression()));
            if (cret.kind != Type::Kind::Pointer)
            {
                THROW_ERR(error::expected_ptr, ctx->castExpression());
            }
            type = *cret.subType;
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
            auto cret = (visitCastExpression(ctx->castExpression()));
            type = cret;
        }
        else if (ctx->unaryOperator()->getText() == "-") //-12
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            auto cret = (visitCastExpression(ctx->castExpression()));
            type = cret;
            funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
        }
    }
    else if (ctx->typeName())
    {
        auto cret = (visitTypeName(ctx->typeName()));
        if ((ctx->typeName() - 1)->getText() == "sizeof")
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, cret.getsize()});
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
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            if (type.kind == Type::Kind::Pointer)
            {
                const auto step =
                    static_cast<long>(type.subType ? type.subType->getsize() : VCPU<>::size_word);
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, step});
                funcnow->asms.push_back(ASM{ASM::basic_asm::ADD});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
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
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            if (type.kind == Type::Kind::Pointer)
            {
                const auto step =
                    static_cast<long>(type.subType ? type.subType->getsize() : VCPU<>::size_word);
                funcnow->asms.push_back(ASM{ASM::basic_asm::LW});
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, step});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SUB});
                funcnow->asms.push_back(ASM{ASM::basic_asm::SW});
            }
            else if (type.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
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
    return type;
}
Type astVisitor::visitPostfixExpression(ComplierParser::PostfixExpressionContext* ctx)
{
    // 注意处理多个后缀
    std::function<Type(int, int)> func = [&func, this, ctx](int start, int end) -> Type
    {
        if (end == 0)
        {
            auto str = ctx->children[0]->getText();
            return (visitPrimaryExpression(
                dc<ComplierParser::PrimaryExpressionContext*>(ctx->children[0])));
        }
        auto todo = ctx->children[end - 1];
        if (todo->getText() == "(") // func call
        {
            int args_num = 0;
            if (ctx->children[end]->getText() != ")") // 有expressionlist
            {
                visitArgumentExpressionList(
                    dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end]));
                args_num = dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end])
                               ->assignmentExpression()
                               .size();
            }
            auto funcaddr = func(0, end - 1); // 解析函数地址
            funcnow->asms.push_back(ASM{ASM::basic_asm::CALL});
            Type rettype = funcaddr;
            if (funcaddr.kind == Type::Kind::Pointer && funcaddr.arr_or_ptr_num == 1 &&
                funcaddr.subType->kind == Type::Kind::Function) // 函数指针
            {
                rettype = *funcaddr.subType->subType;
                // args_num = funcaddr.value().subType->args.size();// 由实参决定以支持可变参
            }
            else if (funcaddr.kind == Type::Kind::Function) // 函数
            {
                rettype = *funcaddr.subType;
                // args_num = funcaddr.value().args.size();
            }
            else
            {
                THROW_ERR(error::expected_func_or_funcptr, ctx);
            }
            funcnow->asms.push_back(ASM{ASM::basic_asm::DARG, args_num});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
            return rettype;
        }
        else if (todo->getText() == "[") // arr[] ptr[]
        {
            auto paret = func(0, end - 1);
            Type eleType;
            if (paret.kind == Type::Kind::Pointer && paret.arr_or_ptr_num == 1)
            {
                eleType = *paret.subType;
            }
            else if (paret.kind == Type::Kind::Pointer && paret.arr_or_ptr_num > 1)
            {
                eleType = paret;
                eleType.arr_or_ptr_num--;
            }
            else if (paret.kind == Type::Kind::Array)
            {
                eleType = *paret.subType;
            }
            (void)(visitExpression(dc<ComplierParser::ExpressionContext*>(ctx->children[end])));
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
            return eleType;
        }
        else if (todo->getText() == "++" || todo->getText() == "--")
        {
            auto valType = func(0, end - 1);
            auto& lastAsm = funcnow->asms.back();
            if (lastAsm != "LC" && lastAsm != "LI" && lastAsm != "LW")
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->asms.pop_back();

            auto pickLoadStore = [](const Type& ty)
            {
                auto charSize = Type{Type::Kind::Basic, Type::BasicType::Char}.getsize();
                auto intSize = Type{Type::Kind::Basic, Type::BasicType::Int}.getsize();
                if (ty.getsize() == charSize)
                {
                    return std::pair{ASM::basic_asm::LC, ASM::basic_asm::SC};
                }
                if (ty.getsize() == intSize)
                {
                    return std::pair{ASM::basic_asm::LI, ASM::basic_asm::SI};
                }
                return std::pair{ASM::basic_asm::LW, ASM::basic_asm::SW};
            };

            auto [loadOp, storeOp] = pickLoadStore(valType);

            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            funcnow->asms.push_back(ASM{loadOp});
            funcnow->asms.push_back(ASM{ASM::basic_asm::COPY});
            funcnow->asms.push_back(ASM{ASM::basic_asm::POP});

            long step = 1;
            if (valType.kind == Type::Kind::Pointer)
            {
                auto pointed = valType.subType ? valType.subType->getsize() : VCPU<>::size_word;
                step = static_cast<long>(pointed);
            }

            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->asms.push_back(
                ASM{todo->getText() == "++" ? ASM::basic_asm::ADD : ASM::basic_asm::SUB});
            funcnow->asms.push_back(ASM{storeOp});
            funcnow->asms.push_back(ASM{ASM::basic_asm::PUSH});
            return valType;
        }
        else
        {
            return func(0, end - 1);
        }
    };
    return func(0, ctx->children.size());
}
Type astVisitor::visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx)
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
            return var->type;
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
            return *varg;
        }
        auto str = ctx->Identifier()->getText();
        auto funcret = obj.symbol_table.lookup_func_decl(ctx->Identifier()->getText());
        if (funcret)
        {
            funcnow->asms.push_back(
                ASM{ASM::basic_asm::IMM, "func@" + ctx->Identifier()->getText()});
            return *funcret;
        }
        THROW_ERR(error::undifined_id, ctx->Identifier());
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
                return Type{Type::Kind::Basic, Type::BasicType::Char};
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->Constant());
            }
        }
        // 处理整型常量
        else if (isIntegerConstant(text))
        {
            try
            {
                int value = parseIntegerConstant(text);
                funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, value});
                return Type{Type::Kind::Basic, Type::BasicType::Int};
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->Constant());
            }
        }
        // [TODO] 其他类型的常量（如浮点数）
        else
        {
            THROW_ERR(error::unsurpported_num, ctx->Constant());
        }
    }
    else if (ctx->expression()) // (expr)
    {
        return visitExpression(ctx->expression());
    }
    else if (ctx->StringLiteral().size())
    {
        // 返回指向字符的指针类型（占位实现）
        Type t{Type::Kind::Pointer, 1};
        t.pushTop(Type{Type::Kind::Basic, Type::BasicType::Char});
        // [TODO] 生成字符串常量存储
        return t;
    }
    else
    {
        auto str = ctx->getText();
        THROW_ERR(error::invalid_constant, ctx);
    }
    throw;
}
Type astVisitor::visitTypeName(ComplierParser::TypeNameContext* ctx)
{
    // typeName由specifierQualifierList和可选的abstractDeclarator组成
}

void astVisitor::visitBlockItem(ComplierParser::BlockItemContext* ctx)
{
    if (ctx->statement())
    {
        return visitStatement(ctx->statement());
    }
    else if (ctx->declaration())
    {
        auto vars = visitDeclaration(ctx->declaration());
        for (auto& each : vars)
        {
            varDef var;
            var.name = each.id;
            var.type = *each.subType;
            auto varptr = funcnow->add_var(var);
            if (!varptr)
            {
                THROW_ERR(error::double_defined, ctx->declaration());
            }
        }
    }
    else if (ctx->asmADDer())
    {
        visitAsmADDer(ctx->asmADDer());
    }
}

void astVisitor::visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx)
{
}

void astVisitor::visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "if")
    {
        (void)(visitExpression(ctx->expression()));
        int after_expr = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // for jz
        visitStatement(ctx->statement()[0]);
        if (ctx->statement().size() == 2) // 有else
        {
            int after_id = funcnow->asms.size();
            funcnow->asms.push_back("HOLD"); // for 'if' statments jump through 'else' to end
            funcnow->asms[after_expr] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->asms.size())};

            visitStatement(ctx->statement()[1]);
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
}

void astVisitor::visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext* ctx)
{
    // 实参从右向左入栈
    for (int i = ctx->assignmentExpression().size() - 1; i >= 0; i--)
    {
        (void)(visitAssignmentExpression(ctx->assignmentExpression()[i]));
    }
}

void astVisitor::visitIterationStatement(ComplierParser::IterationStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "while")
    {
        int startppos = funcnow->asms.size();
        (void)(visitExpression(ctx->expression()));
        int after_expr = funcnow->asms.size();
        funcnow->asms.push_back("HOLD"); // for jz end
        visitStatement(ctx->statement());
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
    }
    // [TODO] 'for' statement
}

void astVisitor::visitJumpStatement(ComplierParser::JumpStatementContext* ctx)
{
    // [TODO] goto statement
    if (ctx->children[0]->getText() == "continue")
    {
        funcnow->asms.push_back("lable@continue");
    }
    else if (ctx->children[0]->getText() == "break")
    {
        funcnow->asms.push_back("lable@break");
    }
    else if (ctx->children[0]->getText() == "return")
    {
        if (ctx->expression())
        {
            (void)(visitExpression(ctx->expression()));
        }
        else
        {
            funcnow->asms.push_back(ASM{ASM::basic_asm::IMM, 0});
        }
        funcnow->asms.push_back(ASM{ASM::basic_asm::RET});
    }
}

Type astVisitor::visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx)
{
    Type rettype;
    if (ctx->directAbstractDeclarator())
    {
        rettype = (visitDirectAbstractDeclarator(ctx->directAbstractDeclarator()));
    }
    if (ctx->pointer())
    {
        rettype.pushTop(Type(Type::Kind::Pointer,
                             static_cast<int>(std::count(ctx->pointer()->getText().begin(),
                                                         ctx->pointer()->getText().end(), '*'))));
    }
    return rettype;
}

Type astVisitor::visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext* ctx)
{
    Type basetype;
    if (ctx->LeftParen() && ctx->abstractDeclarator()) // (abst)
    {
        basetype = (visitAbstractDeclarator(ctx->abstractDeclarator()));
        return basetype;
    }
    if (ctx->directAbstractDeclarator())
    {
        basetype = (visitDirectAbstractDeclarator(ctx->directAbstractDeclarator()));
    }
    if (ctx->LeftBracket() && ctx->assignmentExpression()) // arr
    {
        // parseConstexpr 返回 long long, 这里数组维度内部使用 int, 显式窄化避免警告
        basetype.pushTop(
            Type(Type::Kind::Array, static_cast<int>(parseConstexpr(ctx->assignmentExpression()))));
        return basetype;
    }
    else if (ctx->LeftParen()) // func
    {
        std::vector<Type> args;
        if (ctx->parameterTypeList())
        {
            args = visitParameterTypeList(ctx->parameterTypeList());
        }
        basetype.pushTop(Type{Type::Kind::Function, args});
        return basetype;
    }
    THROW_ERR(error::unsurpport_abstractDeclarator, ctx);
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
void astVisitor::visitAsmADDer(ComplierParser::AsmADDerContext* ctx)
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
    return;
}
// void astVisitor::visitByTypeIndex(antlr4::ParserRuleContext* ctx)
// {
//     switch (ctx->getRuleIndex())
//     {
//     case ComplierParser::RuleCompilationUnit:
//         return visitCompilationUnit(
//             dynamic_cast<ComplierParser::ComplierParser::CompilationUnitContext*>(ctx));
//         break;
//     case ComplierParser::RuleTranslationUnit:
//         return visitTranslationUnit(
//             dynamic_cast<ComplierParser::ComplierParser::TranslationUnitContext*>(ctx));
//         break;
//     case ComplierParser::RuleExternalDeclaration:
//         return visitExternalDeclaration(
//             dynamic_cast<ComplierParser::ExternalDeclarationContext*>(ctx));
//         break;
//     case ComplierParser::RuleDeclaration:
//         return visitDeclaration(
//             dynamic_cast<ComplierParser::ComplierParser::DeclarationContext*>(ctx));
//         break;
//     case ComplierParser::RuleFunctionDefinition:
//         return visitFunctionDefinition(
//             dynamic_cast<ComplierParser::FunctionDefinitionContext*>(ctx));
//         break;
//     case ComplierParser::RuleDeclarator:
//         // 处理声明符，包括数组和函数指针等
//         // 这里暂不实现
//         break;
//     case ComplierParser::RuleDirectDeclarator:
//         // 处理直接声明符，包括数组维度等
//         // 这里暂不实现
//         break;
//     // 更多类型的处理...
//     default:
//         // 对于未明确处理的节点类型，返回默认值
//     }

//     // 默认返回成功
// }
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