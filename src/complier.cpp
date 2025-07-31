#include "complier.hpp"
#include "file.hpp"
#include "preprocessor.hpp"
#include "tokenprocessor.h"
#include <string>
#include <vector>

std::expected<bool, error> Complier::try_parse_fun(Tokens& tokens, obj& obj)
{
    auto ret = try_parse_args(tokens, obj);
    if (!ret)
    {
        return std::unexpected(ret.error());
    }
    auto ret2 = try_parse_block(tokens, obj);
    if (!ret2)
    {
        return std::unexpected(ret2.error());
    }
    return true;
}

std::expected<bool, error> Complier::try_parse_block(Tokens& tokens, obj& obj)
{
    tokens.save();
    if (!(tokens.now().content == "{"))
    {
        return std::unexpected(error::expected_fenhao);
    }
    bool flag = false;
    do
    {
        flag = false;
        if (auto ret = try_parse_args(tokens, obj))
        {
            flag = true;
        }
        else
        {
            tokens.load();
            return std::unexpected(ret.error());
        }
        if (auto ret = try_parse_if(tokens, obj))
        {
            flag = true;
        }
        else
        {
            tokens.load();
            return std::unexpected(ret.error());
        }
        if (auto ret = try_parse_while(tokens, obj))
        {
            flag = true;
        }
        else
        {
            tokens.load();
            return std::unexpected(ret.error());
        }
        if (auto ret = try_parse_block(tokens, obj))
        {
            flag = true;
        }
        else
        {
            tokens.load();
            return std::unexpected(ret.error());
        }
    } while (flag);
}

std::expected<bool, error> Complier::try_parse_args(Tokens& tokens, obj& obj)
{
    tokens.save();
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
        type.num_lay++;
        tokens.pos++;
    }
    if (tokens.now().can_be_id())
    {
        var_def arg;
        arg.defined = true;
        arg.id = tokens.now().content;
        arg.type = type;
        obj.fun_var_defs.push_arg(arg);
    }
    else
    {
        tokens.load();
        return std::unexpected(error::illageid);
    }
    tokens.pos++;
    if (tokens.now().content == ",")
    {
        tokens.pos++;
        return try_parse_args(tokens, obj);
    }
    else if (tokens.now().content == ")")
    {
        tokens.pos++;
        return true;
    }
}

// std::expected<bool, error> Complier::try_parse_while(Tokens& tokens, obj& obj)
// {
//     tokens.save();
//     if (tokens.now().content == "while")
//     {
//         pos++;
//         try_expression();
//     }
// }

std::expected<bool, error> Complier::try_parse_var(Tokens& tokens, obj& obj)
{
    tokens.save();
    auto ret = tokens[tokens.pos].is_type();
    if (!ret)
    {
        return false;
    }
    else
    {
        Type type{ret.value()};
        tokens.pos++;
        while (tokens.now().content == "*")
        {
            type.num_lay++;
            tokens.pos++;
        }

        if (tokens.now().can_be_id() && tokens[tokens.pos + 1].content == ";")
        {
            var_def var;
            var.id = tokens.now().content;
            var.defined = true;
            var.type = type;
            auto ret = obj.global_var_defs.push(var);
            if (!ret)
            {
                return std::unexpected(error::doubledefine);
            }
        }
        else
        {
            tokens.load();
            return std::unexpected(error::illageid);
        }
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
    // 链接
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
        if (try_parse_var(tokens, obj))
        {
        }
        else if (try_parse_fun(tokens, obj))
        {
        }
    }
}