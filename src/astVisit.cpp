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
    // 使用改进的makeByNode函数来解析声明
    if (auto ret = varDef::makeByNode(ctx))
    {
        // 成功解析为变量定义
        obj.symbol_table.add_global_symbol(ret.value());
        
        // 处理函数指针的特殊情况
        if (ret.value().type.is_function())
        {
            // 这可能是一个函数指针，因此可能需要查找函数符号
            funcnow = obj.symbol_table.lookup_fun(ret.value().name);
        }
    }
    else if (auto ret2 = funcDef::makeByNode(ctx))
    {
        // 成功解析为函数定义
        obj.symbol_table.add_global_symbol(ret2.value());
        funcnow = obj.symbol_table.lookup_fun(ret2.value().name);
    }
    else
    {
        // 如果无法解析，可能是因为声明中有不支持的语法
        // 这里可以打印警告，但现在我们简单地返回true
        std::cerr << "警告: 无法解析的声明: " << ctx->getText() << std::endl;
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
    Type combinedType;
    combinedType.kind = Type::Kind::Basic; // 默认为基本类型
    combinedType.basic_type = Type::BasicType::Int; // 默认为int类型
    
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
std::any astVisitor::visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx)
{
    // 检查是否为类型说明符
    if (ctx->typeSpecifier())
    {
        // 直接返回typeSpecifier的结果，不再转换为varDef
        return visitTypeSpecifier(ctx->typeSpecifier());
    }
    // [TODO] 处理存储类说明符
    else if (ctx->storageClassSpecifier())
    {
        // 暂时返回默认类型
        return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    }
    // [TODO] 处理类型限定符
    else if (ctx->typeQualifier())
    {
        // 暂时返回默认类型
        return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    }
    // [TODO] 处理函数说明符
    else if (ctx->functionSpecifier())
    {
        // 暂时返回默认类型
        return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    }
    // 处理对齐说明符
    else if (ctx->alignmentSpecifier())
    {
        // 暂时返回默认类型
        return std::expected<Type::BasicType, error>(Type::BasicType::Int);
    }
    
    // 如果无法识别说明符类型，返回错误
    return std::unexpected(error::unsurpport_basictype);
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
// 辅助函数：递归收集多维数组的维度信息
std::vector<int> astVisitor::collectArrayDimensions(ComplierParser::DirectDeclaratorContext* ddCtx)
{
    std::vector<int> dimensions;
    
    // 递归处理嵌套的directDeclarator
    std::function<void(ComplierParser::DirectDeclaratorContext*)> collect = 
        [&dimensions, &collect](ComplierParser::DirectDeclaratorContext* ctx) {
            // 检查是否有数组维度声明
            for (size_t i = 0; i < ctx->children.size(); ++i)
            {
                if (i + 3 <= ctx->children.size() && ctx->children[i]->getText() == "[" &&
                    ctx->children[i + 2]->getText() == "]")
                {
                    // 尝试获取数组大小
                    auto sizeExpr = ctx->children[i + 1];
                    int dimension = -1; // 默认为未指定大小
                    
                    if (auto constExpr = dynamic_cast<ComplierParser::ConstantExpressionContext*>(sizeExpr))
                    {
                        // 尝试从常量表达式中提取整数值
                        try
                        {
                            dimension = std::stoi(constExpr->getText());
                        }
                        catch (...)
                        {
                            // 转换失败，保持默认值
                        }
                    }
                    
                    dimensions.push_back(dimension);
                    i += 2; // 跳过已处理的部分
                }
            }
            
            // 递归处理第一个子节点，如果它是directDeclarator
            if (!ctx->children.empty())
            {
                if (auto childDD = dynamic_cast<ComplierParser::DirectDeclaratorContext*>(ctx->children[0]))
                {
                    collect(childDD);
                }
            }
        };
    
    // 开始收集
    collect(ddCtx);
    
    return dimensions;
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
    case ComplierParser::RuleDeclaration:
        return visitDeclaration(dynamic_cast<ComplierParser::DeclarationContext*>(ctx));
        break;
    case ComplierParser::RuleFunctionDefinition:
        return visitFunctionDefinition(dynamic_cast<ComplierParser::FunctionDefinitionContext*>(ctx));
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