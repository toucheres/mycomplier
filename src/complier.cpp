#include "complier.hpp"
#include "ASM.hpp"
#include "CParser.h"
#include "obj.h"
#include "settings.h"
#include <antlr4-runtime/antlr4-runtime.h>
#include <astVisit.h>
#include <iostream>
#include <regex>
#include <utility>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include <cstdio>
size_t linker::pushfunc(std::string funcname)
{
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
                    THROW_ERR_NOCTX_INFO(error::double_defined, funcname);
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
                                this->exe.asms[i].replace(pos, len, std::to_string(ret));
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
                            auto varname = match[1].str(); // 返回第一个捕获组
                            size_t pos = match.position(0);
                            size_t len = match.length(0);
                            auto it = addrmap.find("globalvar@" + varname);
                            if (it != addrmap.end())
                            {
                                // 替换为实际地址
                                this->exe.asms[i].replace(pos, len, std::to_string(it->second));
                            }
                            else
                            {
                                THROW_ERR_NOCTX_INFO(error::undefined_symbol, varname);
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
        if (!flag)
        {
            THROW_ERR_NOCTX_INFO(error::undefined_symbol, funcname);
        }
    }
    // 重定向表已记录
    return addrmap["func@" + funcname];
}
std::vector<std::string> linker::process()
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
                THROW_ERR_NOCTX_INFO(error::double_defined, labal);
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
            THROW_ERR_NOCTX_INFO(error::undifined_obj_init_fun, "__global_init" + eachobj.name);
        }
        for (auto& eachasm : ret->second.funcInfo.asms)
        {
            pushfunc("__global_init" + eachobj.name);
        }
    }

    int argn = mainargs.size() + 1;
    char** argv = (char**)malloc((argn + 1) * sizeof(char*));
    argv[0] = ""; // [TODO]
    for (int i = 0; i < mainargs.size(); i++)
    {
        argv[i + 1] = (char*)malloc(mainargs[i].length() + 1);
        strcpy(argv[i + 1], mainargs[i].c_str());
        argv[i + 1][mainargs[i].length()] = '\0';
    }

    this->exe.asms.push_back(ASM{ASM::basic_asm::IMM, (long)argv});          // argv
    this->exe.asms.push_back(ASM{ASM::basic_asm::IMM, argn});                // argn
    this->exe.asms.push_back(ASM{ASM::basic_asm::IMM, exe.asms.size() + 3}); // call main
    this->exe.asms.push_back(ASM{ASM::basic_asm::CALL});                     // call main
    this->exe.asms.push_back(ASM{ASM::basic_asm::EXIT});
    // 推入main函数, 并重定向func@
    pushfunc("main");
    return exe.asms;
}

void complier::printAST(antlr4::tree::ParseTree* tree)
{
    // read config from settings singleton
    using namespace app_options;
    auto& tvm = settings::tvm();
    int tolerate = tvm.get(app_options::tolerate).or_(4);
    bool showFoldedNames = tvm.get(app_options::show_folded_names).or_(false);

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
                // CParser::initialize(); // [TIME]
                static CParser parser(nullptr);
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
                // CParser::initialize();
                static CParser parser(nullptr); // [TIME]
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
                                // CParser::initialize();
                                static CParser parser(nullptr);
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

// 新的重载：从 settings 单例读取运行时选项并调用完整实现
std::vector<std::string> complier::process(std::vector<std::string> paths)
{
    using namespace app_options;
    auto& tvm = settings::tvm();
    bool showASt = tvm.get(app_options::show_ast).or_(false);
    int tolerate = tvm.get(app_options::tolerate).or_(4);
    bool showFoldedNames = tvm.get(app_options::show_folded_names).or_(false);
    bool disableStd = tvm.get(app_options::disable_std).or_(false);
    bool preprocess_only = tvm.get(app_options::preprocess_only).or_(false);
    std::vector<std::string> mainargs;
    try
    {
        mainargs = tvm.get_direct(main_args);
    }
    catch (...)
    {
        mainargs = {};
    }
    return process(std::move(paths), showASt, tolerate, showFoldedNames, disableStd, mainargs,
                   preprocess_only);
}