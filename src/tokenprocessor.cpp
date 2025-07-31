#include "tokenprocessor.h"
#include "file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
std::expected<Tokens, error> Tokenprocessor::process(const std::string& src_path)
{
    file in{src_path};
    std::regex token_regex(
        R"([a-zA-Z_][a-zA-Z0-9_]*|\d+|\".*?\"|\'.*?\'|==|!=|<=|>=|&&|\|\||[{}()\[\];,<>+\-*/%=&|^!~])");
    std::string src;
    in.readalllast(src); // 预处理后的文本
    Tokens tokens;
    auto begin = std::sregex_iterator(src.begin(), src.end(), token_regex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it)
    {
        tokens.push_back(Token{it->str()});
    }
    return tokens;
}