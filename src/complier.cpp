#include "complier.hpp"
#include "file.hpp"
#include "preprocessor.hpp"
#include "tokenprocessor.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

std::expected<bool, error> Complier::try_parse_fun(Tokens& tokens, obj& obj)
{
    fun_def thisfun;
    int startpos = tokens.pos;
    if (auto ret = tokens.now().is_type())
    {
        Type rettype{ret.value()};
        tokens.pos++;
        while (tokens.now().content == "*")
        {
            rettype.ptr_lay++;
            tokens.pos++;
        }
        thisfun.type = rettype;
    }
    else
    {
        tokens.pos = startpos;
        return std::unexpected(error::unkowntype);
    }
    if (tokens.now().can_be_id())
    {
        thisfun.id = tokens.now().content;
        tokens.pos++;
    }
    else
    {
        obj.func_var_defs_.outto_old_namespace();
        tokens.pos = startpos;
        return std::unexpected(error::illageid);
    }
    // 为函数创建新的作用域
    obj.func_var_defs_.clear();
    obj.func_var_defs_.into_new_namespace();
    // 在解析函数体前推入thisfun以支持递归
    thisfun.addr = obj.content.size();
    thisfun.defined = true;
    obj.fun_defs_.push(thisfun);

    auto ret = try_parse_args(tokens, obj);
    if (!ret)
    {
        obj.func_var_defs_.into_new_namespace();
        tokens.pos = startpos;
        return std::unexpected(ret.error());
    }
    else
    {
        thisfun.argtypes = ret.value();
        // 为返回地址预留
        var_def retaddr{};
        retaddr.id = "returnaddr";
        retaddr.type = Type{Basic_Type::INT};
        retaddr.lr == var_def::valtype::funcval;
        obj.func_var_defs_.push(retaddr);
        // 为oldbp预留
        retaddr.id = "oldbp";
        obj.func_var_defs_.push(retaddr);
    }

    obj.pushASM(VM::ASM::HOLD); // 占位
    // std::cout << "do once\n";
    auto nvar_pos = obj.content.size();

    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        obj.func_var_defs_.outto_old_namespace();

        tokens.pos = startpos;
        return std::unexpected(ret2.error());
    }
    obj.func_var_defs_.outto_old_namespace();
    obj.content[nvar_pos - 1] = std::format("NVAR {}", obj.func_var_defs_.get_max_size() - 2);
    // 函数解析完成，退出函数作用域

    return true;
}

std::expected<bool, error> Complier::try_parse_block(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;
    if (!(tokens.now().content == "{"))
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++; // 跳过开始的 '{'

    bool flag = false;
    do
    {
        flag = false;
        int startpos = tokens.pos;
        // 遇到下一个作用域
        if (tokens.now().content == "{")
        {
            obj.func_var_defs_.into_new_namespace();
            return try_parse_block(tokens, obj);
        }
        // 检查是否到达结束大括号
        if (tokens.now().content == "}")
        {
            tokens.pos++;
            return true;
        }

        // 尝试解析变量声明
        if (auto ret = try_parse_func_var(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.pos = startpos;

        // 尝试解析 if 语句
        if (auto ret = try_parse_if(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.pos = startpos;

        // 尝试解析 while 语句
        if (auto ret = try_parse_while(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.pos = startpos;

        // 尝试解析 return 语句
        if (auto ret = try_parse_return(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.pos = startpos;

        // 尝试解析表达式语句
        if (auto ret = try_parse_expr(tokens, obj))
        {
            if (tokens.now().content == ";")
            {
                tokens.pos++;
                flag = true;
                continue;
            }
        }
        tokens.pos = startpos;

        // 如果所有解析都失败，说明遇到了不认识的token
        // 为了避免无限循环，我们需要跳过这个token
        if (!flag)
        {
            tokens.pos++;
            if (tokens.prase_over())
            {
                return std::unexpected(error::expected_fenhao);
            }
        }

    } while (flag || !tokens.prase_over());

    // 如果循环结束还没找到结束大括号，报错
    return std::unexpected(error::expected_fenhao);
}

std::expected<std::vector<Type>, error> Complier::try_parse_args(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;
    if (tokens.now().content != "(")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 处理空参数列表
    if (tokens.now().content == ")")
    {
        tokens.pos++;
        return {};
    }
    std::vector<Type> argtypes;
    std::vector<var_def> args;
    do
    {
        auto ret = tokens.now().is_type();
        if (!ret)
        {
            tokens.pos = startpos;
            return std::unexpected(error::unkowntype);
        }
        tokens.pos++;
        Type type{ret.value()};

        while (tokens.now().content == "*")
        {
            type.ptr_lay++;
            tokens.pos++;
        }

        if (tokens.now().can_be_id())
        {
            var_def arg;
            arg.defined = true;
            arg.id = tokens.now().content;
            arg.type = type;
            argtypes.push_back(type);
            args.push_back(arg);
            tokens.pos++;
        }
        else
        {
            tokens.pos = startpos;
            return std::unexpected(error::illageid);
        }

        if (tokens.now().content == ",")
        {
            tokens.pos++;
            continue;
        }
        else if (tokens.now().content == ")")
        {
            tokens.pos++;
            obj.func_var_defs_.push_func_args(args);
            return argtypes;
        }
        else
        {
            tokens.pos = startpos;
            return std::unexpected(error::expected_fenhao);
        }
    } while (true);
}

std::expected<bool, error> Complier::try_parse_while(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;
    if (tokens.now().content != "while")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_while);
    }
    tokens.pos++;

    // 解析条件表达式 (expression)
    if (tokens.now().content != "(")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    auto ret = try_parse_expr(tokens, obj);
    if (!ret)
    {
        tokens.pos = startpos;
        return std::unexpected(ret.error());
    }

    if (tokens.now().content != ")")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 解析循环体
    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        tokens.pos = startpos;
        return std::unexpected(ret2.error());
    }

    return true;
}

// if(exp)
// {
//     do1
// }
// else
// {
//      do2
// }
// 计算exp
//  jnz else
//  do1
//  jump end
// else:
//  do2
// end:
std::expected<bool, error> Complier::try_parse_if(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;
    if (tokens.now().content != "if")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 解析条件表达式 (expression)
    if (tokens.now().content != "(")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    auto ret = try_parse_expr(tokens, obj);
    if (!ret)
    {
        tokens.pos = startpos;
        return std::unexpected(ret.error());
    }
    obj.pushASM(VM::ASM::HOLD);
    auto flag = obj.content.size();

    if (tokens.now().content != ")")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        tokens.pos = startpos;
        return std::unexpected(ret2.error());
    }
    obj.pushASM(VM::ASM::HOLD); // 主分支跳转end
    auto pos_if_end = obj.content.size();

    obj.content[flag - 1] = std::format("JZ {}", obj.content.size());
    // 可选的 else 分支
    if (tokens.now().content == "else")
    {
        tokens.pos++;
        auto ret3 = try_parse_block(tokens, obj);
        if (!ret3)
        {
            tokens.pos = startpos;
            return std::unexpected(ret3.error());
        }
    }
    obj.content[pos_if_end - 1] = std::format("JMP {}", obj.content.size());
    return true;
}

std::expected<bool, error> Complier::try_parse_return(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;
    if (tokens.now().content != "return")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 检查是否有返回值表达式
    if (!tokens.prase_over() && tokens.now().content != ";")
    {
        // 解析返回值表达式
        auto ret = try_parse_expr(tokens, obj);
        if (!ret)
        {
            tokens.pos = startpos;
            return std::unexpected(ret.error());
        }
    }
    else
    {
        // 无返回值的 return 语句，压入 0 作为默认返回值
        obj.pushASM(VM::ASM::IMM, 0);
    }

    // 检查分号
    if (tokens.prase_over() || tokens.now().content != ";")
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 生成返回指令 (表达式的结果已经在栈顶，直接返回)
    obj.pushASM(VM::ASM::RET);

    return true;
}

std::expected<bool, error> Complier::try_parse_expr(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }

    // 解析赋值表达式（最低优先级）
    auto ret = try_parse_assignment_expr(tokens, obj);
    if (!ret)
    {
        tokens.pos = startpos;
        return std::unexpected(ret.error());
    }

    return true;
}

// 解析赋值表达式
std::expected<bool, error> Complier::try_parse_assignment_expr(Tokens& tokens, obj& obj)
{
    // auto startpos = tokens.pos;
    // auto ret = try_parse_left_var_and_get_addr(tokens, obj);
    // if (ret && is_assignment_operator(tokens[tokens.pos + 1].content))
    // {
    //     tokens.pos++;
    //     obj.save();
    //     if (ret.value().lr == var_def::vartype::funcval)
    //     {
    //         obj.pushASM(VM::ASM::LEA, ret.value().addr);
    //     }
    //     else
    //     {
    //         obj.pushASM(VM::ASM::IMM, ret.value().addr);
    //     }
    //     if (auto ret2 = try_parse_assignment_expr(tokens, obj); !ret2)
    //     {
    //         tokens.pos = startpos;
    //         return ret2;
    //     }
    //     // if (ret.value().type.ptr_lay == 0 && ret.value().type.bt == Basic_Type::CHAR)
    //     // {}else{
    //     obj.pushASM(VM::ASM::SI); // 存储到指定地址
    // }
    // tokens.pos = startpos;
    // return try_parse_rightVal_and_get_value(tokens, obj);
    // //...
    int startpos = tokens.pos;

    // [TODO]基于左值解析的赋值
    // 先检查是否是简单的变量赋值: identifier = expression
    if (tokens.now().can_be_id() && tokens.pos + 1 < tokens.size() &&
        is_assignment_operator(tokens[tokens.pos + 1].content))
    {
        // 这是一个赋值表达式
        std::string var_id = tokens.now().content;
        tokens.pos++; // 跳过变量名

        std::string op = tokens.now().content;
        tokens.pos++; // 跳过赋值运算符

        // 解析右值表达式
        auto ret = try_parse_assignment_expr(tokens, obj);
        if (!ret)
        {
            tokens.pos = startpos;
            return ret;
        }

        // 查找变量地址并生成存储指令
        auto var_result = obj.func_var_defs_.find(var_id);
        if (var_result)
        {
            obj.pushASM(VM::ASM::LEA, var_result.value().addr); // 取bp+bias
            obj.pushASM(VM::ASM::SI);                            // 存储到指定地址
        }
        else
        {
            var_result = obj.global_var_defs_.find(var_id);
            if (var_result)
            {
                obj.pushASM(VM::ASM::SI, var_result.value().addr);
            }
            else
            {
                return std::unexpected(error::undefinedvar);
            }
        }

        return true;
    }

    // 不是赋值表达式，按普通表达式处理
    tokens.pos = startpos;
    return try_parse_left_or_right_value_and_get_value(tokens, obj);
}

// 解析逻辑或表达式
std::expected<bool, error> Complier::try_parse_left_or_right_value_and_get_value(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_logical_and_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && tokens.now().content == "||")
    {
        tokens.pos++;
        auto ret2 = try_parse_logical_and_expr(tokens, obj);
        if (!ret2)
            return ret2;

        obj.pushASM(VM::ASM::OR);
    }

    return true;
}

// 解析逻辑与表达式
std::expected<bool, error> Complier::try_parse_logical_and_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_equality_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && tokens.now().content == "&&")
    {
        tokens.pos++;
        auto ret2 = try_parse_equality_expr(tokens, obj);
        if (!ret2)
            return ret2;

        obj.pushASM(VM::ASM::AND);
    }

    return true;
}

// 解析相等性表达式
std::expected<bool, error> Complier::try_parse_equality_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_relational_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && (tokens.now().content == "==" || tokens.now().content == "!="))
    {
        std::string op = tokens.now().content;
        tokens.pos++;
        auto ret2 = try_parse_relational_expr(tokens, obj);
        if (!ret2)
            return ret2;

        if (op == "==")
            obj.pushASM(VM::ASM::EQ);
        else if (op == "!=")
            obj.pushASM(VM::ASM::NE);
    }

    return true;
}

// 解析关系表达式
std::expected<bool, error> Complier::try_parse_relational_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_additive_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && is_relational_operator(tokens.now().content))
    {
        std::string op = tokens.now().content;
        tokens.pos++;
        auto ret2 = try_parse_additive_expr(tokens, obj);
        if (!ret2)
            return ret2;

        if (op == "<")
            obj.pushASM(VM::ASM::LT);
        else if (op == ">")
            obj.pushASM(VM::ASM::GT);
        else if (op == "<=")
            obj.pushASM(VM::ASM::LE);
        else if (op == ">=")
            obj.pushASM(VM::ASM::GE);
    }

    return true;
}

// 解析加法表达式
std::expected<bool, error> Complier::try_parse_additive_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_multiplicative_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && (tokens.now().content == "+" || tokens.now().content == "-"))
    {
        std::string op = tokens.now().content;
        tokens.pos++;
        auto ret2 = try_parse_multiplicative_expr(tokens, obj);
        if (!ret2)
            return ret2;

        if (op == "+")
            obj.pushASM(VM::ASM::ADD);
        else if (op == "-")
            obj.pushASM(VM::ASM::SUB);
    }

    return true;
}

// 解析乘法表达式
std::expected<bool, error> Complier::try_parse_multiplicative_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_unary_expr(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over() && is_multiplicative_operator(tokens.now().content))
    {
        std::string op = tokens.now().content;
        tokens.pos++;
        auto ret2 = try_parse_unary_expr(tokens, obj);
        if (!ret2)
            return ret2;

        if (op == "*")
            obj.pushASM(VM::ASM::MUL);
        else if (op == "/")
            obj.pushASM(VM::ASM::DIV);
        else if (op == "%")
            obj.pushASM(VM::ASM::MOD);
    }

    return true;
}

// 解析一元表达式
std::expected<bool, error> Complier::try_parse_unary_expr(Tokens& tokens, obj& obj)
{
    if (tokens.prase_over())
    {
        return std::unexpected(error::expected_fenhao);
    }

    // 一元运算符
    if (is_unary_operator(tokens.now().content))
    {
        std::string op = tokens.now().content;
        tokens.pos++;
        //[TODO] 先尝试 左值op+find左值 在其余op+parse

        if (op == "&")
        {
            auto lret = try_parse_left_var_and_get_addr(tokens, obj);
            if (lret)
            {
                return true;
            }
            else
            {
                return std::unexpected(error::expected_left_value);
            }
        }

        auto ret = try_parse_unary_expr(tokens, obj); // 递归处理嵌套一元运算符
        if (!ret)
            return ret;

        // 生成一元运算符汇编
        if (op == "*")
        {
            // 解引用：从地址加载值
            obj.pushASM(VM::ASM::LI);
        }
        // else if (op == "&")
        // {
        //     // 取地址：获取变量地址
        //     auto name = tokens.now();
        //     auto ret = obj.func_var_defs_.find(name.content);
        //     if (ret)
        //     {
        //         obj.pushASM(VM::ASM::LEA, ret.value().addr);
        //         return true;
        //     }
        //     ret = obj.global_var_defs_.find(name.content);
        //     if (ret)
        //     {
        //         obj.pushASM(VM::ASM::IMM, ret.value().addr);
        //         return true;
        //     }
        // }
        else if (op == "-")
        {
            // 负号：0 - expr
            obj.pushASM(VM::ASM::IMM, 0);
            obj.pushASM(VM::ASM::SUB);
        }
        // TODO: 其他一元运算符 ! ~ ++ --

        return true;
    }

    return try_parse_postfix_expr(tokens, obj);
}

// 解析后缀表达式（函数调用、数组访问等）
std::expected<bool, error> Complier::try_parse_postfix_expr(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_primary(tokens, obj);
    if (!ret)
        return ret;

    while (!tokens.prase_over())
    {
        if (tokens.now().content == "[")
        {
            // 数组访问 expr[index]
            tokens.pos++;
            auto index_ret = try_parse_expr(tokens, obj);
            if (!index_ret)
                return index_ret;

            if (tokens.prase_over() || tokens.now().content != "]")
            {
                return std::unexpected(error::expected_fenhao);
            }
            tokens.pos++;

            // 生成数组访问汇编：base + index * sizeof(type)
            obj.pushASM(VM::ASM::ADD); // 简化：假设 sizeof(type) = 1
            obj.pushASM(VM::ASM::LI);  // 从计算出的地址加载值
        }
        else if (tokens.now().content == "(")
        {
            // 函数调用已在 try_parse_primary 中处理
            break;
        }
        else
        {
            break;
        }
    }

    return true;
}
std::expected<bool, error> Complier::try_parse_primary(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.pos = startpos;
        return std::unexpected(error::expected_fenhao);
    }

    // 数字字面量
    if (is_number(tokens.now().content))
    {
        int value = std::stoi(tokens.now().content);
        obj.pushASM(VM::ASM::IMM, value);
        tokens.pos++;
        return true;
    }

    // 标识符（变量或函数调用）
    if (tokens.now().can_be_id())
    {
        std::string id = tokens.now().content;
        tokens.pos++;

        // 检查是否是函数调用
        if (!tokens.prase_over() && tokens.now().content == "(")
        {
            tokens.pos++;

            // 解析参数列表
            int arg_count = 0;
            if (!tokens.prase_over() && tokens.now().content != ")")
            {
                do
                {
                    auto ret = try_parse_expr(tokens, obj);
                    if (!ret)
                    {
                        tokens.pos = startpos;
                        return std::unexpected(ret.error());
                    }
                    arg_count++;

                    if (tokens.prase_over())
                    {
                        tokens.pos = startpos;
                        return std::unexpected(error::expected_fenhao);
                    }

                    if (tokens.now().content == ",")
                    {
                        tokens.pos++;
                        continue;
                    }
                    else if (tokens.now().content == ")")
                    {
                        break;
                    }
                    else
                    {
                        tokens.pos = startpos;
                        return std::unexpected(error::expected_fenhao);
                    }
                } while (true);
            }

            if (!tokens.prase_over() && tokens.now().content == ")")
            {
                tokens.pos++;
            }
            else
            {
                tokens.pos = startpos;
                return std::unexpected(error::expected_fenhao);
            }

            // 生成函数调用汇编
            auto fun_result = obj.fun_defs_.find(id);
            if (fun_result)
            {
                obj.pushASM(VM::ASM::CALL, fun_result.value().addr); // 调用指定地址的函数
                // 局部变量 arg1 arg2 ...
                // obj.pushASM(VM::ASM::MOVE, (int)VCPU::stack_cpu::STACK,
                // (int)VCPU::stack_cpu::AX);
                obj.pushASM(VM::ASM::DARG, arg_count);
                // for (int i = 0; i < arg_count; i++)
                // {
                //     obj.pushASM(VM::ASM::POP);
                // }
                obj.pushASM(VM::ASM::PUSH);
            }
            else
            {
                // 未定义的函数（如 printf），生成系统调用
                obj.pushASM(VM::ASM::SYSTEMCALL); // 标记为系统调用
            }
            return true;
        }
        else
        {
            // 变量引用 - 需要查找变量地址
            auto var_result = obj.func_var_defs_.find(id);
            // auto var_result = obj.global_var_defs_.find(id);
            if (var_result)
            {
                obj.pushASM(VM::ASM::LEA, var_result.value().addr); // 压入bp+局部变量偏移
                obj.pushASM(VM::ASM::LI);                            // 加载指定地址的变量值
            }
            else
            {
                // func局部如果找不到变量，尝试全局
                var_result = obj.global_var_defs_.find(id);
                if (var_result)
                {
                    obj.pushASM(VM::ASM::LI, var_result.value().addr); // 加载变量值
                }
                else
                {
                    std::unexpected(error::undefinedvar);
                }
            }
            return true;
        }
    }

    // 括号表达式
    if (tokens.now().content == "(")
    {
        tokens.pos++;
        auto ret = try_parse_expr(tokens, obj);
        if (!ret)
        {
            tokens.pos = startpos;
            return std::unexpected(ret.error());
        }

        if (tokens.prase_over() || tokens.now().content != ")")
        {
            tokens.pos = startpos;
            return std::unexpected(error::expected_fenhao);
        }
        tokens.pos++;
        return true;
    }

    tokens.pos = startpos;
    return std::unexpected(error::expected_fenhao);
}

// 辅助函数
bool Complier::is_assignment_operator(const std::string& token)
{
    return token == "=" || token == "+=" || token == "-=" || token == "*=" || token == "/=" ||
           token == "%=";
}

bool Complier::is_relational_operator(const std::string& token)
{
    return token == "<" || token == ">" || token == "<=" || token == ">=";
}

bool Complier::is_multiplicative_operator(const std::string& token)
{
    return token == "*" || token == "/" || token == "%";
}

bool Complier::is_unary_operator(const std::string& token)
{
    return token == "*" || token == "&" || token == "!" || token == "~" || token == "++" ||
           token == "--" || token == "+" || token == "-";
}

bool Complier::is_binary_operator(const std::string& token)
{
    return token == "+" || token == "-" || token == "*" || token == "/" || token == "==" ||
           token == "!=" || token == "<" || token == ">" || token == "<=" || token == ">=" ||
           token == "=" || token == "&&" || token == "||";
}

bool Complier::is_number(const std::string& token)
{
    if (token.empty())
        return false;
    for (char c : token)
    {
        if (!std::isdigit(c))
            return false;
    }
    return true;
}

// void Complier::generate_binary_op_asm(const std::string& op, obj& obj)
// {
//     if (op == "+")
//         obj.pushASM(VM::ASM::ADD);
//     else if (op == "-")
//         obj.pushASM(VM::ASM::SUB);
//     else if (op == "*")
//         obj.pushASM(VM::ASM::MUL);
//     else if (op == "/")
//         obj.pushASM(VM::ASM::DIV);
//     else if (op == "==")
//         obj.pushASM(VM::ASM::EQ);
//     else if (op == "!=")
//         obj.pushASM(VM::ASM::NE);
//     else if (op == "<")
//         obj.pushASM(VM::ASM::LT);
//     else if (op == ">")
//         obj.pushASM(VM::ASM::GT);
//     else if (op == "<=")
//         obj.pushASM(VM::ASM::LE);
//     else if (op == ">=")
//         obj.pushASM(VM::ASM::GE);
//     else if (op == "=")
//         obj.pushASM(VM::ASM::SI); // 赋值
// }

std::expected<bool, error> Complier::try_parse_global_var(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.pos = startpos;
        return std::unexpected(error::unkowntype);
    }

    auto ret = tokens.now().is_type();
    if (!ret)
    {
        tokens.pos = startpos;
        return std::unexpected(error::unkowntype);
    }

    Type type{ret.value()};
    tokens.pos++;

    // 检查指针层数
    while (!tokens.prase_over() && tokens.now().content == "*")
    {
        type.ptr_lay++;
        tokens.pos++;
    }

    // 检查标识符和分号
    if (!tokens.prase_over() && tokens.now().can_be_id() && tokens.pos + 1 < tokens.size() &&
        tokens[tokens.pos + 1].content == ";")
    {
        var_def var;
        var.id = tokens.now().content;
        var.defined = true;
        var.type = type;
        var.lr = var_def::valtype::globalval;
        auto push_ret = obj.global_var_defs_.push(var);
        if (!push_ret)
        {
            tokens.pos = startpos;
            return std::unexpected(error::doubledefined);
        }
        tokens.pos += 2; // 跳过标识符和分号
    }
    else
    {
        tokens.pos = startpos;
        return std::unexpected(error::illageid);
    }

    return true;
}

std::expected<bool, error> Complier::try_parse_func_var(Tokens& tokens, obj& obj)
{
    int startpos = tokens.pos;

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.pos = startpos;
        return std::unexpected(error::unkowntype);
    }

    auto ret = tokens.now().is_type();
    if (!ret)
    {
        tokens.pos = startpos;
        return std::unexpected(error::unkowntype);
    }

    Type type{ret.value()};
    tokens.pos++;

    // 检查指针层数
    while (!tokens.prase_over() && tokens.now().content == "*")
    {
        type.ptr_lay++;
        tokens.pos++;
    }

    // 检查标识符和分号
    if (!tokens.prase_over() && tokens.now().can_be_id() && tokens.pos + 1 < tokens.size() &&
        tokens[tokens.pos + 1].content == ";")
    {
        var_def var;
        var.id = tokens.now().content;
        var.defined = true;
        var.type = type;
        var.lr = var_def::valtype::funcval;
        auto push_ret = obj.func_var_defs_.push(var);
        if (!push_ret)
        {
            tokens.pos = startpos;
            return std::unexpected(error::doubledefined);
        }
        tokens.pos += 2; // 跳过标识符和分号
    }
    else
    {
        tokens.pos = startpos;
        return std::unexpected(error::illageid);
    }

    return true;
}

std::expected<bool, error> Complier::try_parse_left_var_and_get_addr(Tokens& tokens, obj& obj)
{
    //[TODO]数组的声明
    auto name = tokens.now().content;
    tokens.pos++;
    auto ret = obj.func_var_defs_.find(name);
    if (ret)
    {
        obj.pushASM(VM::ASM::LEA, ret.value().addr);
        return true;
    }
    ret = obj.global_var_defs_.find(name);
    if (ret)
    {
        obj.pushASM(VM::ASM::IMM, ret.value().addr);
        return true;
    }
    tokens.pos--;
    return std::unexpected(error::expected_left_value);
}

std::expected<obj, error> Complier::process(std::vector<std::string> args)
{
    std::vector<obj> objs;
    objs.reserve(args.size());
    for (auto& each : args)
    {
        auto ret = eachFile(each);
        if (!ret)
        {
            return std::unexpected(error::failed);
        }
        objs.push_back(ret.value());
    }

    if (objs.empty())
    {
        std::cerr << "No objects to process" << std::endl;
        return std::unexpected(error::failed);
    }

    // 输出字符串格式的汇编代码（原来的格式）
    // std::stack<std::string> tp;
    // auto size = objs[0].content.size();
    // for (auto i = 0; i < size; objs[0].content.pop(), i++)
    // {
    //     tp.push(objs[0].content.top());
    // }
    // for (auto i = 0; i < size; i++, tp.pop())
    // {
    //     std::cout << tp.top() << '\n';
    // }

    // 链接阶段 - 这里可以实现链接逻辑
    // 例如：合并所有 obj 的符号表、解析外部引用等
    return objs[0];
}

std::expected<obj, error> Complier::eachFile(std::string path)
{
    obj obj;

    // 为程序入口点预留空间
    obj.pushASM(VM::ASM::HOLD); // 临时占位，稍后分配globalvar
    obj.pushASM(VM::ASM::HOLD); // 临时占位，稍后更新main地址
    obj.pushASM(VM::ASM::HOLD); // 临时占位，稍后ret

    Preprocessor p;
    auto ret = p.process(path, path + ".pre");
    if (!ret)
    {
        return std::unexpected(ret.error());
    }
    auto maytokens = Tokenprocessor::process(path + ".pre");
    if (!maytokens)
    {
        return std::unexpected(maytokens.error());
    }
    auto tokens = maytokens.value();

    while (!tokens.prase_over())
    {
        int startpos = tokens.pos;

        // 尝试解析变量声明
        if (auto result = try_parse_global_var(tokens, obj))
        {
            continue;
        }

        tokens.pos = startpos;

        // 尝试解析函数定义
        if (auto result = try_parse_fun(tokens, obj))
        {
            continue;
        }

        // 如果都解析失败，跳过当前 token
        tokens.pos++;
        if (tokens.prase_over())
        {
            std::cerr << "fail!\n";
            break;
        }
    }

    auto gsize = obj.global_var_defs_.get_max_size();
    obj.content[0] =
        std::format("UP {}", gsize); // 防止全局为空影响ret时对bp的判断->使用exit指令而非依赖bp值

    // 更新程序入口点的跳转地址
    auto main_fun = obj.fun_defs_.find("main");
    if (main_fun)
    {
        // obj.content[1] = std::format("call {}", main_fun.value().addr);
        obj.content[1] = std::format("CALL {}", main_fun.value().addr);
        obj.content[2] = std::format("EXIT");
    }
    return obj;
}