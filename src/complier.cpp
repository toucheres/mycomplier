#include "complier.hpp"
#include <filesystem>
#include <format>
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
    program = simplify_expr_ast(this->program);
    visit_ast(program);
    return generate_code(program, this->symbol_table.lookup_fun("__global_init_" + name), 0);
}
// [TODO] 修正generate_expression
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
        // 变量引用处理
        std::string var_name = node.token_to_string();
        const varDef* var = func->lookup_var(var_name);
        if (!var)
        {
            // 全局变量
            var = symbol_table.lookup_var(var_name);
            if (!var)
            {
                std::cerr << "未定义的变量: " << var_name << ": "
                          << std::format("{}:{}:{}\n", name, node.line, node.column);
                return std::unexpected(error::undifined_var);
            }
            func->asms.push_back(ASM{ASM::basic_asm::IMM, var->name}); // 后期链接
        }
        else
        {
            // 局部变量
            func->asms.push_back(ASM{ASM::basic_asm::LEA, var->addr});
        }
        func->asms.push_back(ASM{ASM::basic_asm::LI});
        // func->asms.push_back(ASM{ASM::basic_asm::PUSH});
        rettype = var->type;
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
                return lrettype;
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
    else if (node.name == "Postfix")
    {
        // 后缀表达式处理（函数调用等）
        if (node.nodes.size() == 1)
        {
            return generate_expression(node.nodes[0], func);
        }
        auto primary = node.nodes[0];
        for (int i = 1; i < node.nodes.size(); i++)
        {
            auto& eachpostfix = *node.nodes[i];
            if (eachpostfix.choice == 0) // ()后缀
            {
                // 函数调用
                if (primary->name == "Identifier")
                {
                    std::string func_name = primary->token_to_string();
                    std::shared_ptr<peg::Ast> args;
                    size_t args_num = 0;
                    if (node.nodes.size() != 1) // 有args
                    {
                        args = node.nodes[0];
                    }
                    if (args_num) // 从左向右入参
                    {
                        for (auto& each : args->nodes)
                        {
                            if (auto ret = generate_expression(each, func); !ret)
                            {
                                return ret;
                            }
                        }
                    }
                    // 调用函数
                    func->asms.push_back(ASM{"CALL " + func_name}); // 链接时确定addr
                    // 清理参数
                    func->asms.push_back(ASM{ASM::basic_asm::DARG, args_num});
                    // 处理返回值返回值(约定在ax)
                    func->asms.push_back(ASM{ASM::basic_asm::PUSH});
                    return func->type;
                }
            }
            // [TODO]
            else if (eachpostfix.choice == 1) // [] 后缀
            {
            }
            else if (eachpostfix.choice == 2) //.后缀
            {
            }
            else if (eachpostfix.choice == 3) //->
            {
            }
            else if (eachpostfix.choice == 4) //++
            {
            }
            else if (eachpostfix.choice == 5) //--
            {
            }
            else
            {
                std::cout << "unsurpport postfix: " << eachpostfix.name << '\n';
                return std::unexpected(error::unsurpported_op);
            }
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
        if (auto ret = this->generate_expression(astnode, func);!ret)
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
