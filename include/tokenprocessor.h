#pragma once
#include <expected>
#include <string>
#include <vector>
using Tokens = std::vector<std::string>;
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