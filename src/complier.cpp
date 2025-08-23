#include "complier.hpp"
#include <filesystem>
// 递归遍历 AST 的辅助函数
void visit_ast(const std::shared_ptr<peg::Ast>& ast, int depth = 0)
{
    // 打印当前节点
    std::string indent(depth * 2, ' ');
    std::cout << indent << "- " << ast->name;

    // 如果是 token（有具体值）
    if (ast->is_token)
    {
        std::cout << " [" << ast->token << "]";
        std::cout << " (位置: " << ast->line << ":" << ast->column << ")";
    }
    std::cout << "\n";

    // 递归访问所有子节点
    for (const auto& child : ast->nodes)
    {
        visit_ast(child, depth + 1);
    }
}
std::shared_ptr<peg::Ast> simplify_ast(std::shared_ptr<peg::Ast> ast)
{
    if (!ast)
        return ast;

    // 先递归处理所有子节点
    for (size_t i = 0; i < ast->nodes.size(); i++)
    {
        ast->nodes[i] = simplify_ast(ast->nodes[i]);
    }

    // 表达式路径压缩优化
    // if (ast->name == "Assignment" || ast->name == "Conditional" || ast->name == "LogicalOr" ||
    //     ast->name == "LogicalAnd" || ast->name == "BitwiseOr" || ast->name == "BitwiseXor" ||
    //     ast->name == "BitwiseAnd" || ast->name == "Equality" || ast->name == "Relational" ||
    //     ast->name == "Shift" || ast->name == "Additive" || ast->name == "Postfix" ||
    //     ast->name == "Multiplicative" || ast->name == "Cast" || ast->name == "Unary")
    if (ast->name == "Assignment" || ast->name == "Conditional" || ast->name == "LogicalOr" ||
        ast->name == "LogicalAnd" || ast->name == "BitwiseOr" || ast->name == "BitwiseXor" ||
        ast->name == "BitwiseAnd" || ast->name == "Equality" || ast->name == "Relational" ||
        ast->name == "Shift" || ast->name == "Additive" || ast->name == "Multiplicative" ||
        ast->name == "Cast" || ast->name == "Unary" || ast->name == "Postfix" ||
        ast->name == "Primary")
    {

        // 如果只有一个子节点，直接返回该子节点
        if (ast->nodes.size() == 1)
        {
            return ast->nodes[0];
        }
    }

    return ast;
}
std::expected<bool, error> OBJ::generate_code()
{
    visit_ast(this->program);
    return generate_code(this->program, this->symbol_table.lookup_fun("__global_init_" + name), 0);
}
// [TODO] 修正generate_expression
std::expected<bool, error> OBJ::generate_expression(std::shared_ptr<peg::Ast> expr, funcDef* func)
{
    if (!expr)
    {
        return std::unexpected(error::empty_node);
    }

    auto& node = *expr;

    if (node.name == "Expression")
    {
        return generate_expression(node.nodes[0], func);
    }
    else if (node.name == "Number")
    {
        // 数字字面量处理 - 将值压栈
        int value = std::stoi(node.token_to_string());
        func->asms.push_back(ASM{ASM::basic_asm::IMM, value});
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return true;
    }
    else if (node.name == "Identifier")
    {
        // 变量引用处理
        std::string var_name = node.token_to_string();
        const varDef* var = func->lookup_var(var_name);
        if (!var)
        {
            var = symbol_table.lookup_var(var_name);
            if (!var)
            {
                std::cerr << "未定义的变量: " << var_name << std::endl;
                return false;
            }
            // 全局变量
            func->asms.push_back(ASM{ASM::basic_asm::IMM, var->addr});
        }
        else
        {
            // 局部变量
            func->asms.push_back(ASM{ASM::basic_asm::LEA, var->addr});
        }
        func->asms.push_back(ASM{ASM::basic_asm::LI});
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return true;
    }
    else if (node.name == "Assignment" && node.nodes.size() >= 3)
    {
        // 赋值表达式处理
        // 1. 获取左值地址
        auto& lvalue = node.nodes[0];
        if (lvalue->name != "Identifier")
        {
            std::cerr << "赋值左侧必须是标识符" << std::endl;
            return false;
        }

        std::string var_name = lvalue->token_to_string();
        const varDef* var = func->lookup_var(var_name);
        if (!var)
        {
            var = symbol_table.lookup_var(var_name);
            if (!var)
            {
                std::cerr << "未定义的变量: " << var_name << std::endl;
                return false;
            }
            // 全局变量
            func->asms.push_back(ASM{ASM::basic_asm::IMM, var->addr});
        }
        else
        {
            // 局部变量
            func->asms.push_back(ASM{ASM::basic_asm::LEA, var->addr});
        }

        // 2. 计算右值表达式
        if (auto ret = generate_expression(node.nodes[2], func); !ret)
        {
            return ret;
        }

        // 3. 存储结果
        func->asms.push_back(ASM{ASM::basic_asm::SI});
        // 保持栈顶有结果值
        func->asms.push_back(ASM{ASM::basic_asm::LI});
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return true;
    }
    else if (node.name == "Additive" && node.nodes.size() >= 3)
    {
        // 加减运算处理
        // 1. 计算左操作数
        if (auto ret = generate_expression(node.nodes[0], func); !ret)
        {
            return ret;
        }

        // 2. 计算右操作数
        if (auto ret = generate_expression(node.nodes[2], func); !ret)
        {
            return ret;
        }

        // 3. 执行运算
        std::string op = node.nodes[1]->token_to_string();
        if (op == "+")
        {
            func->asms.push_back(ASM{ASM::basic_asm::ADD});
        }
        else if (op == "-")
        {
            func->asms.push_back(ASM{ASM::basic_asm::SUB});
        }
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return true;
    }
    else if (node.name == "Multiplicative" && node.nodes.size() >= 3)
    {
        // 乘除模运算处理
        // 1. 计算左操作数
        if (auto ret = generate_expression(node.nodes[0], func); !ret)
        {
            return ret;
        }

        // 2. 计算右操作数
        if (auto ret = generate_expression(node.nodes[2], func); !ret)
        {
            return ret;
        }

        // 3. 执行运算
        std::string op = node.nodes[1]->token_to_string();
        if (op == "*")
        {
            func->asms.push_back(ASM{ASM::basic_asm::MUL});
        }
        else if (op == "/")
        {
            func->asms.push_back(ASM{ASM::basic_asm::DIV});
        }
        else if (op == "%")
        {
            func->asms.push_back(ASM{ASM::basic_asm::MOD});
        }
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return true;
    }
    else if (node.name == "Postfix" && node.nodes.size() >= 2)
    {
        // 后缀表达式处理（函数调用等）
        auto primary = node.nodes[0];

        // 函数调用
        if (primary->name == "Identifier" && node.nodes[1]->nodes.size() > 0 &&
            node.nodes[1]->nodes[0]->name == "Args")
        {
            std::string func_name = primary->token_to_string();
            auto args = node.nodes[1]->nodes[0];

            // 按从右到左顺序计算参数
            for (int i = args->nodes.size() - 1; i >= 0; i--)
            {
                if (auto ret = generate_expression(args->nodes[i], func); !ret)
                {
                    return ret;
                }
            }

            // 调用函数
            func->asms.push_back(ASM{"CALL " + func_name});

            // 处理返回值
            func->asms.push_back(ASM{ASM::basic_asm::PUSH});
            return true;
        }
    }

    // 其他表达式类型递归处理
    if (!node.nodes.empty())
    {
        return generate_expression(node.nodes[0], func);
    }

    return true;
}
std::expected<bool, error> OBJ::generate_code(std::shared_ptr<peg::Ast> astnode, funcDef* func,
                                              size_t deep)
{
    if (!astnode)
    {
        return std::unexpected(error::empty_node);
    }
    auto& node = *astnode;
    // if (node.name == "VarDef")
    // {
    //     std::cout << "123" << '\n';
    // };
    if (node.name == "Program")
    {
        for (auto& each : node.nodes)
        {
            if (auto ret = generate_code(each, func, deep + 1); !ret)
            {
                return ret;
            }
        }
        return true;
    }
    else if (node.name == "FuncDef")
    {
        funcDef func;
        func.is_defined = true;
        func.type = Type{node.nodes[0]};
        func.name = node.nodes[1]->token_to_string();
        if (node.nodes.size() == 4)
        {
            // 有参数
            std::vector<varDef> funargs;
            for (auto eachargptr : node.nodes[2]->nodes)
            {
                funargs.push_back(varDef{eachargptr});
            }
            func.add_arg(funargs);
            this->symbol_table.add_global_symbol(func);
            // funname_codes.emplace(func.name, func);
            return generate_code(node.nodes[3], this->symbol_table.lookup_fun(func.name), 0);
        }
        else
        {
            this->symbol_table.add_global_symbol(func);
            return generate_code(node.nodes[2], this->symbol_table.lookup_fun(func.name), 0);
        }
    }
    else if (node.name == "Block")
    {
        // symbol_table.enter_scope();
        func->enter_scope();
        for (auto eachStatement : node.nodes)
        {
            if (auto ret = generate_code(eachStatement, func, deep + 1); !ret)
            {
                return ret;
            }
        }
        // symbol_table.exit_scope();
        func->exit_scope();
    }
    else if (node.name == "Statement")
    {
        if (auto ret = generate_code(astnode->nodes[0], func, 0); !ret)
        {
            return ret;
        }
    }
    else if (node.name == "ReturnStmt")
    {
        if (!node.nodes.size())
        {
            // 返回void, 返回0
            func->asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            func->asms.push_back(ASM{ASM::basic_asm::RET});
        }

        if (auto ret = generate_code(astnode->nodes[0], func, 0); !ret)
        {
            return ret;
        }
        func->asms.push_back(ASM{ASM::basic_asm::RET});
    }
    else if (node.name == "VarDef")
    {
        varDef tpvar;
        tpvar.type = Type{node.nodes[0]};
        tpvar.name = node.nodes[1]->token_to_string();
        func->add_var(tpvar);
        // [TODO] 带初始化的生命
    }
    else if (node.name == "IfStmt")
    {
        // expr:...
        //      ...
        //      jz else
        // true:...
        //      ...
        //      jmup end
        // else:...
        //      ...
        // end: ...
        if (auto ret = generate_code(node.nodes[0], func, 0); !ret)
        {
            return ret;
        }
        size_t startpos = func->asms.size(); // condition_over

        func->enter_scope();
        if (auto ret = generate_code(node.nodes[1], func, 0); !ret)
        {
            func->exit_scope();
            return ret;
        }
        func->exit_scope();

        func->asms.insert(func->asms.begin() + startpos,
                          ASM{ASM::basic_asm::JZ, func->asms.size() + 1});

        size_t if_end = func->asms.size();
        if (node.nodes.size() == 3) // 存在else
        {
            func->enter_scope();
            if (auto ret = generate_code(node.nodes[2], func, 0); !ret)
            {
                func->exit_scope();
                return ret;
            }
            func->exit_scope();
            func->asms.insert(func->asms.begin() + if_end,
                              ASM{ASM::basic_asm::JMP, func->asms.size() + 1});
        }
    }
    else if (node.name == "WhileStmt")
    {
        // start/exp: ...
        //            jz end
        // statement  ...
        //            jmp start
        // end         ...
        //
        size_t startpos = func->asms.size(); // start
        // [TODO] 为break传入信息
        if (auto ret = generate_code(node.nodes[0], func, 0); !ret)
        {
            return ret;
        }
        size_t jumppos = func->asms.size(); // start

        if (auto ret = generate_code(node.nodes[1], func, 0); !ret)
        {
            return ret;
        }
        func->asms.insert(func->asms.begin() + jumppos,
                          ASM{ASM::basic_asm::JZ, func->asms.size() + 1});
        size_t exprok = func->asms.size(); // condition_over
        func->asms.push_back(ASM{ASM::basic_asm::JMP, startpos});
    }
    else if (node.name == "ExprStmt")
    {
        return this->generate_expression(node.nodes[0], func);
    }
    else if (node.name == "Expression")
    {
        return this->generate_expression(astnode,func);
    }
    else
    {
        std::cout << "unsurpported ast: " << node.name << '\n';
    }
    return true;
}

std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths)
{
    using namespace peg;
    std::string peg_rules;
    std::ifstream in("/home/toucher/vscoderope/mycomplier/src/myc_rule.peg");
    if (!in)
    {
        std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/src/myc_rule.peg"
                  << std::endl;
        return std::unexpected(error::file_not_exsist);
    }
    peg_rules.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    parser parser{peg_rules};
    parser.set_logger([](size_t line, size_t col, const std::string& msg)
                      { std::cerr << line << ":" << col << ": " << msg << std::endl; });
    // 确保解析器有效
    assert(static_cast<bool>(parser) == true);
    std::vector<OBJ> objs;
    parser.enable_ast();
    parser.enable_packrat_parsing();
    for (auto& each : paths)
    {
        std::string test_src;
        std::ifstream test_src_in(each);
        if (!test_src_in)
        {
            std::cerr << "无法打开源文件: " << each << std::endl;
            return std::unexpected(error::file_not_exsist);
        }
        test_src.assign((std::istreambuf_iterator<char>(test_src_in)),
                        std::istreambuf_iterator<char>());
        // 定义一个接收 AST 的共享指针
        std::shared_ptr<Ast> ast;
        // 解析输入并生成 AST
        if (parser.parse(test_src, ast))
        {
            OBJ obj{ast, std::filesystem::path{each}.filename()};
            if (auto ret = obj.generate_code())
            {
                objs.push_back(obj);
            }
            else
            {
                return std::unexpected{ret.error()};
            }
        }
    }
    return linker::process(objs);
}

std::expected<std::vector<std::string>, error> linker::process(std::vector<OBJ>& objs)
{
    return std::expected<std::vector<std::string>, error>();
}

size_t Type::getsize() const
{
    if (this->basic_type == BasicType::Char && (!this->is_pointer()))
    {
        return 1;
    }
    return VCPU::size_word;
}

Type::Type(std::shared_ptr<peg::Ast> astnode)
{
    if (!astnode)
    {
        throw;
    }
    auto node = *astnode;
    std::string basictypename = node.nodes[0]->token_to_string();
    if (basictypename == "int")
    {
        basic_type = BasicType::Int;
    }
    else if (basictypename == "char")
    {
        basic_type = BasicType::Char;
    }
    else if (basictypename == "void")
    {
        basic_type = BasicType::Void;
    }
    if (node.nodes.size() == 2)
    {
        // 有ptr
        auto& ptrs = *node.nodes[1];
        std::string ptr_str = ptrs.token_to_string();
        this->pointer_level = std::count(ptr_str.begin(), ptr_str.end(), '*');
    }
}

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
        args[i].addr = -VCPU::size_word * (vardef.size() - i);
    }
    return true;
}

varDef* SymbolTable::add_global_symbol(const varDef& vardef)
{
    if (auto ret = globalvar.find(vardef.name); ret != globalvar.end())
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
    this->type = Type{node.nodes[0]};
    this->is_defined = true;
    this->name = node.nodes[1]->token_to_string();
    // [TODO] 初始化
}

size_t varDef::get_addr_in_mem(size_t posnow)
{
    // 考虑对齐要求
    if (type.getsize() > 1)
    {
        // 对于 int 等较大类型，确保地址是 4 的倍数
        posnow = (posnow + 3) & ~3; // 向上对齐到 4 字节边界
    }
    return posnow;
}
