#include <iostream>

#include <boost/program_options.hpp>

#include "complier.hpp"
#include "settings.h"
#include "vm.h"
namespace po = boost::program_options;

int main(int argc, const char* argv[])
{
    auto opcli = getsetting(argc, argv);
    if (!opcli)
    {
        return -1;
    }
    auto [cli, des] = *opcli;
    if (cli.count("help"))
    {
        std::cout << des << '\n';
    }
    if (cli.count("input-files"))
    {
        // 从命令行读取新的选项
        bool showASt = false;
        int tolerate = 4;
        bool showFoldedNames = false;
        bool enable_debug = false;
        bool printasm = false;
        bool printretvalue = false;

        if (cli.count("showASt"))
            showASt = cli["showASt"].as<bool>();
        else if (cli.count("show-ast"))
            showASt = cli["show-ast"].as<bool>();

        if (cli.count("tolerate"))
            tolerate = cli["tolerate"].as<int>();

        if (cli.count("showFoldedNames"))
            showFoldedNames = cli["showFoldedNames"].as<bool>();
        else if (cli.count("show-folded-names"))
            showFoldedNames = cli["show-folded-names"].as<bool>();

        if (cli.count("enable_debug"))
            enable_debug = cli["enable_debug"].as<bool>();
        else if (cli.count("enable-debug"))
            enable_debug = cli["enable-debug"].as<bool>();

        if (cli.count("printasm") || cli.count("print-asm"))
        {
            if (cli.count("printasm"))
                printasm = cli["printasm"].as<bool>();
            else
                printasm = cli["print-asm"].as<bool>();
        }

        if (cli.count("printretvalue") || cli.count("print-ret-value"))
        {
            if (cli.count("printretvalue"))
                printretvalue = cli["printretvalue"].as<bool>();
            else
                printretvalue = cli["print-ret-value"].as<bool>();
        }

        auto ret = complier::process(cli["input-files"].as<std::vector<std::string>>(), showASt, tolerate, showFoldedNames);
        if (!ret)
        {
            std::cout << "error\n";
        }
        else
        {
            if (printasm)
            {
                for (int i = 0; i < ret.value().size(); i++)
                {
                    std::cout << "[" << i << "]:" << ret.value()[i] << '\n';
                }
            }
        }
        VM vm{ret.value()};
        vm.enable_debug = enable_debug;
        vm.print_asm = printasm;
        vm.print_ret_value = printretvalue;
        auto retval = vm.run();
        if (retval)
        {
            if (vm.print_ret_value)
                std::cout << "ret: " << retval.value() << " = " << std::hex << retval.value() << '\n';
            return retval.value();
        }
        else
        {
            std::cout << "error\n";
            return -1;
        }
    }
    return 0;
}