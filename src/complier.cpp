#include "complier.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "obj.h"
#include "preprocessor.hpp"
#include <antlr4-runtime/antlr4-runtime.h>
#include <astVisit.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <regex>
// 修改 getsize() 方法以支持新的类型表示
size_t Type::getsize() const
{
    if (is_array() && array_info.has_value() && !array_info->dimensions.empty())
    {
        // 计算元素大小
        int element_size =
            (this->basic_type == BasicType::Char && pointer_level == 0) ? 1 : VCPU::size_word;

        // 计算多维数组的总元素数量
        size_t total_elements = 1;
        for (const auto& dim : array_info->dimensions)
        {
            // 如果任何维度未指定大小，返回默认大小
            if (dim <= 0)
            {
                return VCPU::size_word;
            }
            total_elements *= dim;
        }

        return element_size * total_elements;
    }
    else if (is_function())
    {
        // 函数指针大小
        return VCPU::size_word;
    }
    else if (this->basic_type == BasicType::Char && !this->is_pointer())
    {
        return 1;
    }
    return VCPU::size_word;
}

// 修改 to_string() 方法
std::string Type::to_string() const
{
    std::string result;

    // 基本类型
    switch (basic_type)
    {
    case BasicType::Int:
        result = "int";
        break;
    case BasicType::Char:
        result = "char";
        break;
    case BasicType::Void:
        result = "void";
        break;
    case BasicType::Float:
        result = "float";
        break;
    case BasicType::Double:
        result = "double";
        break;
    case BasicType::Long:
        result = "long";
        break;
    case BasicType::Short:
        result = "short";
        break;
    case BasicType::Unsigned:
        result = "unsigned";
        break;
    case BasicType::Signed:
        result = "signed";
        break;
    }

    // 数组类型
    if (is_array() && array_info.has_value())
    {
        for (const auto& dim : array_info->dimensions)
        {
            result += "[";
            if (dim > 0)
            {
                result += std::to_string(dim);
            }
            result += "]";
        }
    }

    // 函数类型
    if (is_function() && func_info.has_value())
    {
        result += "(";
        for (size_t i = 0; i < func_info->param_types.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += func_info->param_types[i].to_string();
        }
        if (func_info->is_variadic)
        {
            if (!func_info->param_types.empty())
                result += ", ";
            result += "...";
        }
        result += ")";
    }

    // 指针
    for (int i = 0; i < pointer_level; ++i)
    {
        result = "*" + result;
    }

    return result;
}

std::expected<size_t, error> linker::pushfunc(std::string funcname)
{
    funcDef* func = nullptr;
    auto ret = addrmap.find("func@" + funcname);
    if (ret == addrmap.end()) // 重定向表未记录
    {
        bool flag = false;
        for (auto& eachobj : objs)
        {
            auto fun = eachobj.symbol_table.globalfuncdef.find(funcname);
            if (fun == eachobj.symbol_table.globalfuncdef.end())
            {
                continue;
            }
            else
            {
                if (flag)
                {
                    return std::unexpected(error::double_defined);
                }
                else
                {
                    func = &fun->second;
                    // 记录在重定向表中
                    size_t thisfuncstart = addrmap["func@" + funcname] = this->exe.asms.size();
                    for (auto& eachasms : func->asms)
                    {
                        this->exe.asms.push_back(eachasms);
                    }
                    flag = true;
                    for (int i = thisfuncstart; i < this->exe.asms.size(); i++)
                    {
                        std::regex pattern("func@([a-zA-Z_][a-zA-Z0-9_]*)");
                        std::smatch match;
                        if (std::regex_search(this->exe.asms[i], match, pattern) &&
                            match.size() > 1)
                        {
                            auto funnametoreaddr = match[1].str(); // 返回第一个捕获组
                            size_t pos = match.position(0);
                            size_t len = match.length(0);
                            auto it = addrmap.find("func@" + funnametoreaddr);
                            if (it != addrmap.end())
                            {
                                // 替换为实际地址
                                this->exe.asms[i].replace(pos, len, std::to_string(it->second));
                            }
                            else
                            {
                                auto ret = pushfunc(funnametoreaddr);
                                if (!ret)
                                {
                                    return std::unexpected(error::undifined_func);
                                }
                                this->exe.asms[i].replace(pos, len, std::to_string(ret.value()));
                            }
                        }
                        else
                        {
                            continue;
                        }
                    }
                    for (int i = thisfuncstart; i < this->exe.asms.size(); i++)
                    {
                        std::regex pattern("thisfun@([0-9]*)");
                        std::smatch match;
                        if (std::regex_search(this->exe.asms[i], match, pattern) &&
                            match.size() > 1)
                        {
                            auto lable = match[1].str(); // 返回第一个捕获组
                            size_t pos = match.position(0);
                            size_t len = match.length(0);
                            // 替换为实际地址
                            this->exe.asms[i].replace(
                                pos, len, std::to_string(std::stoi(lable) + thisfuncstart));
                        }
                        else
                        {
                            continue;
                        }
                    }
                }
            }
        }
    }
    else // 重定向表已记录
    {
        return addrmap["func@" + funcname];
    }
    return addrmap["func@" + funcname];
}
std::expected<std::vector<std::string>, error> linker::process()
{
    // 分配全局变量空间,确定地址
    for (auto& eachobj : objs)
    {
        for (auto& [name, eachgvar] : eachobj.symbol_table.globalvar)
        {
            eachgvar.addr = eachgvar.get_addr_in_mem(exe.global_size);
            exe.global_size = eachgvar.addr + eachgvar.type.getsize();
            auto labal = "globalvar@" + eachgvar.name;
            if (addrmap.find(labal) != addrmap.end())
            {
                return std::unexpected(error::double_defined);
            }
            addrmap[labal] = eachgvar.addr;
        }
    }
    auto ceiling = [](int n, int x)
    {
        // x 必须是 2 的幂
        return (n + x - 1) & ~(x - 1);
    };
    // 对其stack到4倍数
    exe.asms.push_back(ASM{ASM::basic_asm::UP,                          // 整体移动sp,bp
                           ceiling(exe.global_size, VCPU::size_word)}); // 向上对齐到 4 字节边界});
    // 拼接obj初始化函数
    for (auto& eachobj : objs)
    {
        auto ret = eachobj.symbol_table.globalfuncdef.find("__global_init_" + eachobj.name);
        if (ret == eachobj.symbol_table.globalfuncdef.end())
        {
            return std::unexpected(error::undifined_obj_init_fun);
        }
        for (auto& eachasm : ret->second.asms)
        {
            exe.asms.push_back(eachasm);
        }
    }
    this->exe.asms.push_back(ASM{ASM::basic_asm::IMM, exe.asms.size() + 3}); // call main
    this->exe.asms.push_back(ASM{ASM::basic_asm::CALL});                     // call main
    this->exe.asms.push_back(ASM{ASM::basic_asm::EXIT});
    // 推入main函数, 并重定向func@
    if (auto ret = pushfunc("main"); !ret)
    {
        return std::unexpected(ret.error());
    }
    else
    {
        return exe.asms;
    }
}

// size_t Type::getsize() const
// {
//     if (this->basic_type == BasicType::Char && (!this->is_pointer()))
//     {
//         return 1;
//     }
//     return VCPU::size_word;
// }

// Type::Type(std::shared_ptr<peg::Ast> astnode)
// {
//     if (!astnode)
//     {
//         throw;
//     }
//     auto node = *astnode;
//     std::string basictypename = node.nodes[0]->token_to_string();
//     if (basictypename == "int")
//     {
//         basic_type = BasicType::Int;
//     }
//     else if (basictypename == "char")
//     {
//         basic_type = BasicType::Char;
//     }
//     else if (basictypename == "void")
//     {
//         basic_type = BasicType::Void;
//     }
//     if (node.nodes.size() == 2)
//     {
//         // 有ptr
//         auto& ptrs = *node.nodes[1];
//         std::string ptr_str = ptrs.token_to_string();
//         this->pointer_level = std::count(ptr_str.begin(), ptr_str.end(), '*');
//     }
// }

// argDef::argDef(std::shared_ptr<peg::Ast> astnode)
// {
//     if (!astnode)
//     {
//         throw;
//     }
//     auto node = *astnode;
//     type = Type{node.nodes[0]};
//     name = node.nodes[1]->token_to_string();
// }

void funcDef::enter_scope()
{
    funcvar_stack.push_back(std::vector<varDef>{});
}

void funcDef::exit_scope()
{
    funcvar_stack.pop_back();
    stack_size_now = VCPU::size_word * 2;
    // 使用反向迭代器
    for (auto it = funcvar_stack.rbegin(); it != funcvar_stack.rend(); ++it)
    {
        if ((*it).empty())
        {
            continue;
        }
        else
        {
            stack_size_now = it->back().addr + it->back().type.getsize();
        }
    }
}

const varDef* funcDef::lookup_var(const std::string& inname) const
{
    const varDef* ptr = nullptr;
    for (int i = 0; i < funcvar_stack.size(); i++)
    {
        auto& block = funcvar_stack[funcvar_stack.size() - i - 1];
        for (int j = 0; j < block.size(); j++)
        {
            if (block[block.size() - j - 1].name == inname)
            {
                ptr = &(block[block.size() - j - 1]);
                break;
            }
        }
    }
    if (!ptr)
    {
        for (auto& each : args)
        {
            if (each.name == inname)
            {
                ptr = &each;
                break;
            }
        }
    }
    return ptr;
}

const varDef* funcDef::add_var(const varDef& vardef)
{
    varDef var = vardef;
    var.addr = var.get_addr_in_mem(stack_size_now);
    var.is_defined = true;
    stack_size_now = var.addr + var.type.getsize();
    max_stack_size = std::max(stack_size_now, max_stack_size);
    funcvar_stack.back().push_back(var);
    return &funcvar_stack.back().back();
}

bool funcDef::add_arg(std::vector<varDef>& vardef)
{
    args = vardef;
    for (int i = 0; i < vardef.size(); i++)
    {
        args[i].addr = -VCPU::size_word * (i + 3);
    }
    return true;
}

varDef* SymbolTable::add_global_symbol(const varDef& vardef)
{
    if (auto ret = globalvar.find(vardef.name); ret == globalvar.end())
    {
        globalvar[vardef.name] = vardef;
        return &globalvar[vardef.name];
    }
    else
    {
        return nullptr;
    }
}

funcDef* SymbolTable::add_global_symbol(const funcDef& funcdef)
{
    if (auto ret = this->globalfuncdef.find(funcdef.name); ret == globalfuncdef.end())
    {
        globalfuncdef[funcdef.name] = funcdef;
        return &globalfuncdef[funcdef.name];
    }
    else
    {
        return nullptr;
    }
}

varDef* SymbolTable::lookup_var(const std::string& name)
{
    varDef* ptr = nullptr;
    if (auto ret = this->globalvar.find(name); ret != globalvar.end())
    {
        ptr = &ret->second;
    }
    return ptr;
}

funcDef* SymbolTable::lookup_fun(const std::string& name) // 移除了 const
{
    if (auto ret = this->globalfuncdef.find(name); ret != globalfuncdef.end())
    {
        return &(ret->second); // 现在可以修改了
    }
    else
    {
        return nullptr;
    }
}

size_t varDef::get_addr_in_mem(size_t posnow)
{
    // [TODO] char的考虑
    // 考虑对齐要求
    auto ceiling = [](int n, int x)
    {
        // x 必须是 2 的幂
        return (n + x - 1) & ~(x - 1);
    };
    if (type.getsize() > 1)
    {
        return ceiling(posnow, VCPU::size_word);
    }
    return posnow;
}
// [TODO] 重构
std::optional<varDef> varDef::makeByNode(ComplierParser::DeclarationContext* ast)
{
    // 检查参数有效性
    if (!ast || !ast->declarationSpecifiers() || !ast->initDeclaratorList())
    {
        return std::nullopt;
    }

    // 解析声明说明符（类型信息）
    Type type;
    bool isTypeValid = false;

    // 遍历所有声明说明符
    for (auto declSpec : ast->declarationSpecifiers()->declarationSpecifier())
    {
        // 只处理类型说明符
        if (declSpec->typeSpecifier())
        {
            auto typeSpec = declSpec->typeSpecifier();

            // 判断基本类型
            if (typeSpec->getText() == "int")
            {
                type.basic_type = Type::BasicType::Int;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "char")
            {
                type.basic_type = Type::BasicType::Char;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "void")
            {
                type.basic_type = Type::BasicType::Void;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "float")
            {
                type.basic_type = Type::BasicType::Float;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "double")
            {
                type.basic_type = Type::BasicType::Double;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "long")
            {
                type.basic_type = Type::BasicType::Long;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "short")
            {
                type.basic_type = Type::BasicType::Short;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "unsigned")
            {
                type.basic_type = Type::BasicType::Unsigned;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "signed")
            {
                type.basic_type = Type::BasicType::Signed;
                isTypeValid = true;
            }
        }
    }

    // 如果没有有效类型，返回空
    if (!isTypeValid)
    {
        return std::nullopt;
    }

    // 获取声明符（变量名和修饰符）
    auto initDeclList = ast->initDeclaratorList();
    if (initDeclList->initDeclarator().empty())
    {
        return std::nullopt;
    }

    // 我们处理第一个声明符（如果有多个，应在外部循环处理）
    auto initDecl = initDeclList->initDeclarator(0);
    if (!initDecl->declarator() || !initDecl->declarator()->directDeclarator())
    {
        return std::nullopt;
    }

    auto directDecl = initDecl->declarator()->directDeclarator();

    // 获取变量名
    std::string varName;
    
    // 处理多维数组和复杂声明的情况
    // 对于像 arr[12][13] 这样的声明，需要递归查找标识符
    antlr4::tree::ParseTree* currentNode = directDecl;
    
    // 尝试直接获取标识符
    if (directDecl->Identifier())
    {
        varName = directDecl->Identifier()->getText();
    }
    else
    {
        // 如果当前节点没有直接的标识符，尝试递归查找
        // 通常，数组声明的第一个子节点是另一个 directDeclarator
        if (directDecl->children.size() > 0)
        {
            // 递归查找，直到找到一个含有 Identifier 的 directDeclarator
            std::function<std::string(antlr4::tree::ParseTree*)> findIdentifier = 
                [&findIdentifier](antlr4::tree::ParseTree* node) -> std::string {
                    // 尝试将节点转换为 directDeclarator
                    if (auto dd = dynamic_cast<ComplierParser::DirectDeclaratorContext*>(node))
                    {
                        if (dd->Identifier())
                        {
                            return dd->Identifier()->getText();
                        }
                        // 如果这个节点没有标识符，但有子节点
                        if (!dd->children.empty())
                        {
                            // 检查第一个子节点
                            return findIdentifier(dd->children[0]);
                        }
                    }
                    return ""; // 没有找到标识符
                };
            
            varName = findIdentifier(directDecl);
            
            if (varName.empty())
            {
                return std::nullopt; // 没有找到标识符
            }
        }
        else
        {
            return std::nullopt; // 没有找到标识符
        }
    }

    // 处理指针
    if (initDecl->declarator()->pointer())
    {
        auto pointer = initDecl->declarator()->pointer();
        // 计算指针级别 - 直接从文本分析 * 的数量
        std::string pointerText = pointer->getText();
        type.pointer_level = std::count(pointerText.begin(), pointerText.end(), '*');
        type.kind = Type::Kind::Pointer;
    }

    // 处理数组
    // 检查directDeclarator是否有数组维度
    bool isArray = false;
    std::vector<int> dimensions;

    for (size_t i = 0; i < directDecl->children.size(); ++i)
    {
        if (i + 3 <= directDecl->children.size() && directDecl->children[i]->getText() == "[" &&
            directDecl->children[i + 2]->getText() == "]")
        {
            isArray = true;

            // 尝试获取数组大小
            auto sizeExpr = directDecl->children[i + 1];
            int dimension = -1; // 默认为未指定大小

            if (auto constExpr = dynamic_cast<ComplierParser::ConstantExpressionContext*>(sizeExpr))
            {
                // 尝试从常量表达式中提取整数值
                try
                {
                    // 后续支持常量表达式
                    dimension = std::stoi(constExpr->getText());
                }
                catch (...)
                {
                    // 转换失败，保持默认值
                }
            }

            dimensions.push_back(dimension);
            i += 2; // 跳到 ']' 后继续检查下一个维度
        }
    }

    // 如果是数组，设置数组信息
    if (isArray)
    {
        type.kind = Type::Kind::Array;
        type.array_info = Type::ArrayInfo{};
        type.array_info->dimensions = dimensions;
    }

    // 创建变量定义
    varDef var;
    var.name = varName;
    var.type = type;
    var.is_defined = true;
    var.addr = 0; // 初始地址，将在后续分配

    // 处理初始值（如果有）
    // 注意：这里只是标记有初始化器，实际值需要在代码生成阶段处理
    if (initDecl->initializer())
    {
        // 这里可以添加初始化器处理逻辑
    }

    return var;
}
std::optional<funcDef> funcDef::makeByNode(ComplierParser::DeclarationContext* ast)
{
    // 检查参数有效性
    if (!ast || !ast->declarationSpecifiers() || !ast->initDeclaratorList())
    {
        return std::nullopt;
    }

    // 解析返回类型
    Type returnType;
    bool isTypeValid = false;

    // 遍历所有声明说明符
    for (auto declSpec : ast->declarationSpecifiers()->declarationSpecifier())
    {
        // 只处理类型说明符
        if (declSpec->typeSpecifier())
        {
            auto typeSpec = declSpec->typeSpecifier();

            // 判断基本类型
            if (typeSpec->getText() == "int")
            {
                returnType.basic_type = Type::BasicType::Int;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "char")
            {
                returnType.basic_type = Type::BasicType::Char;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "void")
            {
                returnType.basic_type = Type::BasicType::Void;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "float")
            {
                returnType.basic_type = Type::BasicType::Float;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "double")
            {
                returnType.basic_type = Type::BasicType::Double;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "long")
            {
                returnType.basic_type = Type::BasicType::Long;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "short")
            {
                returnType.basic_type = Type::BasicType::Short;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "unsigned")
            {
                returnType.basic_type = Type::BasicType::Unsigned;
                isTypeValid = true;
            }
            else if (typeSpec->getText() == "signed")
            {
                returnType.basic_type = Type::BasicType::Signed;
                isTypeValid = true;
            }
        }
    }

    // 如果没有有效类型，返回空
    if (!isTypeValid)
    {
        return std::nullopt;
    }

    // 获取函数声明符
    auto initDeclList = ast->initDeclaratorList();
    if (initDeclList->initDeclarator().empty())
    {
        return std::nullopt;
    }

    auto initDecl = initDeclList->initDeclarator(0);
    if (!initDecl->declarator() || !initDecl->declarator()->directDeclarator())
    {
        return std::nullopt;
    }

    auto directDecl = initDecl->declarator()->directDeclarator();

    // 检查是否为函数声明（包含参数列表）
    bool isFunction = false;
    std::string funcName;
    std::vector<varDef> parameters;
    bool isVariadic = false;

    // 首先获取函数名
    // 处理复杂声明的情况
    if (directDecl->Identifier())
    {
        funcName = directDecl->Identifier()->getText();
    }
    else
    {
        // 如果当前节点没有直接的标识符，尝试递归查找
        std::function<std::string(antlr4::tree::ParseTree*)> findIdentifier = 
            [&findIdentifier](antlr4::tree::ParseTree* node) -> std::string {
                // 尝试将节点转换为 directDeclarator
                if (auto dd = dynamic_cast<ComplierParser::DirectDeclaratorContext*>(node))
                {
                    if (dd->Identifier())
                    {
                        return dd->Identifier()->getText();
                    }
                    // 如果这个节点没有标识符，但有子节点
                    if (!dd->children.empty())
                    {
                        // 检查第一个子节点
                        return findIdentifier(dd->children[0]);
                    }
                }
                return ""; // 没有找到标识符
            };
        
        funcName = findIdentifier(directDecl);
        
        if (funcName.empty())
        {
            return std::nullopt; // 没有找到标识符
        }
    }

    // 检查是否有参数列表
    for (size_t i = 0; i < directDecl->children.size(); ++i)
    {
        if (i + 2 < directDecl->children.size() && directDecl->children[i]->getText() == "(" &&
            directDecl->children[i + 2]->getText() == ")")
        {

            isFunction = true;

            // 检查是否有参数
            auto paramCtx = directDecl->children[i + 1];
            if (auto paramList = dynamic_cast<ComplierParser::ParameterTypeListContext*>(paramCtx))
            {
                // 处理参数列表
                if (paramList->parameterList())
                {
                    for (auto paramDecl : paramList->parameterList()->parameterDeclaration())
                    {
                        // 为每个参数创建一个变量定义
                        varDef param;

                        // 获取参数类型
                        if (paramDecl->declarationSpecifiers())
                        {
                            Type paramType;
                            for (auto declSpec :
                                 paramDecl->declarationSpecifiers()->declarationSpecifier())
                            {
                                if (declSpec->typeSpecifier())
                                {
                                    auto typeSpec = declSpec->typeSpecifier();
                                    if (typeSpec->getText() == "int")
                                    {
                                        paramType.basic_type = Type::BasicType::Int;
                                    }
                                    else if (typeSpec->getText() == "char")
                                    {
                                        paramType.basic_type = Type::BasicType::Char;
                                    }
                                    else if (typeSpec->getText() == "void")
                                    {
                                        paramType.basic_type = Type::BasicType::Void;
                                    }
                                    else if (typeSpec->getText() == "float")
                                    {
                                        paramType.basic_type = Type::BasicType::Float;
                                    }
                                    else if (typeSpec->getText() == "double")
                                    {
                                        paramType.basic_type = Type::BasicType::Double;
                                    }
                                    else if (typeSpec->getText() == "long")
                                    {
                                        paramType.basic_type = Type::BasicType::Long;
                                    }
                                    else if (typeSpec->getText() == "short")
                                    {
                                        paramType.basic_type = Type::BasicType::Short;
                                    }
                                    else if (typeSpec->getText() == "unsigned")
                                    {
                                        paramType.basic_type = Type::BasicType::Unsigned;
                                    }
                                    else if (typeSpec->getText() == "signed")
                                    {
                                        paramType.basic_type = Type::BasicType::Signed;
                                    }
                                }
                            }

                            // 处理参数修饰符（指针等）
                            if (paramDecl->declarator())
                            {
                                if (paramDecl->declarator()->pointer())
                                {
                                    auto pointer = paramDecl->declarator()->pointer();
                                    // 计算指针级别 - 直接从文本分析 * 的数量
                                    paramType.pointer_level = 0;
                                    std::string pointerText = pointer->getText();
                                    paramType.pointer_level =
                                        std::count(pointerText.begin(), pointerText.end(), '*');
                                    paramType.kind = Type::Kind::Pointer;
                                }

                                // 获取参数名
                                if (paramDecl->declarator()->directDeclarator() &&
                                    paramDecl->declarator()->directDeclarator()->Identifier())
                                {
                                    param.name = paramDecl->declarator()
                                                     ->directDeclarator()
                                                     ->Identifier()
                                                     ->getText();
                                }
                                else
                                {
                                    param.name = ""; // 匿名参数
                                }
                            }

                            param.type = paramType;
                            param.is_defined = true;
                            param.addr = 0; // 初始地址，将在后续分配
                            parameters.push_back(param);
                        }
                    }
                }

                // 检查是否有可变参数（...）
                isVariadic =
                    paramList->children.size() >= 3 &&
                    paramList->children[paramList->children.size() - 2]->getText() == "," &&
                    paramList->children[paramList->children.size() - 1]->getText() == "...";
            }
            break;
        }
    }

    // 如果不是函数，返回空
    if (!isFunction)
    {
        return std::nullopt;
    }

    // 处理返回类型的指针部分
    if (initDecl->declarator()->pointer())
    {
        auto pointer = initDecl->declarator()->pointer();
        // 计算指针级别 - 直接从文本分析 * 的数量
        std::string pointerText = pointer->getText();
        returnType.pointer_level = std::count(pointerText.begin(), pointerText.end(), '*');
        returnType.kind = Type::Kind::Pointer;
    }

    // 创建函数类型信息
    returnType.kind = Type::Kind::Function;
    returnType.func_info = Type::FunctionInfo{};
    for (const auto& param : parameters)
    {
        returnType.func_info->param_types.push_back(param.type);
    }
    returnType.func_info->is_variadic = isVariadic;

    // 创建函数定义
    funcDef func;
    func.name = funcName;
    func.type = returnType;
    func.is_defined = false; // 这只是一个声明，不是定义
    func.addr = 0;
    func.args = parameters;
    func.max_stack_size = VCPU::size_word * 2; // 初始堆栈大小（为旧BP和返回地址预留空间）
    func.stack_size_now = VCPU::size_word * 2;

    return func;
}
std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths)
{
    std::vector<OBJ> objs;
    for (auto each : paths)
    {
        // 创建输入流
        Preprocessor{}.process(each);
        std::ifstream in(each + ".pre");
        std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        antlr4::ANTLRInputStream inputStream(input);
        // 创建词法分析器
        ComplierLexer lexer(&inputStream);
        antlr4::CommonTokenStream tokens(&lexer);
        // 创建语法分析器
        ComplierParser parser(&tokens);
        // 使用正确的入口规则
        auto tree = parser.compilationUnit();
        // 创建和使用自定义访问器
        astVisitor visitor{each};
        auto ret = std::any_cast<bool>(visitor.visitCompilationUnit(tree));
        if (ret)
        {
            objs.push_back(visitor.obj);
        }
        else
        {
            std::cout << "fail: " << each << '\n';
        }
    }
    linker linker{objs};
    return linker.exe.asms;
}
