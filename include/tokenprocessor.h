#pragma once
#include <expected>
#include <string>
#include <vector>
struct Token
{
    std::string content;
    bool can_be_name();
};
using Tokens = std::vector<std::string>;
class TokenStream
{
    Tokens tokens;
    size_t pos = 0;

  public:
    TokenStream(Tokens tokens);
};
class Tokenprocessor
{
    enum class error
    {
        file_not_exsist,
        can_not_create_file
    };

  public:
    static std::expected<Tokens, error> process(const std::string& path);
};