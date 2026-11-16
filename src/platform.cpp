#include "platform.h"
#include <cstdlib>
#include <filesystem>

const std::vector<std::string> platform::get_default_include_dir_paths()
{
    return std::vector<std::string>{get_default_root_paths() + "/stdhead"};
}

const std::vector<std::string> platform::get_default_libc_paths()
{
    std::vector<std::string> libcs{};
    const std::filesystem::path libcPath =
        std::filesystem::path(get_default_root_paths()) / "stdlibc";
    std::error_code ec;
    if (!std::filesystem::is_directory(libcPath, ec))
    {
        return libcs;
    }
    for (const auto& entry : std::filesystem::directory_iterator(libcPath, ec))
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
    if (const char* envRoot = std::getenv("MYCOMPLIER_ROOT"))
    {
        return std::string{envRoot};
    }
#ifdef MYCOMPLIER_ROOT
    // Injected by the build system: the source tree that contains stdhead/ and
    // stdlibc/.  This keeps the compiler independent of the current directory.
    return std::string{MYCOMPLIER_ROOT};
#else
    return (std::filesystem::current_path() / "..").string();
#endif
}
