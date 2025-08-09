#include "tokenprocessor.h"
#include "file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <cctype>

bool Token::can_be_id()
{
    if (content.empty()) return false;
    
    // 检查是否以字母或下划线开头
    if (!std::isalpha(content[0]) && content[0] != '_') return false;
    
    // 检查其余字符是否为字母、数字或下划线
    for (size_t i = 1; i < content.size(); ++i)
    {
        if (!std::isalnum(content[i]) && content[i] != '_') return false;
    }
    
    // 检查是否为关键字
    static const std::unordered_set<std::string> keywords = {
        "int", "char", "if", "else", "while", "for", "return", "void", "struct", "enum", "typedef"
    };
    
    return keywords.find(content) == keywords.end();
}

std::optional<Basic_Type> Token::is_type()
{
    if (content == "int") return Basic_Type::INT;
    return std::nullopt;
}
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