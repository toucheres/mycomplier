#include "complier.hpp"
#include "file.hpp"
#include "preprocessor.hpp"
#include "tokenprocessor.h"
#include <algorithm>
#include <cctype>
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
        tokens.load();
        return std::unexpected(error::illageid);
    }

    // 为函数创建新的作用域
    obj.var_defs_.into_new_namespace();

    auto ret = try_parse_args(tokens, obj);
    if (!ret)
    {
        obj.var_defs_.outto_old_namespace(); // 退出函数作用域
        tokens.load();
        return std::unexpected(ret.error());
    }
    else
    {
        thisfun.argtypes = ret.value();
    }

    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        obj.var_defs_.outto_old_namespace(); // 退出函数作用域
        tokens.load();
        return std::unexpected(ret2.error());
    }

    // 函数解析完成，退出函数作用域
    obj.var_defs_.outto_old_namespace();
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
            obj.var_defs_.into_new_namespace();
            return try_parse_block(tokens, obj);
        }
        // 检查是否到达结束大括号
        if (tokens.now().content == "}")
        {
            tokens.pos++;
            return true;
        }

        // 尝试解析变量声明
        if (auto ret = try_parse_var(tokens, obj))
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
            obj.var_defs_.push_arg(arg);
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

std::expected<bool, error> Complier::try_parse_expr(Tokens& tokens, obj& obj)
{
    tokens.save();

    // 检查边界
    if (tokens.prase_over())
    {
        tokens.load();
        return std::unexpected(error::expected_fenhao);
    }

    // 解析主表达式（primary expression）
    auto ret = try_parse_primary(tokens, obj);
    if (!ret)
    {
        tokens.load();
        return std::unexpected(ret.error());
    }

    // 解析二元运算符表达式
    while (!tokens.prase_over() && is_binary_operator(tokens.now().content))
    {
        std::string op = tokens.now().content;
        tokens.pos++;

        auto ret2 = try_parse_primary(tokens, obj);
        if (!ret2)
        {
            tokens.load();
            return std::unexpected(ret2.error());
        }

        // 生成对应的汇编指令
        generate_binary_op_asm(op, obj);
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
            obj.pushASM(VM::ASM::CALL);
            return true;
        }
        else
        {
            // 变量引用
            obj.pushASM(VM::ASM::LI); // 加载变量值
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

std::expected<bool, error> Complier::try_parse_var(Tokens& tokens, obj& obj)
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
        auto push_ret = obj.var_defs_.push(var);
        if (!push_ret)
        {
            tokens.load();
            return std::unexpected(error::doubledefine);
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

int Complier::process(std::vector<std::string> args)
{
    std::vector<obj> objs;
    objs.reserve(args.size());
    for (auto& each : args)
    {
        auto ret = eachFile(each);
        if (!ret)
        {
            return -1;
        }
        objs.push_back(ret.value());
    }
    // 链接阶段 - 这里可以实现链接逻辑
    // 例如：合并所有 obj 的符号表、解析外部引用等
    return 0;
}

std::expected<obj, error> Complier::eachFile(std::string path)
{
    obj obj;
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
        if (auto result = try_parse_var(tokens, obj))
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
            break;
        }
    }

    return obj;
}