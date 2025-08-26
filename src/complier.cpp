#include "complier.hpp"
#include <filesystem>
#include <format>
#include <regex>
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
// // 不适用于解析表达式，暂且不用
// std::shared_ptr<peg::Ast> simplify_ast(std::shared_ptr<peg::Ast> ast)
// {
//     if (!ast)
//         return ast;

//     // 先递归处理所有子节点
//     for (size_t i = 0; i < ast->nodes.size(); i++)
//     {
//         ast->nodes[i] = simplify_ast(ast->nodes[i]);
//     }

//     // 表达式路径压缩优化
//     // if (ast->name == "Assignment" || ast->name == "Conditional" || ast->name == "LogicalOr" ||
//     //     ast->name == "LogicalAnd" || ast->name == "BitwiseOr" || ast->name == "BitwiseXor" ||
//     //     ast->name == "BitwiseAnd" || ast->name == "Equality" || ast->name == "Relational" ||
//     //     ast->name == "Shift" || ast->name == "Additive" || ast->name == "Postfix" ||
//     //     ast->name == "Multiplicative" || ast->name == "Cast" || ast->name == "Unary")
//     if (ast->name == "Assignment" || ast->name == "Conditional" || ast->name == "LogicalOr" ||
//         ast->name == "LogicalAnd" || ast->name == "BitwiseOr" || ast->name == "BitwiseXor" ||
//         ast->name == "BitwiseAnd" || ast->name == "Equality" || ast->name == "Relational" ||
//         ast->name == "Shift" || ast->name == "Additive" || ast->name == "Multiplicative" ||
//         ast->name == "Cast" || ast->name == "Unary" || ast->name == "Postfix" ||
//         ast->name == "Primary")
//     {

//         // 如果只有一个子节点，直接返回该子节点
//         if (ast->nodes.size() == 1)
//         {
//             return ast->nodes[0];
//         }
//     }

//     return ast;
// }
std::shared_ptr<peg::Ast> OBJ::simplify_expr_ast(std::shared_ptr<peg::Ast> ast)
{
    if (!ast)
    {
        return ast;
    }
    for (size_t i = 0; i < ast->nodes.size(); i++)
    {
        ast->nodes[i] = simplify_expr_ast(ast->nodes[i]);
    }

    // 表达式路径压缩优化
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
// std::expected<Type, error> OBJ::parse_lvalue_and_push_addr(std::shared_ptr<peg::Ast> expr,
//                                                            funcDef* func)
// {
//     // 变量，数组，指针解引用
//     if (!expr)
//     {
//         return std::unexpected(error::empty_node);
//     }
//     auto& node = *expr;
//     // 直接变量引用
//     if (node.name == "Identifier")
//     {
//         if (auto ret = symbol_table.lookup_var(node.token_to_string()))
//         {
//             func->asms.push_back(ASM{ASM::basic_asm::IMM, ret->name});
//             return ret->type;
//         }
//         if (auto ret = func->lookup_var(node.token_to_string()))
//         {
//             func->asms.push_back(ASM{ASM::basic_asm::LEA, ret->addr});
//             return ret->type;
//         }
//         // [TODO] 数组左值
//         return std::unexpected(error::undifined_var);
//     }
//     else if (node.name == "Unary") // *ptr
//     {
//         if (node.nodes[0]->choice == 7) // *
//         {
//             return generate_expression(node.nodes[1], func); // 取出ptr值
//         }
//     }
//     return std::unexpected(error::expected_lvalue);
// }
std::expected<bool, error> OBJ::generate_code()
{
    transform_postfix_nodes(program);
    transform_left_combine_binary_op_nodes(program);
    program = simplify_expr_ast(this->program);
    visit_ast(program);
    return generate_code(program, this->symbol_table.lookup_fun("__global_init_" + name), 0);
}
// [TODO] 检查类型安全
// [TODO] 连加，连法bug(循环改递归)
std::expected<Type, error> OBJ::generate_expression(std::shared_ptr<peg::Ast> expr, funcDef* func)
{
    if (!expr)
    {
        return std::unexpected(error::empty_node);
    }

    auto& node = *expr;
    Type rettype;
    if (node.name == "Expression")
    {
        return generate_expression(node.nodes[0], func);
    }
    else if (node.name == "Number")
    {
        // 数字字面量处理 - 将值压栈
        int value = std::stoi(node.token_to_string());
        func->asms.push_back(ASM{ASM::basic_asm::IMM, value});
        // func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        rettype.basic_type = Type::BasicType::Int;
        rettype.pointer_level = 0;
        return rettype;
    }
    else if (node.name == "Identifier")
    {
        // id引用处理
        std::string var_name = node.token_to_string();
        if (const varDef* lovar = func->lookup_var(var_name))
        {
            // 局部变量
            func->asms.push_back(ASM{ASM::basic_asm::LEA, lovar->addr});
            func->asms.push_back(ASM{ASM::basic_asm::LI});
            rettype = lovar->type;
        }
        else if (const varDef* glvar = symbol_table.lookup_var(var_name))
        {
            // 全局
            func->asms.push_back(ASM{ASM::basic_asm::IMM, "globalvar@" + glvar->name}); // 后期链接
            func->asms.push_back(ASM{ASM::basic_asm::LI});
            rettype = glvar->type;
        }
        else if (const auto& fun = symbol_table.lookup_fun(var_name))
        {
            // 函数
            func->asms.push_back(ASM{ASM::basic_asm::IMM, "func@" + fun->name}); // 后期链接
            rettype = func->type;
        }
        else
        {
            std::cerr << "未定义的变量: " << var_name << ": "
                      << std::format("{}:{}:{}\n", name, node.line, node.column);
            return std::unexpected(error::undifined_var);
        }
        return rettype;
    }
    else if (node.name == "Assignment")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
        // 赋值表达式处理
        // 1. 获取左值地址
        if (auto lrettype = generate_expression(node.nodes[0], func))
        {
            if (!lrettype)
            {
                return lrettype;
            }
            rettype = lrettype.value();
            if ((func->asms.back().find("LI") != std::string::npos) ||
                (func->asms.back().find("LC") != std::string::npos))
            {
                // 左值均以LI/LC从内存中加载，去掉加载指令后栈顶即为addr
                func->asms.pop_back();
            }
            else
            {
                std::unexpected(error::expected_lvalue);
            }
        }
        // 2. 计算右值表达式
        if (auto ret = generate_expression(node.nodes[2], func); !ret)
        {
            return ret;
        }
        // 备份值到a
        func->asms.push_back(ASM{ASM::basic_asm::MOVE, "stack", "ax"});
        // 3. 存储结果
        func->asms.push_back(ASM{ASM::basic_asm::SI});
        // 保持栈顶的结果值
        func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        return rettype;
    }
    else if (node.name == "Additive")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
        // 加减运算处理
        // 1. 计算左操作数
        auto leftret = generate_expression(node.nodes[0], func);
        if (!leftret)
        {
            return leftret;
        }
        // 2. 计算右操作数
        auto rightret = generate_expression(node.nodes[2], func);
        if (!rightret)
        {
            return rightret;
        }
        // 3. 执行运算
        std::string op = node.nodes[1]->token_to_string();
        if (leftret.value().is_pointer())
        {
            if (!rightret.value().is_pointer())
            {
                Type elementtype = leftret.value();
                elementtype.pointer_level--;
                func->asms.push_back(ASM{ASM::basic_asm::IMM, elementtype.getsize()});
                func->asms.push_back(ASM{ASM::basic_asm::MUL});

                if (op == "+")
                {
                    func->asms.push_back(ASM{ASM::basic_asm::ADD});
                }
                else if (op == "-")
                {
                    func->asms.push_back(ASM{ASM::basic_asm::SUB});
                }
            }
            else if (rightret.value() == rightret.value())
            {
                if (op == "+")
                {
                    func->asms.push_back(ASM{ASM::basic_asm::ADD});
                }
                else if (op == "-")
                {
                    func->asms.push_back(ASM{ASM::basic_asm::SUB});
                }
                func->asms.push_back(ASM{ASM::basic_asm::IMM, rightret.value().getsize()});
                func->asms.push_back(ASM{ASM::basic_asm::DIV});
                rettype.pointer_level = 0;
                rettype.basic_type = Type::BasicType::Int;
                return rettype;
            }
            else
            {
                return std::unexpected(error::illegal_calcu);
            }
        }
        if (op == "+")
        {
            func->asms.push_back(ASM{ASM::basic_asm::ADD});
        }
        else if (op == "-")
        {
            func->asms.push_back(ASM{ASM::basic_asm::SUB});
        }
        return rettype;
    }
    else if (node.name == "Multiplicative")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
        // 乘除模运算处理
        // 1. 计算左操作数
        auto lret = generate_expression(node.nodes[0], func);
        if (!lret)
        {
            return lret;
        }
        // 2. 计算右操作数
        auto rret = generate_expression(node.nodes[2], func);
        if (!rret)
        {
            return rret;
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
            if (lret.value().is_pointer() || rret.value().is_pointer() ||
                lret.value().basic_type == Type::BasicType::Void ||
                rret.value().basic_type == Type::BasicType::Void)
            {
                return std::unexpected(error::illegal_calcu);
            }
            func->asms.push_back(ASM{ASM::basic_asm::MOD});
        }
        rettype.basic_type == Type::BasicType::Int;
        rettype.pointer_level = 0;
        return rettype;
    }
    // [TODO] 优化ast后缀逻辑
    else if (node.name == "Postfix") // 后缀表达式处理
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
        auto primary = node.nodes[0];
        auto& postfix = *node.nodes[1];
        if (postfix.choice == 0) // ()后缀, 调用函数
        {
            // 1. 计算所有参数（从右向左压栈）
            std::vector<std::shared_ptr<peg::Ast>> args;
            size_t args_num = 0;
            if (postfix.nodes.size() > 0)
            {
                auto& args_list = *(postfix.nodes[0]);
                args_num = args_list.nodes.size();
                // 从右向左压栈
                for (int i = args_num; i > 0; i--)
                {
                    if (auto ret = generate_expression(args_list.nodes[i - 1], func); !ret)
                    {
                        return ret;
                    }
                }
            }
            // 2. 最后计算函数地址
            auto funcret = generate_expression(primary, func);
            if (!funcret)
            {
                return funcret;
            }
            // 3. 调用函数
            func->asms.push_back(ASM{ASM::basic_asm::CALL});
            func->asms.push_back(ASM{ASM::basic_asm::DARG, args_num});
            func->asms.push_back(ASM{ASM::basic_asm::PUSH});

            return funcret;
        }
        // [TODO]
        else if (postfix.choice == 1) // [] 后缀
        {
        }
        else if (postfix.choice == 2) //.后缀
        {
        }
        else if (postfix.choice == 3) //->
        {
        }
        else if (postfix.choice == 4) //++
        {
        }
        else if (postfix.choice == 5) //--
        {
        }
        else
        {
            std::cout << "unsurpport postfix: " << postfix.name << '\n';
            return std::unexpected(error::unsurpported_op);
        }
    }
    //[TODO]
    else if (node.name == "Conditional")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "LogicalOr")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "LogicalAnd")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "BitwiseOr")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "BitwiseXor")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "BitwiseAnd")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Equality")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Relational")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Shift")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Cast")
    {
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Unary")
    {
        if (node.choice == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
    }
    else if (node.name == "Primary")
    {
        return generate_expression(node.nodes[0], func);
    }
    else
    {
        std::cout << "unsurpport operator: " << node.name << '\n';
        return std::unexpected(error::unsurpported_op);
    }

    // 理论上不会到这
    return rettype;
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
    else if (node.name == "GlobalVarDef")
    {
        varDef gvardef;
        auto& varnode = *node.nodes[0];
        gvardef.type = Type{varnode.nodes[0]};
        gvardef.name = varnode.nodes[1]->token_to_string();
        if (!this->symbol_table.add_global_symbol(gvardef))
        {
            return std::unexpected(error::double_defined);
        }
        if (varnode.choice == 1) // 带初始化
        {
            func->asms.push_back(ASM{ASM::basic_asm::IMM, "globalvar@" + gvardef.name});
            if (auto ret = generate_expression(node.nodes[0]->nodes[2], func); !ret)
            {
                return std::unexpected{ret.error()};
            }
            // [TODO] 区分char 与 int
            func->asms.push_back(ASM{ASM::basic_asm::LI});
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
            symbol_table.lookup_fun(func.name)->asms.push_back(
                ASM{ASM::basic_asm::NVAR, "labal@NVAR"});
            if (auto ret =
                    generate_code(node.nodes[2], this->symbol_table.lookup_fun(func.name), 0);
                !ret)
            {
                return ret;
            }
            auto ceiling = [](int n, int x)
            {
                // x 必须是 2 的幂
                return (n + x - 1) & ~(x - 1);
            };
            symbol_table.lookup_fun(func.name)->asms[0] =
                ASM{ASM::basic_asm::NVAR,
                    std::to_string(ceiling(symbol_table.lookup_fun(func.name)->max_stack_size,
                                           VCPU::size_word) /
                                   VCPU::size_word)};
            return true;
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
        auto ret = func->add_var(tpvar);
        if (!ret)
        {
            return std::unexpected(error::double_defined);
        }
        if (node.choice == 1) // 带初始化
        {
            func->asms.push_back(ASM{ASM::basic_asm::LEA, ret->addr});
            if (auto ret = generate_expression(node.nodes[2], func); !ret)
            {
                return std::unexpected{ret.error()};
            }
            // [TODO] 区分char 与 int
            func->asms.push_back(ASM{ASM::basic_asm::LI});
        }
        return true;
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
        // size_t startpos = func->asms.size(); // start
        // // [TODO] 为break传入信息
        // if (auto ret = generate_code(node.nodes[0], func, 0); !ret)
        // {
        //     return ret;
        // }
        // size_t jumppos = func->asms.size(); // start

        // if (auto ret = generate_code(node.nodes[1], func, 0); !ret)
        // {
        //     return ret;
        // }
        // func->asms.insert(func->asms.begin() + jumppos,
        //                   ASM{ASM::basic_asm::JZ, func->asms.size() + 1});
        // size_t exprok = func->asms.size(); // condition_over
        // func->asms.push_back(ASM{ASM::basic_asm::JMP, startpos});
    }
    else if (node.name == "ExprStmt")
    {
        if (auto ret = this->generate_expression(node.nodes[0], func); !ret)
        {
            return std::unexpected(ret.error());
        }
        func->asms.push_back(ASM{ASM::basic_asm::POP});
        return true;
    }
    else if (node.name == "Expression")
    {
        if (auto ret = this->generate_expression(astnode, func); !ret)
        {
            return std::unexpected(ret.error());
        }
    }
    else
    {
        std::cout << "unsurpported ast: " << node.name << '\n';
    }
    return true;
}

OBJ::OBJ(std::string content)
{
    // 使用字符串流处理输入
    std::istringstream iss(content);
    std::string line;

    funcDef* current_func = nullptr;

    while (std::getline(iss, line))
    {
        // 跳过空行
        if (line.empty())
            continue;

        // 解析前缀和值
        auto pos = line.find(':');
        if (pos == std::string::npos)
            continue;

        std::string prefix = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        if (prefix == "OBJ_NAME")
        {
            // 设置OBJ名称
            name = value;

            // 创建全局初始化函数
            funcDef __global_init_fun{};
            __global_init_fun.name = "__global_init_" + name;
            __global_init_fun.is_defined = true;
            symbol_table.add_global_symbol(__global_init_fun);
        }
        else if (prefix == "GLOBAL_VAR")
        {
            // 解析全局变量
            std::istringstream var_stream(value);
            std::string var_name, type_str, addr_str, defined_str;

            std::getline(var_stream, var_name, ',');
            std::getline(var_stream, type_str, ',');
            std::getline(var_stream, addr_str, ',');
            std::getline(var_stream, defined_str);

            varDef var;
            var.name = var_name;
            // 解析类型
            if (type_str == "int")
            {
                var.type.basic_type = Type::BasicType::Int;
            }
            else if (type_str == "char")
            {
                var.type.basic_type = Type::BasicType::Char;
            }
            else if (type_str == "void")
            {
                var.type.basic_type = Type::BasicType::Void;
            }

            // 解析指针级别
            size_t ptr_pos = type_str.find('*');
            if (ptr_pos != std::string::npos)
            {
                var.type.pointer_level = type_str.length() - ptr_pos;
            }

            var.addr = std::stoi(addr_str);
            var.is_defined = (defined_str == "1");

            symbol_table.add_global_symbol(var);
        }
        else if (prefix == "FUNCTION")
        {
            // 解析函数定义
            std::istringstream func_stream(value);
            std::string func_name, type_str, defined_str;

            std::getline(func_stream, func_name, ',');
            std::getline(func_stream, type_str, ',');
            std::getline(func_stream, defined_str);

            funcDef func;
            func.name = func_name;
            // 解析返回类型
            if (type_str == "int")
            {
                func.type.basic_type = Type::BasicType::Int;
            }
            else if (type_str == "char")
            {
                func.type.basic_type = Type::BasicType::Char;
            }
            else if (type_str == "void")
            {
                func.type.basic_type = Type::BasicType::Void;
            }

            // 解析指针级别
            size_t ptr_pos = type_str.find('*');
            if (ptr_pos != std::string::npos)
            {
                func.type.pointer_level = type_str.length() - ptr_pos;
            }

            func.is_defined = (defined_str == "1");

            symbol_table.add_global_symbol(func);
            current_func = symbol_table.lookup_fun(func_name);
        }
        else if (prefix == "FUNCTION_CODE" && current_func)
        {
            // 开始读取函数代码
            std::string func_name = value;
            current_func = symbol_table.lookup_fun(func_name);
        }
        else if (prefix == "FUNCTION_END")
        {
            // 结束当前函数的代码读取
            current_func = nullptr;
        }
        else if (prefix == "ASM" && current_func)
        {
            // 添加汇编指令到当前函数
            current_func->asms.push_back(value);
        }
        else if (prefix == "ARG" && current_func)
        {
            // 解析函数参数
            std::istringstream arg_stream(value);
            std::string arg_name, type_str, addr_str;

            std::getline(arg_stream, arg_name, ',');
            std::getline(arg_stream, type_str, ',');
            std::getline(arg_stream, addr_str);

            varDef arg;
            arg.name = arg_name;
            // 解析类型
            if (type_str == "int")
            {
                arg.type.basic_type = Type::BasicType::Int;
            }
            else if (type_str == "char")
            {
                arg.type.basic_type = Type::BasicType::Char;
            }
            else if (type_str == "void")
            {
                arg.type.basic_type = Type::BasicType::Void;
            }

            // 解析指针级别
            size_t ptr_pos = type_str.find('*');
            if (ptr_pos != std::string::npos)
            {
                arg.type.pointer_level = type_str.length() - ptr_pos;
            }

            arg.addr = std::stoi(addr_str);
            arg.is_defined = true;

            current_func->args.push_back(arg);
        }
    }
}

std::string OBJ::to_string()
{
    std::ostringstream oss;

    // 输出对象名称
    oss << "OBJ_NAME:" << name << std::endl;

    // 输出全局变量
    auto* sym_table = &symbol_table;
    for (const auto& var_entry : sym_table->globalvar)
    {
        const auto& var = var_entry.second;

        // 构造类型字符串
        std::string type_str;
        if (var.type.basic_type == Type::BasicType::Int)
        {
            type_str = "int";
        }
        else if (var.type.basic_type == Type::BasicType::Char)
        {
            type_str = "char";
        }
        else if (var.type.basic_type == Type::BasicType::Void)
        {
            type_str = "void";
        }

        // 添加指针星号
        for (int i = 0; i < var.type.pointer_level; i++)
        {
            type_str += "*";
        }

        oss << "GLOBAL_VAR:" << var.name << "," << type_str << "," << var.addr << ","
            << (var.is_defined ? "1" : "0") << std::endl;
    }

    // 输出函数定义和代码
    for (const auto& func_entry : sym_table->globalfuncdef)
    {
        const auto& func = func_entry.second;

        // 构造类型字符串
        std::string type_str;
        if (func.type.basic_type == Type::BasicType::Int)
        {
            type_str = "int";
        }
        else if (func.type.basic_type == Type::BasicType::Char)
        {
            type_str = "char";
        }
        else if (func.type.basic_type == Type::BasicType::Void)
        {
            type_str = "void";
        }

        // 添加指针星号
        for (int i = 0; i < func.type.pointer_level; i++)
        {
            type_str += "*";
        }

        // 输出函数定义
        oss << "FUNCTION:" << func.name << "," << type_str << "," << (func.is_defined ? "1" : "0")
            << std::endl;

        // 输出函数参数
        for (const auto& arg : func.args)
        {
            // 构造参数类型字符串
            std::string arg_type_str;
            if (arg.type.basic_type == Type::BasicType::Int)
            {
                arg_type_str = "int";
            }
            else if (arg.type.basic_type == Type::BasicType::Char)
            {
                arg_type_str = "char";
            }
            else if (arg.type.basic_type == Type::BasicType::Void)
            {
                arg_type_str = "void";
            }

            // 添加指针星号
            for (int i = 0; i < arg.type.pointer_level; i++)
            {
                arg_type_str += "*";
            }

            oss << "ARG:" << arg.name << "," << arg_type_str << "," << arg.addr << std::endl;
        }

        // 输出函数代码
        oss << "FUNCTION_CODE:" << func.name << std::endl;
        for (const auto& asm_instr : func.asms)
        {
            oss << "ASM:" << asm_instr << std::endl;
        }
        oss << "FUNCTION_END" << std::endl;
    }

    return oss.str();
}
void OBJ::transform_postfix_nodes(std::shared_ptr<peg::Ast>& ast)
{
    // 如果不是 Postfix 节点，或只有一个子节点，直接返回
    if (!ast || ast->name != "Postfix" || ast->nodes.size() <= 1)
    {
        // 递归处理子节点
        if (ast && !ast->nodes.empty())
        {
            for (auto& node : ast->nodes)
            {
                transform_postfix_nodes(node);
            }
        }
        return;
    }

    // 第一个节点是 Primary
    auto result = ast->nodes[0];

    // 遍历处理所有后缀操作
    for (size_t i = 1; i < ast->nodes.size(); i++)
    {
        // 正确创建空节点容器
        std::vector<std::shared_ptr<peg::Ast>> nodes;
        nodes.push_back(result);
        nodes.push_back(ast->nodes[i]);

        // 创建新节点
        auto new_postfix = std::make_shared<peg::Ast>(ast->path.c_str(), // 使用 c_str() 转换
                                                      ast->line, ast->column,
                                                      "Postfix", // 直接使用字符串字面量是可以的
                                                      nodes      // 使用预先创建的向量
        );

        result = new_postfix;
    }

    // 替换当前 AST - 使用指针赋值而不是内容赋值
    ast = result;

    // 递归处理新树的子节点
    for (auto& node : ast->nodes)
    {
        transform_postfix_nodes(node);
    }
}
void OBJ::transform_left_combine_binary_op_nodes(std::shared_ptr<peg::Ast>& ast)
{
    if (!ast)
        return;

    // 1. 先递归处理所有子节点
    for (size_t i = 0; i < ast->nodes.size(); i++)
    {
        transform_left_combine_binary_op_nodes(ast->nodes[i]);
    }

    // 2. 检查是否是二元表达式
    if (!is_binary_expr(ast->name) || ast->nodes.size() <= 1)
    {
        return;
    }

    // 3. 二元表达式转换为递归结构
    auto expr_type = ast->name;
    auto first_term = ast->nodes[0];
    auto current = first_term;

    // 如果有操作符节点和操作数对，循环处理
    for (size_t i = 1; i < ast->nodes.size(); i += 2)
    {
        // 确保有一个操作符和一个操作数
        if (i + 1 >= ast->nodes.size())
            break;

        auto op_node = ast->nodes[i];
        auto right_term = ast->nodes[i + 1];

        // 创建新的表达式节点
        std::vector<std::shared_ptr<peg::Ast>> new_nodes;
        new_nodes.push_back(current);
        new_nodes.push_back(op_node);
        new_nodes.push_back(right_term);

        auto new_expr = std::make_shared<peg::Ast>(ast->path.c_str(), ast->line, ast->column,
                                                   expr_type.c_str(), new_nodes);

        current = new_expr;
    }

    // 4. 替换原始节点
    ast = current;
}

// 辅助函数：检查是否是二元表达式类型
bool OBJ::is_binary_expr(const std::string& name)
{
    static const std::unordered_set<std::string> binary_exprs = {
        "LogicalOr", "LogicalAnd", "BitwiseOr", "BitwiseXor", "BitwiseAnd",
        "Equality",  "Relational", "Shift",     "Additive",   "Multiplicative"};
    return binary_exprs.find(name) != binary_exprs.end();
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
                std::cout << obj.to_string() << '\n';
            }
            else
            {
                return std::unexpected{ret.error()};
            }
        }
    }
    return linker{objs}.process();
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
// [TODO] bug
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
    this->exe.asms.push_back(ASM{ASM::basic_asm::CALL, exe.asms.size() + 2}); // call main
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
        args[i].addr = -VCPU::size_word * (i + 2);
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
    this->type = Type{node.nodes[0]};
    this->is_defined = true;
    this->name = node.nodes[1]->token_to_string();
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
