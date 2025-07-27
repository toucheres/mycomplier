#include "preprocessor.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
Preprocessor::Preprocessor(const std::vector<std::string>& include_paths_)
    : include_paths(include_paths_)
{
}

std::expected<bool, Preprocessor::error> Preprocessor::process(const std::string& src_path,
                                                               const std::string& out_path)
{
    // 打开文件进行读取
    std::ifstream in(src_path);
    if (!in.is_open())
    {
        // 打开失败
        return std::unexpected(error::file_not_exsist);
    }

    // 打开文件进行写入
    std::ofstream out(out_path);
    if (!out.is_open())
    {
        // 打开失败
        return std::unexpected(error::can_not_create_file);
    }
    std::string line;
    // 用于保存宏定义
    std::unordered_map<std::string, std::string> defines;
    while (std::getline(in, line))
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
                std::ifstream tp{path};
                if (!tp.is_open())
                {
                    return std::unexpected(error::file_not_exsist);
                }
                std::string inc_line;
                while (std::getline(tp, inc_line))
                {
                    out << inc_line << '\n';
                }
            }
            else if (directive == "define")
            {
                // 解析 define 指令，假设格式为 #define NAME VALUE
                // [TODO]支持递归替换
                std::istringstream iss(arg);
                std::string name, value;
                iss >> name;
                std::getline(iss, value);
                // 去除前导空格
                value.erase(0, value.find_first_not_of(" \t"));
                if (!name.empty())
                    defines[name] = value;
            }
        }
        else
        {
            // 替换宏定义
            for (const auto& def : defines)
            {
                size_t pos = 0;
                while ((pos = line.find(def.first, pos)) != std::string::npos)
                {
                    line.replace(pos, def.first.length(), def.second);
                    pos += def.second.length();
                }
            }
            out << line << '\n';
        }
    }
    out.close();
    return true;
}

std::expected<bool, Preprocessor::error> Preprocessor::process(const std::string& src_path)
{
    return this->process(src_path, src_path + ".pre");
}
