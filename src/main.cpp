#include <iostream>

#include <boost/program_options.hpp>

#include "complier.hpp"
#include "settings.h"
#include "vm.h"

namespace po = boost::program_options;
using namespace typed_options;
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
        // 创建类型安全的包装器
        TypedVariablesMap tvm(cli);
        using namespace app_options;

        // 使用链式调用 get().or_try().or_()，编译期类型检查
        bool showASt = tvm.get(show_ast).or_(false);
        int tolerate = tvm.get(app_options::tolerate).or_(4);
        bool showFoldedNames = tvm.get(show_folded_names).or_(false);
        bool enable_debug = tvm.get(app_options::enable_debug).or_(false);
        bool printasm = tvm.get(print_asm).or_(false);
        bool printretvalue = tvm.get(print_ret_value).or_(false);

        auto ret =
            complier::process(tvm.get_direct(input_files), showASt, tolerate, showFoldedNames);
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
                std::cout << "ret: " << retval.value() << " = " << "0x" << std::hex
                          << retval.value() << '\n';
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