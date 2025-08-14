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
    tokens.save();
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
        tokens.load();
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
        tokens.load();
        return std::unexpected(error::illageid);
    }
    // 为函数创建新的作用域
    obj.func_var_defs_.clear();
    obj.func_var_defs_.into_new_namespace();
    thisfun.addr = obj.content.size();
    auto ret = try_parse_args(tokens, obj);
    if (!ret)
    {
        obj.func_var_defs_.into_new_namespace();
        tokens.load();
        return std::unexpected(ret.error());
    }
    else
    {
        thisfun.argtypes = ret.value();
        // 为返回地址预留
        var_def retaddr{};
        retaddr.id = "returnaddr";
        retaddr.type = Type{Basic_Type::INT};
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

        tokens.load();
        return std::unexpected(ret2.error());
    }
    obj.func_var_defs_.outto_old_namespace();
    obj.content[nvar_pos - 1] = std::format("NVAR {}", obj.func_var_defs_.get_max_size());
    // 函数解析完成，退出函数作用域
    thisfun.defined = true;
    // 注意：不在这里生成 RET，因为函数体中的 return 语句会生成
    // 如果函数没有显式 return，编译器应该在语义分析阶段处理
    obj.fun_defs_.push(thisfun);
    return true;
}

std::expected<bool, error> Complier::try_parse_block(Tokens& tokens, obj& obj)
{
    tokens.save();
    if (!(tokens.now().content == "{"))
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++; // 跳过开始的 '{'

    bool flag = false;
    do
    {
        flag = false;
        tokens.save();
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
        tokens.load();

        // 尝试解析 if 语句
        if (auto ret = try_parse_if(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.load();

        // 尝试解析 while 语句
        if (auto ret = try_parse_while(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.load();

        // 尝试解析 return 语句
        if (auto ret = try_parse_return(tokens, obj))
        {
            flag = true;
            continue;
        }
        tokens.load();

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
        tokens.load();

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
    tokens.save();
    if (tokens.now().content != "(")
    {
        tokens.load();
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
            tokens.load();
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
            tokens.load();
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
            tokens.load();
            return std::unexpected(error::expected_fenhao);
        }
    } while (true);
}

std::expected<bool, error> Complier::try_parse_while(Tokens& tokens, obj& obj)
{
    tokens.save();
    if (tokens.now().content != "while")
    {
        tokens.load();
        return std::unexpected(error::expected_while);
    }
    tokens.pos++;

    // 解析条件表达式 (expression)
    if (tokens.now().content != "(")
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    auto ret = try_parse_expr(tokens, obj);
    if (!ret)
    {
        tokens.load();
        return std::unexpected(ret.error());
    }

    if (tokens.now().content != ")")
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 解析循环体
    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        tokens.load();
        return std::unexpected(ret2.error());
    }

    return true;
}

std::expected<bool, error> Complier::try_parse_if(Tokens& tokens, obj& obj)
{
    tokens.save();
    if (tokens.now().content != "if")
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 解析条件表达式 (expression)
    if (tokens.now().content != "(")
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    auto ret = try_parse_expr(tokens, obj);
    if (!ret)
    {
        tokens.load();
        return std::unexpected(ret.error());
    }

    if (tokens.now().content != ")")
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 解析 then 分支
    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        tokens.load();
        return std::unexpected(ret2.error());
    }

    // 可选的 else 分支
    if (tokens.now().content == "else")
    {
        tokens.pos++;
        auto ret3 = try_parse_block(tokens, obj);
        if (!ret3)
        {
            tokens.load();
            return std::unexpected(ret3.error());
        }
    }

    return true;
}

std::expected<bool, error> Complier::try_parse_return(Tokens& tokens, obj& obj)
{
    tokens.save();
    if (tokens.now().content != "return")
    {
        tokens.load();
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
            tokens.load();
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
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }
    tokens.pos++;

    // 生成返回指令 (表达式的结果已经在栈顶，直接返回)
    obj.pushASM(VM::ASM::RET);

    return true;
}

std::expected<bool, error> Complier::try_parse_expr(Tokens& tokens, obj& obj)
{
    tokens.save();

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }

    // 解析赋值表达式（最低优先级）
    auto ret = try_parse_assignment_expr(tokens, obj);
    if (!ret)
    {
        tokens.load();
        return std::unexpected(ret.error());
    }

    return true;
}

// 解析赋值表达式
std::expected<bool, error> Complier::try_parse_assignment_expr(Tokens& tokens, obj& obj)
{
    tokens.save();

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
            tokens.load();
            return ret;
        }

        // 查找变量地址并生成存储指令
        auto var_result = obj.func_var_defs_.find(var_id);
        if (var_result)
        {
            obj.pushASM(VM::ASM::LEA, var_result.value()->addr); // 取bp+bias
            obj.pushASM(VM::ASM::SI);                            // 存储到指定地址
        }
        else
        {
            var_result = obj.global_var_defs_.find(var_id);
            if (var_result)
            {
                obj.pushASM(VM::ASM::SI, var_result.value()->addr);
            }
            else
            {
                return std::unexpected(error::undefinedvar);
            }
        }

        return true;
    }

    // 不是赋值表达式，按普通表达式处理
    tokens.load();
    return try_parse_logical_or_expr(tokens, obj);
}

// 解析逻辑或表达式
std::expected<bool, error> Complier::try_parse_logical_or_expr(Tokens& tokens, obj& obj)
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

        auto ret = try_parse_unary_expr(tokens, obj); // 递归处理嵌套一元运算符
        if (!ret)
            return ret;

        // 生成一元运算符汇编
        if (op == "*")
        {
            // 解引用：从地址加载值
            obj.pushASM(VM::ASM::LI);
        }
        else if (op == "&")
        {
            // 取地址：获取变量地址
            obj.pushASM(VM::ASM::LEA);
        }
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
    tokens.save();

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.load();
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
                        tokens.load();
                        return std::unexpected(ret.error());
                    }
                    arg_count++;

                    if (tokens.prase_over())
                    {
                        tokens.load();
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
                        tokens.load();
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
                tokens.load();
                return std::unexpected(error::expected_fenhao);
            }

            // 生成函数调用汇编
            auto fun_result = obj.fun_defs_.find(id);
            if (fun_result)
            {
                obj.pushASM(VM::ASM::CALL, fun_result.value().addr); // 调用指定地址的函数
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
                obj.pushASM(VM::ASM::LEA, var_result.value()->addr); // 压入bp+局部变量偏移
                obj.pushASM(VM::ASM::LI);                            // 加载指定地址的变量值
            }
            else
            {
                // func局部如果找不到变量，尝试全局
                var_result = obj.global_var_defs_.find(id);
                if (var_result)
                {
                    obj.pushASM(VM::ASM::LI, var_result.value()->addr); // 加载变量值
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
            tokens.load();
            return std::unexpected(ret.error());
        }

        if (tokens.prase_over() || tokens.now().content != ")")
        {
            tokens.load();
            return std::unexpected(error::expected_fenhao);
        }
        tokens.pos++;
        return true;
    }

    tokens.load();
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

void Complier::generate_binary_op_asm(const std::string& op, obj& obj)
{
    if (op == "+")
        obj.pushASM(VM::ASM::ADD);
    else if (op == "-")
        obj.pushASM(VM::ASM::SUB);
    else if (op == "*")
        obj.pushASM(VM::ASM::MUL);
    else if (op == "/")
        obj.pushASM(VM::ASM::DIV);
    else if (op == "==")
        obj.pushASM(VM::ASM::EQ);
    else if (op == "!=")
        obj.pushASM(VM::ASM::NE);
    else if (op == "<")
        obj.pushASM(VM::ASM::LT);
    else if (op == ">")
        obj.pushASM(VM::ASM::GT);
    else if (op == "<=")
        obj.pushASM(VM::ASM::LE);
    else if (op == ">=")
        obj.pushASM(VM::ASM::GE);
    else if (op == "=")
        obj.pushASM(VM::ASM::SI); // 赋值
}

std::expected<bool, error> Complier::try_parse_global_var(Tokens& tokens, obj& obj)
{
    tokens.save();

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.load();
        return std::unexpected(error::unkowntype);
    }

    auto ret = tokens.now().is_type();
    if (!ret)
    {
        tokens.load();
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
        auto push_ret = obj.global_var_defs_.push(var);
        if (!push_ret)
        {
            tokens.load();
            return std::unexpected(error::doubledefined);
        }
        tokens.pos += 2; // 跳过标识符和分号
    }
    else
    {
        tokens.load();
        return std::unexpected(error::illageid);
    }

    return true;
}

std::expected<bool, error> Complier::try_parse_func_var(Tokens& tokens, obj& obj)
{
    tokens.save();

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.load();
        return std::unexpected(error::unkowntype);
    }

    auto ret = tokens.now().is_type();
    if (!ret)
    {
        tokens.load();
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
        auto push_ret = obj.func_var_defs_.push(var);
        if (!push_ret)
        {
            tokens.load();
            return std::unexpected(error::doubledefined);
        }
        tokens.pos += 2; // 跳过标识符和分号
    }
    else
    {
        tokens.load();
        return std::unexpected(error::illageid);
    }

    return true;
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
    obj.pushASM(VM::ASM::JMP, 0); // 临时占位，稍后会更新地址

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
        tokens.save();

        // 尝试解析变量声明
        if (auto result = try_parse_global_var(tokens, obj))
        {
            continue;
        }

        tokens.load();

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

    // 更新程序入口点的跳转地址
    auto main_fun = obj.fun_defs_.find("main");
    if (main_fun)
    {
        // 使用main函数记录的地址，但这次是基于vector下标的
        int main_addr = main_fun.value().addr;

        // 更新第一条指令的参数为main函数地址
        if (!obj.content.empty())
        {
            std::stack<std::string> temp_content;
            std::string first_instruction;

            // 取出第一条指令
            first_instruction = obj.content[0];
            auto ret = first_instruction.find_first_of("JMP");
            if (ret != std::string::npos)
            {
                first_instruction.pop_back();
                first_instruction += std::to_string(main_addr);
                obj.content[0] = first_instruction;
            }
        }
    }
    return obj;
}