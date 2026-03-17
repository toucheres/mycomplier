#include "platform.h"
#include <filesystem>

const std::vector<std::string> platform::get_default_include_dir_paths()
{
    return std::vector<std::string>{get_default_root_paths() + "/stdhead"};
}

const std::vector<std::string> platform::get_default_libc_paths()
{
    std::vector<std::string> libcs{};
    get_default_root_paths() + "/stdlibc";
    std::filesystem::path libcPath = get_default_root_paths() + "/stdlibc";
    for (const auto &entry : std::filesystem::directory_iterator(libcPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".c")
        {
            libcs.push_back(entry.path().string());
        }
    }
    return libcs;
}

const std::string platform::get_default_root_paths()
{
    return std::filesystem::current_path() / "..";
}
