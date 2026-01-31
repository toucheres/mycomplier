#pragma once
#include <string>
#include <vector>

namespace platform
{
    const std::vector<std::string> get_default_include_dir_paths();
    const std::vector<std::string> get_default_libc_paths();
    const std::string get_default_root_paths();
} // namespace platform
