#include "complier.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "preprocessor.hpp"
#include <antlr4-runtime/antlr4-runtime.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <regex>
#include <astVisit.h>
// 修改 getsize() 方法以支持新的类型表示
size_t Type::getsize() const
{
    if (is_array() && array_info.has_value())
    {
        // 数组大小 = 元素大小 * 元素数量
        int element_size =
            (this->basic_type == BasicType::Char && pointer_level == 0) ? 1 : VCPU::size_word;
        return array_info->size > 0 ? element_size * array_info->size : VCPU::size_word;
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
        result += "[";
        if (array_info->size > 0)
        {
            result += std::to_string(array_info->size);
        }
        result += "]";
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

varDef::varDef(std::shared_ptr<peg::Ast> astnode)
{
    if (!astnode)
    {
        throw;
    }
    auto& node = *astnode;
    while (1)
    {
        // 找到最内层
        if (astnode->name == "Identifier")
        {
            name = astnode->token_to_string();
            this->type = Type{astnode};
            this->is_defined = true;
            break;
        }
        if (astnode->name == "idDecl")
        {
            astnode = astnode->nodes[1];
            continue;
        }
        if (astnode->name == "Declarator")
        {
            astnode = astnode->nodes[astnode->nodes.size() - 1];
            continue;
        }
        astnode = astnode->nodes[0];
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

std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths)
{
    // 创建输入流
    std::ifstream in("/home/toucher/vscoderope/mycomplier/test/test1.c.pre");
    std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    antlr4::ANTLRInputStream inputStream(input);
    // antlr4::ANTLRInputStream inputStream(input);

    // 创建词法分析器
    ComplierLexer lexer(&inputStream);
    antlr4::CommonTokenStream tokens(&lexer);

    // 创建语法分析器
    ComplierParser parser(&tokens);

    // 使用正确的入口规则 - 根据你的语法确定
    // 可能是 translationUnit、compilationUnit 或 expression
    auto tree = parser.compilationUnit(); // 替换成你语法的入口规则
    std::cout << "ast: \n";
    // 在 main 函数中调用：
    // 创建和使用自定义访问器
    ComplierVisitor visitor{};
    visitor.visitCompilationUnit(tree);
}
