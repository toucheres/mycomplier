#pragma once
#include <boost/program_options.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
std::optional<
    std::pair<boost::program_options::variables_map, boost::program_options::options_description>>
getsetting(int argc, const char* argv[])
{
    namespace po = boost::program_options;
    try
    {
        po::variables_map cli;

        po::options_description visible("Allowed options");
        visible.add_options()
            ("help", "produce help message")
            ("compression", po::value<double>(), "set compression level")
            ("showASt,show-ast", po::value<bool>()->default_value(false)->implicit_value(true), "show AST")
            ("tolerate", po::value<int>()->default_value(4), "tolerance for AST printing")
            ("showFoldedNames,show-folded-names", po::value<bool>()->default_value(false)->implicit_value(true), "show folded names in AST")
            ("enable_debug,enable-debug,enable_debug", po::value<bool>()->default_value(false)->implicit_value(true), "enable VM debug");
            
            visible.add_options()
            ("printasm,print-asm", po::value<bool>()->default_value(false)->implicit_value(true), "print asm during debug/run")
            ("printretvalue,print-ret-value", po::value<bool>()->default_value(false)->implicit_value(true), "print return value after run");

        po::options_description hidden("Hidden options");
        hidden.add_options()("input-files", po::value<std::vector<std::string>>(),
                             "input files");

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