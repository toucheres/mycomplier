#pragma once
#include "file.hpp"
#include "platform.h"
#include <expected>
#include <format>
#include <string>
#include <vector>
#include "error.hpp"
// [TODO] 以token为单位解析
class Preprocessor
{
  public:

  private:
    const std::vector<std::string> include_paths;
    inline static std::vector<std::string> default_include_paths =
        platform::get_default_include_paths();
    std::expected<file, error> deal_line_continuation(file in); // 处理行尾反斜杠续行
    std::expected<file, error> deal_include(file in);
    std::expected<file, error> deal_des(file in);
    std::expected<file, error> deal_def(file in);

  public:
    Preprocessor(const std::vector<std::string>& include_paths_ = default_include_paths);
    std::expected<bool, error> process(const std::string& src_path, const std::string& out_path);
    std::expected<bool, error> process(const std::string& src_path);
};