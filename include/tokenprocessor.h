#pragma once
#include "enums.h"
#include "error.hpp"
#include <expected>
#include <optional>
#include <string>
#include <vector>
struct Token
{
    std::string content;
    bool can_be_id();
    std::optional<Basic_Type> is_type();
};
// using Tokens = std::vector<Token>;
class Tokens : public std::vector<Token>
{
  public:
    int pos = 0;
    // int last_pos = 0;
    // inline void save()
    // {
    //     last_pos = pos;
    // }
    // inline void load()
    // {
    //     pos = last_pos;
    // }
    bool prase_over()
    {
        return pos >= size();
    };
    Token& now()
    {
        return (*this)[pos];
    };
};
class TokenStream
{
    Tokens tokens;
    int pos = 0;

  public:
    TokenStream(Tokens tokens);
};
class Tokenprocessor
{
  public:
    static std::expected<Tokens, error> process(const std::string& path);
};