#include "preprocessor.hpp"
#include "file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_map>
#include <sstream>
#include <optional>
#include <cctype>

// 辅助函数：检查是否是标识符字符
static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// 辅助函数：去除字符串前后空白
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 宏定义结构
struct MacroDef {
    std::string name;
    std::vector<std::string> params;
    std::string replacement;
    bool is_function_like = false;
};

// 解析函数式宏定义
static std::optional<MacroDef> parse_macro_define(const std::string& line) {
    // 匹配 #define NAME... 
    std::regex def_regex(R"(^\s*#\s*define\s+(\w+))");
    std::smatch m;
    if (!std::regex_search(line, m, def_regex))
        return std::nullopt;
    
    MacroDef macro;
    macro.name = m[1];
    
    // 找到宏名后的位置
    size_t name_end = m.position(1) + m.length(1);
    std::string rest = line.substr(name_end);
    
    // 检查是否紧跟 '(' (函数式宏)
    if (!rest.empty() && rest[0] == '(') {
        macro.is_function_like = true;
        
        // 找匹配的右括号
        int depth = 1;
        size_t i = 1;
        while (i < rest.size() && depth > 0) {
            if (rest[i] == '(') depth++;
            else if (rest[i] == ')') depth--;
            i++;
        }
        if (depth != 0) return std::nullopt;
        
        // 解析参数列表
        std::string params_str = rest.substr(1, i - 2);
        std::istringstream iss(params_str);
        std::string param;
        while (std::getline(iss, param, ',')) {
            param = trim(param);
            if (!param.empty())
                macro.params.push_back(param);
        }
        
        // 剩余部分是替换文本
        macro.replacement = trim(rest.substr(i));
    } else {
        // 对象式宏
        macro.replacement = trim(rest);
    }
    
    return macro;
}

// 展开函数式宏调用
static std::string expand_function_macro(const std::string& text, const MacroDef& macro) {
    std::string result = text;
    size_t search_pos = 0;
    
    while (search_pos < result.size()) {
        // 查找宏名
        size_t pos = result.find(macro.name, search_pos);
        if (pos == std::string::npos) break;
        
        // 检查单词边界（前）
        if (pos > 0 && is_ident_char(result[pos - 1])) {
            search_pos = pos + 1;
            continue;
        }
        
        // 检查宏名后是否紧跟 '('
        size_t after_name = pos + macro.name.length();
        if (after_name >= result.size() || result[after_name] != '(') {
            search_pos = pos + 1;
            continue;
        }
        
        // 找匹配的右括号
        size_t paren_start = after_name;
        int depth = 1;
        size_t i = paren_start + 1;
        while (i < result.size() && depth > 0) {
            if (result[i] == '(') depth++;
            else if (result[i] == ')') depth--;
            i++;
        }
        if (depth != 0) {
            search_pos = pos + 1;
            continue;
        }
        size_t paren_end = i; // 指向 ')' 之后
        
        // 提取参数 (处理嵌套括号和逗号)
        std::string args_content = result.substr(paren_start + 1, paren_end - paren_start - 2);
        std::vector<std::string> args;
        std::string cur_arg;
        int arg_depth = 0;
        
        for (char ch : args_content) {
            if (ch == ',' && arg_depth == 0) {
                args.push_back(trim(cur_arg));
                cur_arg.clear();
            } else {
                if (ch == '(') arg_depth++;
                else if (ch == ')') arg_depth--;
                cur_arg += ch;
            }
        }
        // 最后一个参数（或唯一参数）
        std::string last_arg = trim(cur_arg);
        if (!last_arg.empty() || !args.empty()) {
            args.push_back(last_arg);
        }
        
        // 无参数宏调用时 args 可能为空
        if (macro.params.empty() && args.size() == 1 && args[0].empty()) {
            args.clear();
        }
        
        // 检查参数数量
        if (args.size() != macro.params.size()) {
            search_pos = pos + 1;
            continue;
        }
        
        // 执行参数替换
        std::string replacement = macro.replacement;
        for (size_t pi = 0; pi < macro.params.size(); pi++) {
            const std::string& param = macro.params[pi];
            const std::string& arg = args[pi];
            
            size_t rpos = 0;
            while ((rpos = replacement.find(param, rpos)) != std::string::npos) {
                // 检查单词边界
                bool start_ok = (rpos == 0 || !is_ident_char(replacement[rpos - 1]));
                bool end_ok = (rpos + param.length() >= replacement.length() ||
                              !is_ident_char(replacement[rpos + param.length()]));
                
                if (start_ok && end_ok) {
                    replacement.replace(rpos, param.length(), arg);
                    rpos += arg.length();
                } else {
                    rpos++;
                }
            }
        }
        
        // 替换整个宏调用
        result.replace(pos, paren_end - pos, replacement);
        search_pos = pos + replacement.length();
    }
    
    return result;
}

// 展开对象式宏
static std::string expand_object_macro(const std::string& text, const MacroDef& macro) {
    std::string result = text;
    size_t pos = 0;
    
    while ((pos = result.find(macro.name, pos)) != std::string::npos) {
        bool start_ok = (pos == 0 || !is_ident_char(result[pos - 1]));
        bool end_ok = (pos + macro.name.length() >= result.length() ||
                      !is_ident_char(result[pos + macro.name.length()]));
        
        if (start_ok && end_ok) {
            result.replace(pos, macro.name.length(), macro.replacement);
            pos += macro.replacement.length();
        } else {
            pos++;
        }
    }
    
    return result;
}

// 对一行应用所有宏展开（可能需要多轮）
static std::string expand_all_macros(const std::string& line, const std::vector<MacroDef>& defines) {
    std::string result = line;
    bool changed = true;
    int max_iterations = 100; // 防止无限递归
    
    while (changed && max_iterations-- > 0) {
        changed = false;
        for (const auto& macro : defines) {
            std::string new_result;
            if (macro.is_function_like) {
                new_result = expand_function_macro(result, macro);
            } else {
                new_result = expand_object_macro(result, macro);
            }
            if (new_result != result) {
                result = new_result;
                changed = true;
            }
        }
    }
    
    return result;
}

// 处理行尾反斜杠续行：将以 '\' 结尾的行与下一行合并
std::expected<file, error> Preprocessor::deal_line_continuation(file src)
{
    std::string content;
    src.readalllast(content);
    
    std::string result;
    result.reserve(content.size());
    
    size_t i = 0;
    while (i < content.size()) {
        if (content[i] == '\\') {
            // 检查是否是行尾反斜杠
            size_t next = i + 1;
            
            // 跳过反斜杠后的空白（某些实现允许 \ 后有空格）
            while (next < content.size() && (content[next] == ' ' || content[next] == '\t')) {
                next++;
            }
            
            // 检查是否紧跟换行符
            if (next < content.size() && content[next] == '\n') {
                // 跳过反斜杠和换行，继续下一行
                i = next + 1;
                continue;
            } else if (next + 1 < content.size() && content[next] == '\r' && content[next + 1] == '\n') {
                // Windows 风格换行 \r\n
                i = next + 2;
                continue;
            }
        }
        
        result += content[i];
        i++;
    }
    
    return file{result, true};
}

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
                // 对被 include 的文件也进行续行处理
                auto included_result = deal_line_continuation(file{path});
                if (!included_result)
                {
                    return std::unexpected(included_result.error());
                }
                std::string f{};
                included_result.value().readalllast(f);
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
    std::vector<MacroDef> defines;
    bool skip = false;
    
    while (src.readline(line))
    {
        // 检查是否是预处理指令
        std::regex directive_regex(R"(^\s*#\s*(\w+))");
        std::smatch dmatch;
        
        if (std::regex_search(line, dmatch, directive_regex))
        {
            std::string directive = dmatch[1];
            
            if (directive == "define")
            {
                auto macro_opt = parse_macro_define(line);
                if (macro_opt.has_value()) {
                    defines.push_back(macro_opt.value());
                }
                // define 行不输出
            }
            else if (directive == "undef")
            {
                // 提取要 undef 的名字
                std::regex undef_regex(R"(^\s*#\s*undef\s+(\w+))");
                std::smatch um;
                if (std::regex_search(line, um, undef_regex)) {
                    std::string name = um[1];
                    defines.erase(
                        std::remove_if(defines.begin(), defines.end(),
                                      [&](const MacroDef& d) { return d.name == name; }),
                        defines.end());
                }
            }
            else if (directive == "ifdef")
            {
                std::regex ifdef_regex(R"(^\s*#\s*ifdef\s+(\w+))");
                std::smatch im;
                if (std::regex_search(line, im, ifdef_regex)) {
                    std::string name = im[1];
                    bool defined = std::any_of(defines.begin(), defines.end(),
                                              [&](const MacroDef& d) { return d.name == name; });
                    skip = !defined;
                }
            }
            else if (directive == "ifndef")
            {
                std::regex ifndef_regex(R"(^\s*#\s*ifndef\s+(\w+))");
                std::smatch nm;
                if (std::regex_search(line, nm, ifndef_regex)) {
                    std::string name = nm[1];
                    bool defined = std::any_of(defines.begin(), defines.end(),
                                              [&](const MacroDef& d) { return d.name == name; });
                    skip = defined;
                }
            }
            else if (directive == "endif")
            {
                skip = false;
            }
            else
            {
                // 其他预处理指令，如果不跳过则输出
                if (!skip) {
                    out += expand_all_macros(line, defines);
                    out += '\n';
                }
            }
        }
        else
        {
            // 普通代码行
            if (!skip)
            {
                out += expand_all_macros(line, defines);
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
    // 1. 首先处理行尾反斜杠续行
    auto line_cont_result = deal_line_continuation(file{src_path});
    if (!line_cont_result)
    {
        return std::unexpected(line_cont_result.error());
    }
    file after_line_continuation = line_cont_result.value();
    
    // 2. 处理 include
    auto result = deal_include(after_line_continuation);
    if (!result)
    {
        // 错误处理
        return std::unexpected(result.error());
    }
    file without_include = result.value();
    
    // 3. 处理注释
    auto res = deal_des(without_include);
    if (!res)
    {
        // 错误处理
        return std::unexpected(res.error());
    }
    file without_include_des = res.value();
    
    // 4. 处理宏定义
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
