#include "complier.hpp"
#include "ASM.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "obj.h"
#include "preprocessor.hpp"
#include "tools.hpp"
#include <algorithm>
#include <antlr4-runtime/antlr4-runtime.h>
#include <astVisit.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <regex>
#include <utility>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <cstdio>
size_t Type::getsize() const
{
    switch (kind)
    {
    case Kind::Pointer:
        return VCPU<>::size_word;
    case Kind::Basic:
        switch (basic_type)
        {
        case BasicType::Char:
            return 1;
        case BasicType::Short:
            return VCPU<>::size_word / 4;
        case BasicType::Int:
            return VCPU<>::size_word / 2;
        case BasicType::Long:
            return VCPU<>::size_word;
        default:
            break;
        }
        break;
    case Kind::Array:
        return static_cast<size_t>(arr_num) * subType->getsize();
    case Kind::Function:
        return VCPU<>::size_word;
    case Kind::Struct:
        return align_up(this->structInfo.members.back().second.addr +
                            this->structInfo.members.back().second.type.getsize(),
                        8);
    case Kind::Undefined:
        break;
    }
    throw;
}

bool Type::operator==(const Type& other_) const
{
    auto one = this;
    auto other = &other_;
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
            if (one->args[i].type != other->args[i].type)
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
    if (kind_ == Type::Kind::Array) {
        arr_num = arg_;
    }
    // Pointer 不再使用 arr_num，通过 subType 嵌套表示
}

Type::Type(Kind kind_, std::string arg_)
{
    kind = kind_;
    structID = std::move(arg_);
}

Type::Type(Kind kind_, std::vector<IDdef> args_)
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
    if (top.kind == Type::Kind::Undefined)
    {
        top = what;
    }
    else
    {
        top.subType = value_ptr<Type>::make_value_ptr(what);
    }
    return true;
}

Type Type::popTop()
{
    Type* p = nullptr;
    Type* now = this;
    while (now->kind != Type::Kind::Basic && now->kind != Type::Kind::Undefined && now->subType)
    {
        p = now;
        now = now->subType.get();
    }
    auto tp = *p->subType;
    p->subType.reset();
    return tp;
}

//[REWRITE] 修改 to_string() 方法
std::string Type::to_string() const
{
    return "";
}

std::expected<size_t, error> linker::pushfunc(std::string funcname)
{
    // [TODO] globalvar@name 的链接
    IDdef* func = nullptr;
    auto ret = addrmap.find("func@" + funcname);
    if (ret == addrmap.end()) // 重定向表未记录
    {
        bool flag = false;
        for (auto& eachobj : objs)
        {
            auto fun = eachobj.symbol_table.globaldef.find(funcname);
            if (fun == eachobj.symbol_table.globaldef.end())
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
                    for (auto& eachasms : func->funcInfo.asms)
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
                        std::regex pattern("globalvar@([a-zA-Z_][a-zA-Z0-9_@#]*)");
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
        for (auto& [name, eachgvar] : eachobj.symbol_table.globaldef)
        {
            if (eachgvar.type.kind == Type::Kind::Function)
            {
                continue;
            }
            if (eachgvar.type.kind == Type::Kind::Array)
            {
                eachgvar.addr = exe.global_size; // 数组值在内存底端, 指针指向整形低地址
            }
            else
            {
                eachgvar.addr = eachgvar.get_addr_in_stack(exe.global_size);
            }
            exe.global_size = eachgvar.addr + eachgvar.type.getsize();
            const bool is_static = eachgvar.storageClassSpecifier == StorageClassSpecifier::Static;
            auto labal =
                std::string{"globalvar@"} + (is_static ? eachobj.name + "@" : "") + eachgvar.name;
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
        auto ret = eachobj.symbol_table.globaldef.find("__global_init" + eachobj.name);
        if (ret == eachobj.symbol_table.globaldef.end())
        {
            return std::unexpected(error::undifined_obj_init_fun);
        }
        for (auto& eachasm : ret->second.funcInfo.asms)
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

size_t IDdef::get_addr_in_stack(size_t posnow)
{
    // [TODO] char的考虑
    // 考虑对齐要求
    auto ceiling = [](int n, int x)
    {
        // x 必须是 2 的幂
        return (n + x - 1) & ~(x - 1);
    };
    if (type.getsize() >= 1)
    {
        return ceiling(posnow + this->type.getsize(), VCPU<>::size_word); // 姑且对齐到size_word
    }
    return posnow;
}
bool IDdef::operator==(const IDdef& that) const
{
    return this->type == that.type && this->name == that.name &&
           this->storageClassSpecifier == that.storageClassSpecifier;
}
std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths,
                                                                 bool showASt, int tolerate,
                                                                 bool showFoldedNames)
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
        ComplierParser::CompilationUnitContext* tree = parser.compilationUnit();
        // fordebug
        if (showASt)
        {
            std::cout << each << ": \n";
            printAST(tree, tolerate, showFoldedNames);
            std::cout << "\n";
        }
        // 创建和使用自定义访问器
        astVisitor visitor{each, obj};
        visitor.visitCompilationUnit(tree);
        obj.flush_global_decls();
        objs.push_back(std::move(obj));
    }
    linker linker{objs};
    return linker.process();
}

void complier::printAST(antlr4::tree::ParseTree* tree, int tolerate, bool showFoldedNames)
{
    // 使用递归打印 ASCII 树，在支持的终端上为终端文本加高亮
    bool use_color = false;
#if defined(__unix__) || defined(__APPLE__)
    use_color = isatty(fileno(stdout));
#endif
    const std::string col_start = "\x1b[1;33m"; // 粗体黄色
    const std::string col_end = "\x1b[0m";

    // 辅助：只打印单行节点（不递归）
    auto printSingleLine =
        [&](antlr4::tree::ParseTree* node, const std::string& prefix, bool isLast)
    {
        std::string name;
        bool term = dynamic_cast<antlr4::tree::TerminalNode*>(node) != nullptr;
        if (term)
        {
            name = "Terminal";
        }
        else if (auto ctx = dynamic_cast<antlr4::ParserRuleContext*>(node))
        {
            size_t idx = ctx->getRuleIndex();
            try
            {
                ComplierParser::initialize();
                ComplierParser parser(nullptr);
                const auto& names = parser.getRuleNames();
                if (idx < names.size())
                    name = names[idx];
                else
                    name = std::to_string(idx);
            }
            catch (...)
            {
                name = std::to_string(idx);
            }
        }
        else
        {
            name = "Unknown";
        }

        if (prefix.empty())
            std::cout << name;
        else
            std::cout << prefix << (isLast ? "└─ " : "├─ ") << name;
        if (term)
        {
            std::cout << " : ";
            if (use_color)
                std::cout << col_start << node->getText() << col_end;
            else
                std::cout << node->getText();
        }
        std::cout << std::endl;
    };

    auto inner = [&](auto&& self, antlr4::tree::ParseTree* node, const std::string& prefix,
                     bool isLast, int parentChildCount, int curDepth,
                     bool suppressDescend = false) -> void
    {
        std::string nodename;
        bool isTerminal = dynamic_cast<antlr4::tree::TerminalNode*>(node) != nullptr;
        if (isTerminal)
        {
            nodename = "Terminal";
        }
        else if (auto ctx = dynamic_cast<antlr4::ParserRuleContext*>(node))
        {
            size_t idx = ctx->getRuleIndex();
            try
            {
                ComplierParser::initialize();
                ComplierParser parser(nullptr);
                const auto& names = parser.getRuleNames();
                if (idx < names.size())
                {
                    nodename = names[idx];
                }
                else
                {
                    nodename = std::to_string(idx);
                }
            }
            catch (...)
            {
                nodename = std::to_string(idx);
            }
        }
        else
        {
            nodename = "Unknown";
        }

        // std::cout << "// ";
        if (prefix.empty())
        {
            std::cout << nodename;
        }
        else
        {
            std::cout << prefix << (isLast ? "└─ " : "├─ ") << nodename;
        }
        if (isTerminal)
        {
            std::cout << " : ";
            if (use_color)
            {
                std::cout << col_start << node->getText() << col_end;
            }
            else
            {
                std::cout << node->getText();
            }
        }
        std::cout << std::endl;

        if (node->children.empty() || suppressDescend)
            return;

        for (size_t i = 0; i < node->children.size(); ++i)
        {
            bool last = (i + 1 == node->children.size());
            std::string childPrefix = prefix + (isLast ? "    " : "│   ");

            // 折叠逻辑: 当达到阈值且当前节点无兄弟节点(parentChildCount==1)
            // 并且子节点形成单链（每个节点只有一个子节点），且不穿过 Terminal 节点时，合并为 ...(m)
            // 折叠判断：若父节点仅有该子节点且折叠链长度超过容忍值，则合并
            if (tolerate != INT_MAX && parentChildCount == 1)
            {
                // 收集从 child 开始的单链节点
                std::vector<antlr4::tree::ParseTree*> chain_nodes;
                antlr4::tree::ParseTree* p = node->children[i];
                if (!dynamic_cast<antlr4::tree::TerminalNode*>(p))
                {
                    while (true)
                    {
                        chain_nodes.push_back(p);
                        if (p->children.size() == 1 &&
                            !dynamic_cast<antlr4::tree::TerminalNode*>(p->children[0]))
                        {
                            p = p->children[0];
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                int chain_len = (int)chain_nodes.size();
                if (chain_len > 0 && chain_len > tolerate)
                {
                    int keep = tolerate / 2; // 保留头尾各 keep 个节点
                    int head = keep;
                    int tail = keep;
                    int collapsed = chain_len - head - tail;

                    // 打印 head 部分（只打印行，不递归），正确传递 isLast 以保持竖线
                    std::string pfx = childPrefix;
                    for (int j = 0; j < head; ++j)
                    {
                        bool localIsLast;
                        if (j == 0)
                            localIsLast = last; // 第一个 head 节点使用上层的 last
                        else
                            localIsLast = true; // 链中其余节点均为其父的唯一子节点，视为 last

                        self(self, chain_nodes[j], pfx, localIsLast, 1, curDepth + 1 + j, true);
                        pfx += (localIsLast ? "    " : "│   ");
                    }

                    // 打印折叠占位
                    if (showFoldedNames)
                    {
                        // 收集被折叠节点的名字
                        std::string foldedPath;
                        for (int j = head; j < chain_len - tail; ++j)
                        {
                            if (!foldedPath.empty())
                                foldedPath += "/";
                            if (auto ctx = dynamic_cast<antlr4::ParserRuleContext*>(chain_nodes[j]))
                            {
                                size_t idx = ctx->getRuleIndex();
                                ComplierParser::initialize();
                                ComplierParser parser(nullptr);
                                const auto& names = parser.getRuleNames();
                                if (idx < names.size())
                                    foldedPath += names[idx];
                                else
                                    foldedPath += std::to_string(idx);
                            }
                        }
                        std::cout << pfx << "└─ " << foldedPath << "(" << collapsed << ")"
                                  << std::endl;
                    }
                    else
                    {
                        std::cout << pfx << "└─ " << "...(" << collapsed << ")" << std::endl;
                    }

                    // 打印 tail 部分（只打印行，不递归），然后对子节点展开递归
                    if (tail > 0)
                    {
                        int tail_start = chain_len - tail;
                        // 为 tail 部分构造初始前缀，折叠占位后缩进
                        std::string tailpfx = pfx + "    ";
                        for (int j = tail_start; j < chain_len; ++j)
                        {
                            // 单链中每个节点都是唯一子节点，始终用 └─
                            bool isLastForThis = true;
                            self(self, chain_nodes[j], tailpfx, isLastForThis, 1, curDepth + 1 + j,
                                 true);
                            tailpfx += "    ";
                        }
                        // 递归 chain_nodes.back() 的子节点
                        auto nodeToRecurse = chain_nodes.back();
                        for (size_t ci = 0; ci < nodeToRecurse->children.size(); ++ci)
                        {
                            bool childLast = (ci + 1 == nodeToRecurse->children.size());
                            self(self, nodeToRecurse->children[ci], tailpfx, childLast,
                                 nodeToRecurse->children.size(), curDepth + chain_len + 1, false);
                        }
                    }
                    else
                    {
                        // tail==0, 直接对 p 的子节点递归
                        std::string pfx2 = pfx;
                        for (size_t ci = 0; ci < p->children.size(); ++ci)
                        {
                            bool childLast = (ci + 1 == p->children.size());
                            self(self, p->children[ci], pfx2, childLast, p->children.size(),
                                 curDepth + chain_len + 1, false);
                        }
                    }
                    continue;
                }
            }

            self(self, node->children[i], childPrefix, last, node->children.size(), curDepth + 1,
                 false);
        }
    };

    inner(inner, tree, "", true, 1, 0, false);
}