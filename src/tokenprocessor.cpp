#include "tokenprocessor.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
std::expected<Tokens, Tokenprocessor::error> Tokenprocessor::process(const std::string& src_path)
{
    std::ifstream in(src_path);
    if (!in.is_open())
    {
        // 打开失败
        return std::unexpected(error::file_not_exsist);
    }
    in.seekg(0, std::ios::end);
    size_t char_count = in.tellg();
    in.seekg(0, std::ios::beg); // 若后续还要读内容
    std::vector<std::string> result;
    result.reserve(char_count / 4);
    std::string token;
    while (in >> token)
    {
        result.push_back(token);
    }
    return result;
}