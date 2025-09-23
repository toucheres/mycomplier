#include "complier.hpp"
#include "ASM.hpp"
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
size_t Type::getsize() const
{
    if (this->kind == Kind::Pointer)
    {
        return VCPU<>::size_word;
    }
    else if (this->kind == Kind::Basic)
    {
        if (this->basic_type == Type::BasicType::Char)
        {
            return 1;
        }
        else if (this->basic_type == Type::BasicType::Int)
        {
            return VCPU<>::size_word / 2;
        }
        else if (this->basic_type == Type::BasicType::Long)
        {
            return VCPU<>::size_word;
        }
    }
    else if (this->kind == Kind::Array)
    {
        return this->arr_or_ptr_num * subType->getsize();
    }
    else if (this->kind == Kind::ID)
    {
        return subType->getsize();
    }
    else if (this->kind == Kind::Function)
    {
        return VCPU<>::size_word;
    }
}

bool Type::operator==(const Type& other_) const
{
    auto one = this;
    auto other = &other_;
    if (one->kind == Kind::ID)
    {
        one = one->subType.get();
    }
    if (other->kind == Kind::ID)
    {
        other = other->subType.get();
    }
    if (other->kind != one->kind)
    {
        return false;
    }
    auto& dkind = one->kind;
    if (dkind == Type::Kind::Basic)
    {
        return one->basic_type == other->basic_type;
    }
    else if (dkind == Type::Kind::Array || dkind == Type::Kind::Pointer)
    {
        return one->args == other->args && *one->subType == *other->subType;
    }
    else if (dkind == Type::Kind::Function)
    {
        if (one->args.size() != other->args.size())
        {
            return false;
        }
        for (size_t i = 0; i < one->args.size(); i++)
        {
            if (one->args[i] != other->args[i])
            {
                return false;
            }
        }
        return one->basic_type == other->basic_type;
    }
    else if (dkind == Type::Kind::Undefined)
    {
        return false;
    }
    return false;
}

Type::Type(Kind kind_, BasicType arg_)
{
    assert(kind_ == Type::Kind::Basic);
    kind = kind_;
    basic_type = arg_;
}

Type::Type(Kind kind_, int arg_)
{
    assert(kind_ == Type::Kind::Array || kind_ == Type::Kind::Pointer);
    kind = kind_;
    arr_or_ptr_num = arg_;
}

Type::Type(Kind kind_, std::string arg_)
{
    assert(kind_ == Type::Kind::ID);
    kind = kind_;
    id = arg_;
}

Type::Type(Kind kind_, std::vector<Type> args_)
{
    assert(kind_ == Type::Kind::Function);
    kind = kind_;
    args = args_;
}

Type& Type::getTop()
{
    Type* now = this;
    while (now->kind != Type::Kind::Basic && now->kind != Type::Kind::Undefined && now->subType)
    {
        now = now->subType.get();
    }
    return *now;
}

bool Type::pushTop(const Type& what)
{
    auto& top = getTop();
    top.subType = copyed_ptr<Type>::make_copyed_ptr(what);
    return true;
}

//[REWRITE] 修改 to_string() 方法
std::string Type::to_string() const
{
    return "";
}

std::expected<size_t, error> linker::pushfunc(std::string funcname)
{
    // [TODO] globalvar@name 的链接
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
                    for (int i = thisfuncstart; i < this->exe.asms.size(); i++)
                    {
                        std::regex pattern("globalvar@([a-zA-Z_][a-zA-Z0-9_]*)");
                        std::smatch match;
                        if (std::regex_search(this->exe.asms[i], match, pattern) &&
                            match.size() > 1)
                        {
                            auto funnametoreaddr = match[1].str(); // 返回第一个捕获组
                            size_t pos = match.position(0);
                            size_t len = match.length(0);
                            auto it = addrmap.find("globalvar@" + funnametoreaddr);
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
        for (auto& [name, eachgvar] : eachobj.symbol_table.globalvardef)
        {
            eachgvar.addr = eachgvar.get_addr_in_stack(exe.global_size);
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
    // 拼接obj初始化函数
    for (auto& eachobj : objs)
    {
        auto ret = eachobj.symbol_table.globalfuncdef.find("__global_init" + eachobj.name);
        if (ret == eachobj.symbol_table.globalfuncdef.end())
        {
            return std::unexpected(error::undifined_obj_init_fun);
        }
        for (auto& eachasm : ret->second.asms)
        {
            pushfunc("__global_init" + eachobj.name);
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
//     return VCPU<>::size_word;
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
    // 使用反向迭代器
    for (auto it = funcvar_stack.rbegin(); it != funcvar_stack.rend(); ++it)
    {
        if ((*it).empty())
        {
            continue;
        }
        else
        {
            stack_size_now = -it->back().addr;
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
    var.addr = -var.get_addr_in_stack(stack_size_now);
    var.is_defined = true;
    stack_size_now += var.type.getsize();
    max_stack_size = std::max(stack_size_now, max_stack_size);
    funcvar_stack.back().push_back(var);
    return &funcvar_stack.back().back();
}

bool funcDef::add_arg(std::vector<varDef>& vardef)
{
    // argn ... arg1  ret oldbp localvar1
    //           +16   +8    0       -8
    args = vardef;
    for (int i = 0; i < vardef.size(); i++)
    {
        args[i].addr = VCPU<>::size_word * (i + 2);
    }
    return true;
}

size_t varDef::get_addr_in_stack(size_t posnow)
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
        return ceiling(posnow + this->type.getsize(), VCPU<>::size_word); // 姑且对齐到size_word
    }
    return posnow;
}
std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths)
{
    std::vector<OBJ> objs;
    for (auto each : paths)
    {
        OBJ obj;
        obj.name = each;
        // 创建输入流
        Preprocessor{}.process(each);
        std::ifstream in(each + ".pre");
        std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        antlr4::ANTLRInputStream inputStream(input);
        // 创建词法分析器
        ComplierLexer lexer(&inputStream);
        antlr4::CommonTokenStream tokens(&lexer);
        // 创建自定义语法分析器
        ComplierParser parser(&tokens);
        // 使用正确的入口规则
        auto tree = parser.compilationUnit();
        // 创建和使用自定义访问器
        astVisitor visitor{each, obj};
        visitor.visitCompilationUnit(tree);
        objs.push_back(visitor.obj);
    }
    linker linker{objs};
    return linker.process();
}
