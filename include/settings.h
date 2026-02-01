#pragma once
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "typed_options.hpp"

// 业务相关的选项定义（完整信息：名称、描述、默认值、隐式值）
namespace app_options {
    using namespace typed_options;
    
    inline constexpr Option<bool> help{
        "help",
        "produce help message"
    };

    inline constexpr Option<double> compression{
        "compression",
        "set compression level"
    };

    inline constexpr Option<bool> show_ast{
        "showASt,show-ast",
        "show AST",
        false,  // default_value
        true    // implicit_value
    };

    inline constexpr Option<int> tolerate{
        "tolerate",
        "tolerance for AST printing",
        4       // default_value
    };

    inline constexpr Option<bool> show_folded_names{
        "showFoldedNames,show-folded-names",
        "show folded names in AST",
        false,
        true
    };

    inline constexpr Option<bool> enable_debug{
        "enable_debug,enable-debug",
        "enable VM debug",
        false,
        true
    };
    
    inline constexpr Option<bool> disable_std{
        "disable_std,disable-std",
        "disable stdlib",
        false,
        true
    };

    inline constexpr Option<bool> print_asm{
        "printasm,print-asm",
        "print asm during debug/run",
        false,
        true
    };

    inline constexpr Option<bool> print_ret_value{
        "printretvalue,print-ret-value",
        "print return value after run",
        false,
        true
    };

    inline constexpr Option<std::vector<std::string>> input_files{
        "input-files",
        "input files"
    };
}

std::optional<
    std::pair<boost::program_options::variables_map, boost::program_options::options_description>>
getsetting(int argc, const char* argv[])
{
    namespace po = boost::program_options;
    try
    {
        po::variables_map cli;

        po::options_description visible("Allowed options");
        
        // 使用 app_options 中的定义来构建选项（自动应用所有配置）
        using namespace app_options;
        typed_options::add_option(visible, help);
        typed_options::add_option(visible, compression);
        typed_options::add_option(visible, show_ast);
        typed_options::add_option(visible, tolerate);
        typed_options::add_option(visible, show_folded_names);
        typed_options::add_option(visible, enable_debug);
        typed_options::add_option(visible, print_asm);
        typed_options::add_option(visible, print_ret_value);
        typed_options::add_option(visible, disable_std);

        po::options_description hidden("Hidden options");
        typed_options::add_option(hidden, input_files);

        po::options_description all;
        all.add(visible).add(hidden);

        po::positional_options_description pos;
        pos.add("input-files", -1); // -1 表示接受无限个

        po::store(po::command_line_parser(argc, argv).options(all).positional(pos).run(), cli);
        po::notify(cli);
        return std::pair{cli, visible};
    }
    catch (std::exception& e)
    {
        std::cerr << "error: " << e.what() << "\n";
        return std::nullopt;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!\n";
        return std::nullopt;
    }
}