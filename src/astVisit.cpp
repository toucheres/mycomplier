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
        if (ac<bool>(visitExternalDeclaration(each)))
        {
        }
    }
    return {};
}
std::any astVisitor::visitDeclaration(ComplierParser::DeclarationContext* ctx)
{
    std::vector<varDef> vars;
    if (ctx->declarationSpecifiers()) // 基础类型
    {
        visitDeclarationSpecifiers(ctx->declarationSpecifiers());
    }
    else if (ctx->initDeclaratorList()) // 数组/函数/指针的组合
    {
        visitInitDeclaratorList(ctx->initDeclaratorList());
    }
    return true;
    // [TODO] 处理初始化器
    // 如果有初始化器，需要处理 ctx->initDeclaratorList()
}
std::any astVisitor::visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx)
{
    // 解析函数声明说明符（返回类型等）
    Type returnType;
    returnType.basic_type = Type::BasicType::Int; // 默认为int

    if (ctx->declarationSpecifiers())
    {
        auto specResult = visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        // 处理返回类型...
    }

    // 解析函数声明符（函数名、参数列表等）
    if (ctx->declarator())
    {
        // 此处可以从declarator中提取函数名和参数列表
        // 这里需要访问directDeclarator等子节点
    }

    // 创建函数定义
    funcDef func;
    // 设置函数属性...

    // 添加到符号表
    obj.symbol_table.add_global_symbol(func);
    funcnow = obj.symbol_table.lookup_fun(func.name);

    // 处理函数体
    if (ctx->compoundStatement())
    {
        // 进入函数作用域
        if (funcnow)
        {
            funcnow->enter_scope();
            // 处理声明列表
            if (ctx->declarationList())
            {
                for (auto decl : ctx->declarationList()->declaration())
                {
                    visitDeclaration(decl);
                }
            }

            // 处理复合语句（函数体）
            // visitCompoundStatement(ctx->compoundStatement());

            // 离开函数作用域
            funcnow->exit_scope();
        }
    }

    return true;
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
    std::vector<varDef> Types;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        std::any result = visitDeclarationSpecifier(each);
        auto ret = std::any_cast<std::expected<varDef, error>>(result);
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
    return std::expected<std::vector<varDef>, error>(Types);
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
    varDef rettype;
    // varDef rettpe;
    if (ctx->typedefName()) // 语法阶段无法判断是类型别名还是id, 均以typedefName表示
    {
        if (obj.typedefs.find(ctx->typedefName()->getText()) != obj.typedefs.end()) // 是类型别名
        {
            rettype.type = obj.typedefs.find(ctx->typedefName()->getText())->second;
            return std::expected<varDef, error>(rettype);
        }
        else // 是id
        {
            rettype.name = ctx->typedefName()->getText();
            return std::expected<varDef, error>(rettype);
        }
    }
    // 判断基本类型
    else if (ctx->getText() == "int")
    {
        rettype.type.basic_type = Type::BasicType::Int;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "char")
    {
        rettype.type.basic_type = Type::BasicType::Char;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "void")
    {
        rettype.type.basic_type = Type::BasicType::Void;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "float")
    {
        rettype.type.basic_type = Type::BasicType::Float;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "double")
    {
        rettype.type.basic_type = Type::BasicType::Double;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "long")
    {
        rettype.type.basic_type = Type::BasicType::Long;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "short")
    {
        rettype.type.basic_type = Type::BasicType::Short;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "unsigned")
    {
        rettype.type.basic_type = Type::BasicType::Unsigned;
        return std::expected<varDef, error>(rettype);
    }
    else if (ctx->getText() == "signed")
    {
        rettype.type.basic_type = Type::BasicType::Signed;
        return std::expected<varDef, error>(rettype);
    }
    return std::unexpected(error::unsurpport_basictype);
}
std::any astVisitor::visitInitDeclaratorList(ComplierParser::InitDeclaratorListContext* ctx)
{
    std::vector<varDef> vars;
    for (auto& each : ctx->initDeclarator())
    {
        auto ret = ac<std::expected<varDef, error>>(visitInitDeclarator(each));
        if (ret)
        {
            vars.push_back(ret.value());
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<std::vector<varDef>, error>(vars);
}
std::any astVisitor::visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx)
{
    auto ret = ac<std::expected<varDef, error>>(visitDeclarator(ctx->declarator()));
    if (!ret)
    {
        return ret;
    }
    return ret;
    // [TODO]初始化ctx->initializer
}
std::any astVisitor::visitDeclarator(ComplierParser::DeclaratorContext* ctx)
{
    varDef var;
    var.type.kind = Type::Kind::Basic;
    if (ctx->pointer())
    {
        auto pointerStr = ctx->pointer()->toString();
        int starCount = std::count(pointerStr.begin(), pointerStr.end(), '*');
        var.type.kind = Type::Kind::Pointer;
        var.type.ptr_info = std::make_shared<PtrInfo>();
        var.type.ptr_info->num_lay = starCount;
        auto ret = ac<std::expected<varDef, error>>(visitDirectDeclarator(ctx->directDeclarator()));
        if (!ret)
        {
            return std::unexpected<error>(ret.error());
        }
        var.type.ptr_info->elementType = std::make_shared<Type>(ret.value().type);
    }
    else if (ctx->directDeclarator())
    {
        return ac<std::expected<varDef, error>>(visitDirectDeclarator(ctx->directDeclarator()));
    }
}
std::any astVisitor::visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx)
{
    varDef var;
    int index = ctx->getAltNumber();
    if (index == 0) // id
    {
        var.name = ctx->toString();
        return std::expected<varDef, error>(var);
    }
    else if (index == 1) // ()
    {
        return ac<std::expected<varDef, error>>(visitDeclarator(ctx->declarator()));
    }
    else if (index == 2) // []
    {
        // [TODO] 表达式解析
        ctx->assignmentExpression();
    }
    else if (index == 3) // unkown
    {
    }
    else if (index == 4) // unkown
    {
    }
    else if (index == 5) // int arr[*] //vla
    {
    }
    else if (index == 6) // fun(int a,int b)
    {
        var.type.kind == Type::Kind::Function;
        auto ret = ac<std::expected<std::vector<varDef>, error>>(
            visitParameterTypeList(ctx->parameterTypeList()));
        if (ret)
        {
            var.type.func_info = std::make_shared<FunctionInfo>();
            var.type.func_info->param_types = ret.value();
        }
        else
        {
        }
        auto rettype =
            ac<std::expected<varDef, error>>(visitDirectDeclarator(ctx->directDeclarator()));
        if (rettype)
        {
            var.type.func_info->retType = std::make_shared<Type>(rettype.value().type);
        }
        else
        {
        }
        return std::expected<varDef, error>(var);
    }
    else if (index == 7) // fun(a,b) 旧式c
    {
    }
    else if (index == 8) // 位域
    {
    }
    else if (index == 9) // vc
    {
    }
    else if (index == 10) // vc
    {
    }
}
std::any astVisitor::visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx)
{
    return visitParameterList(ctx->parameterList());
}
std::any astVisitor::visitParameterList(ComplierParser::ParameterListContext* ctx)
{
    std::vector<varDef> vars;
    for (auto& each : ctx->parameterDeclaration())
    {
        auto ret = ac<std::expected<varDef, error>>(visitParameterDeclaration(each));
        if (ret)
        {
            vars.push_back(ret.value());
        }
        else
        {
            return std::unexpected<error>(ret.error());
        }
    }
    return std::expected<std::vector<varDef>, error>(vars);
}
std::any astVisitor::visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx)
{
    if (ctx->getAltNumber() == 0)
    {
        return ac<std::expected<std::vector<varDef>, error>>(
            visitDeclarationSpecifiers(ctx->declarationSpecifiers()));
    }
    else if (ctx->getAltNumber() == 1)
    {
        visitDeclarationSpecifiers2(ctx->declarationSpecifiers2());
    }
    return std::any();
}
std::any astVisitor::visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context* ctx)
{
    Type combinedType;
    bool hasTypeInfo = false;
    // 遍历所有声明说明符
    for (auto each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        std::any result = visitDeclarationSpecifier(each);

        // 检查返回类型是否符合预期
        if (result.type() == typeid(std::expected<Type::BasicType, error>))
        {
            auto ret = std::any_cast<std::expected<Type::BasicType, error>>(result);
            if (ret)
            {
                // 合并类型信息
                // 这里简化处理，实际上需要考虑类型组合规则
                combinedType.basic_type = ret.value();
                hasTypeInfo = true;
            }
            else
            {
                // 返回错误
                std::vector<varDef> empty;
                return std::unexpected(ret.error());
            }
        }
        // 其他类型的返回值处理
        // ...
    }

    // 创建一个临时变量定义来返回类型信息
    varDef tempVar;
    tempVar.type = combinedType;

    std::vector<varDef> vars;
    vars.push_back(tempVar);

    return std::expected<std::vector<varDef>, error>(vars);
}

std::any astVisitor::visitTypeName(ComplierParser::TypeNameContext* ctx)
{
    // typeName由specifierQualifierList和可选的abstractDeclarator组成

    Type type;
    type.kind = Type::Kind::Basic; // 默认为基本类型

    // 先处理类型说明符列表
    if (ctx->specifierQualifierList())
    {
        auto result = visitSpecifierQualifierList(ctx->specifierQualifierList());
        if (result.type() == typeid(std::expected<Type::BasicType, error>))
        {
            auto basicType = std::any_cast<std::expected<Type::BasicType, error>>(result);
            if (basicType)
            {
                type.basic_type = basicType.value();
            }
            else
            {
                return std::unexpected<error>(basicType.error());
            }
        }
    }

    // 处理抽象声明符（如有）
    if (ctx->abstractDeclarator())
    {
        auto ret = visitAbstractDeclarator(ctx->abstractDeclarator());
        if (ret.type() == typeid(std::expected<Type, error>))
        {
            auto abstractType = std::any_cast<std::expected<Type, error>>(ret);
            if (abstractType)
            {
                // 合并基本类型和抽象声明符信息
                Type resultType = abstractType.value();
                resultType.basic_type = type.basic_type;
                return std::expected<Type, error>(resultType);
            }
            else
            {
                return std::unexpected<error>(abstractType.error());
            }
        }
    }

    return std::expected<Type, error>(type);
}

std::any astVisitor::visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx)
{
    varDef basicType; // 默认为int类型

    // 处理类型说明符
    if (ctx->typeSpecifier())
    {
        auto result = ac<std::expected<varDef, error>>(visitTypeSpecifier(ctx->typeSpecifier()));
        if (result)
        {
            basicType = result.value();
        }
        else
        {
            return std::unexpected<error>(result.error());
        }
    }

    // 处理类型限定符（暂时忽略）
    // if (ctx->typeQualifier()) { ... }

    // 递归处理剩余的说明符和限定符
    if (ctx->specifierQualifierList())
    {
        auto result = visitSpecifierQualifierList(ctx->specifierQualifierList());
        if (result.type() == typeid(std::expected<Type::BasicType, error>))
        {
            // 可以进一步结合类型，例如处理 "unsigned int" 这样的组合
            // 此处简化处理，优先使用左侧的类型说明符
        }
    }

    return std::expected<varDef, error>(basicType);
}

std::any astVisitor::visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx)
{
    // abstractDeclarator有三种可能：
    // 1. 仅指针
    // 2. 仅directAbstractDeclarator（可能带gccDeclaratorExtension）
    // 3. 指针+directAbstractDeclarator（可能带gccDeclaratorExtension）

    Type type;

    // 处理指针（如果存在）
    if (ctx->pointer())
    {
        auto pointerStr = ctx->pointer()->getText();
        int starCount = std::count(pointerStr.begin(), pointerStr.end(), '*');

        type.kind = Type::Kind::Pointer;
        type.ptr_info = std::make_shared<PtrInfo>();
        type.ptr_info->num_lay = starCount;

        // 如果有directAbstractDeclarator，指针指向的是它表示的类型
        if (ctx->directAbstractDeclarator())
        {
            auto ret = visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
            if (ret.type() == typeid(std::expected<Type, error>))
            {
                auto directType = std::any_cast<std::expected<Type, error>>(ret);
                if (directType)
                {
                    type.ptr_info->elementType = std::make_shared<Type>(directType.value());
                }
                else
                {
                    return std::unexpected<error>(directType.error());
                }
            }
        }
        else
        {
            // 如果没有directAbstractDeclarator，指针指向的是基本类型
            // 这个基本类型会在typeName中被设置
            Type baseType;
            baseType.kind = Type::Kind::Basic;
            type.ptr_info->elementType = std::make_shared<Type>(baseType);
        }
    }
    // 如果没有指针，只有directAbstractDeclarator
    else if (ctx->directAbstractDeclarator())
    {
        return visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
    }

    return std::expected<Type, error>(type);
}

std::any astVisitor::visitDirectAbstractDeclarator(
    ComplierParser::DirectAbstractDeclaratorContext* ctx)
{
    // directAbstractDeclarator可以有多种形式，对应C语言中各种类型声明

    Type type;
    int altNumber = ctx->getAltNumber();

    // 判断当前匹配的是哪个规则分支
    // 注意：altNumber基于ANTLR规则中的分支顺序，从0开始

    // 分支0: '(' abstractDeclarator ')'
    if (altNumber == 0)
    {
        if (ctx->abstractDeclarator())
        {
            return visitAbstractDeclarator(ctx->abstractDeclarator());
        }
    }
    // 分支1: '[' typeQualifierList? assignmentExpression? ']'
    else if (altNumber == 1)
    {
        // 数组类型
        type.kind = Type::Kind::Array;
        type.array_info = std::make_shared<ArrayInfo>();

        // 尝试获取数组大小
        int dimension = -1; // 默认为未指定大小
        if (ctx->assignmentExpression())
        {
            // 简化处理，假设是常量表达式
            try
            {
                dimension = std::stoi(ctx->assignmentExpression()->getText());
            }
            catch (...)
            {
                // 转换失败，保持默认值
            }
        }

        type.array_info->size = dimension;
    }
    // 分支2: '[' 'static' typeQualifierList? assignmentExpression ']'
    else if (altNumber == 2)
    {
        // 带static的数组
        type.kind = Type::Kind::Array;
        type.array_info = std::make_shared<ArrayInfo>();

        // 尝试获取数组大小
        int dimension = -1;
        if (ctx->assignmentExpression())
        {
            try
            {
                dimension = std::stoi(ctx->assignmentExpression()->getText());
            }
            catch (...)
            {
                // 转换失败，保持默认值
            }
        }

        type.array_info->size = dimension;
        // 注意：static关键字通常用于函数参数，指示编译器数组至少有这么多元素
        // 这里可以添加标记来表示这一点
    }
    // 分支3: '[' typeQualifierList 'static' assignmentExpression ']'
    else if (altNumber == 3)
    {
        // 类型限定符+static的数组，处理方式类似分支2
        type.kind = Type::Kind::Array;
        type.array_info = std::make_shared<ArrayInfo>();

        int dimension = -1;
        if (ctx->assignmentExpression())
        {
            try
            {
                dimension = std::stoi(ctx->assignmentExpression()->getText());
            }
            catch (...)
            {
                // 转换失败，保持默认值
            }
        }

        type.array_info->size = dimension;
    }
    // 分支4: '[' '*' ']'
    else if (altNumber == 4)
    {
        // 可变长度数组
        type.kind = Type::Kind::Array;
        type.array_info = std::make_shared<ArrayInfo>();
        type.array_info->size = -1; // VLA用-1表示
        // 可以添加标记表示这是VLA
    }
    // 分支5: '(' parameterTypeList? ')'
    else if (altNumber == 5)
    {
        // 函数类型
        type.kind = Type::Kind::Function;
        type.func_info = std::make_shared<FunctionInfo>();

        // 处理参数类型列表
        if (ctx->parameterTypeList())
        {
            // 这里需要处理参数类型列表，可以复用已有的visitParameterTypeList
            // 暂时简化处理
            type.func_info->is_variadic = false; // 默认非可变参数
            // TODO: 添加参数类型
        }
    }
    // 分支6-10: 递归形式的directAbstractDeclarator
    // 这些是前面基本形式的递归应用，例如数组的数组、函数返回数组等
    else if (altNumber >= 6 && altNumber <= 10)
    {
        // 递归处理内层的directAbstractDeclarator
        if (ctx->directAbstractDeclarator())
        {
            auto innerResult = visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
            if (innerResult.type() == typeid(std::expected<Type, error>))
            {
                auto innerType = std::any_cast<std::expected<Type, error>>(innerResult);
                if (!innerType)
                {
                    return std::unexpected<error>(innerType.error());
                }

                // 根据当前分支，修改或扩展内层类型
                if (altNumber == 6 || altNumber == 7 || altNumber == 8 || altNumber == 9)
                {
                    // 数组相关分支，为内层类型添加数组维度
                    Type arrayType;
                    arrayType.kind = Type::Kind::Array;
                    arrayType.array_info = std::make_shared<ArrayInfo>();

                    // 获取数组大小
                    int dimension = -1;
                    if (ctx->assignmentExpression())
                    {
                        try
                        {
                            dimension = std::stoi(ctx->assignmentExpression()->getText());
                        }
                        catch (...)
                        {
                            // 转换失败，保持默认值
                        }
                    }

                    arrayType.array_info->size = dimension;
                    arrayType.array_info->elementType = std::make_shared<Type>(innerType.value());

                    return std::expected<Type, error>(arrayType);
                }
                else if (altNumber == 10)
                {
                    // 函数相关分支，设置函数返回类型
                    Type funcType;
                    funcType.kind = Type::Kind::Function;
                    funcType.func_info = std::make_shared<FunctionInfo>();
                    funcType.func_info->retType = std::make_shared<Type>(innerType.value());

                    // 处理参数
                    if (ctx->parameterTypeList())
                    {
                        // TODO: 处理参数类型列表
                    }

                    return std::expected<Type, error>(funcType);
                }
            }
        }
    }

    return std::expected<Type, error>(type);
}
astVisitor::astVisitor(std::string name,OBJ& ob):obj(ob)
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