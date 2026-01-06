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
        // std::vector<std::string> args(argv + 1, argv + argc);
        auto ret = complier::process(cli["input-files"].as<std::vector<std::string>>(), true, 4);
        if (!ret)
        {
            std::cout << "error\n";
        }
        else
        {
            for (int i = 0; i < ret.value().size(); i++)
            {
                std::cout << "[" << i << "]:" << ret.value()[i] << '\n';
            }
        }
        VM vm{ret.value()};
        vm.enable_debug = false;
        auto retval = vm.run();
        if (retval)
        {
            std::cout << "ret: " << retval.value() << " = " << std::hex << retval.value() << '\n';
        }
        else
        {
            std::cout << "error\n";
        }
    }
    return 0;
}