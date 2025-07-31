#include "preprocessor.hpp"
#include "file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
std::expected<file, error> Preprocessor::deal_include(file src)
{
    std::string out;
    std::string line;

    // 处理include
    while (src.readline(line))
    {
        std::smatch match;
        std::regex preprocessor_regex(R"(^\s*#\s*(\w+)\s*(.*))");
        auto ret = std::regex_search(line, match, preprocessor_regex);
        if (ret)
        {
            std::string directive = match[1];
            std::string arg = match[2];
            if (directive == "include")
            {
                std::string filename = arg.substr(1, arg.size() - 2);
                auto fun_find_file_in_dirs =
                    [](const std::vector<std::string>& dirs, const std::string& filename)
                {
                    for (const auto& dir : dirs)
                    {
                        std::filesystem::path file_path = std::filesystem::path(dir) / filename;
                        if (std::filesystem::exists(file_path))
                        {
                            return file_path.string();
                        }
                    }
                    return std::string("");
                };
                auto path = fun_find_file_in_dirs(this->include_paths, filename);
                if (path == "")
                {
                    return std::unexpected(error::file_not_exsist);
                }
                file tp{path};
                std::string f{};
                tp.readalllast(f);
                src.insert("\n" + f + "\n");
            }
            else
            {
                out += line;
                out += '\n';
            }
        }
        else
        {
            out += line;
            out += '\n';
        }
    }
    return file{out, true};
}
std::expected<file, error> Preprocessor::deal_des(file in)
{
    std::string src;
    in.readalllast(src);
    std::regex comment_regex(R"(\/\/.*|\/\*[\s\S]*?\*\/)");
    std::string result = std::regex_replace(src, comment_regex, "");
    return file{result, true};
}
std::expected<file, error> Preprocessor::deal_def(file src)
{
    std::string out;
    std::string line;
    std::vector<std::pair<std::string, std::string>> defines;
    bool skip = false;
    while (src.readline(line))
    {
        std::smatch match;
        std::regex preprocessor_regex(R"(^\s*#\s*(\w+)\s*(\w+)?\s*(.*))");
        auto ret = std::regex_search(line, match, preprocessor_regex);
        if (ret)
        {
            std::string directive = match[1];
            std::string key = match[2];
            std::string val = match[3];
            if (directive == "define")
            {
                if (!key.empty())
                    defines.push_back({key, val});
            }
            else if (directive == "undef")
            {
                defines.erase(std::remove_if(defines.begin(), defines.end(),
                                             [&](const auto& def) { return def.first == key; }),
                              defines.end());
            }
            else if (directive == "ifdef")
            {
                skip = std::find_if(defines.begin(), defines.end(), [&](const auto& def)
                                    { return def.first == key; }) == defines.end();
            }
            else if (directive == "ifndef")
            {
                skip = std::find_if(defines.begin(), defines.end(), [&](const auto& def)
                                    { return def.first == key; }) != defines.end();
            }
            else if (directive == "endif")
            {
                skip = false;
            }
            else
            {
                if (!skip)
                {
                    // 替换宏
                    for (const auto& def : defines)
                    {
                        size_t pos = 0;
                        while ((pos = line.find(def.first, pos)) != std::string::npos)
                        {
                            line.replace(pos, def.first.length(), def.second);
                            pos += def.second.length();
                        }
                    }
                    out += line;
                    out += '\n';
                }
            }
        }
        else
        {
            if (!skip)
            {
                for (const auto& def : defines)
                {
                    size_t pos = 0;
                    while ((pos = line.find(def.first, pos)) != std::string::npos)
                    {
                        line.replace(pos, def.first.length(), def.second);
                        pos += def.second.length();
                    }
                }
                out += line;
                out += '\n';
            }
        }
    }
    return file{out, true};
}
Preprocessor::Preprocessor(const std::vector<std::string>& include_paths_)
    : include_paths(include_paths_)
{
}
std::expected<bool, error> Preprocessor::process(const std::string& src_path,
                                                 const std::string& out_path)
{
    auto result = deal_include(file{src_path});
    if (!result)
    {
        // 错误处理
        return std::unexpected(result.error());
    }
    file without_include = result.value();
    auto res = deal_des(without_include);
    if (!res)
    {
        // 错误处理
        return std::unexpected(res.error());
    }
    file without_include_des = res.value();
    auto res2 = deal_def(without_include_des);
    if (!res2)
    {
        // 错误处理
        return std::unexpected(res.error());
    }
    res2.value().writeto(out_path);
    return true;
}
std::expected<bool, error> Preprocessor::process(const std::string& src_path)
{
    return this->process(src_path, src_path + ".pre");
}
