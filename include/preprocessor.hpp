#pragma once
#include "platform.h"
#include <expected>
#include <format>
#include <string>
#include <vector>
class Preprocessor
{
    const std::vector<std::string> include_paths;
    inline static std::vector<std::string> default_include_paths =
        platform::get_default_include_paths();
  public:
    enum class error
    {
        file_not_exsist,
        can_not_create_file
    };
    Preprocessor(const std::vector<std::string>& include_paths_ = default_include_paths);
    std::expected<bool, error> process(const std::string& src_path, const std::string& out_path);
    std::expected<bool, error> process(const std::string& src_path);
};