#include <iostream>

#include <boost/program_options.hpp>

#include "complier.hpp"
#include "settings.h"
#include "vm.h"

namespace po = boost::program_options;
using namespace typed_options;
int main(int argc, const char* argv[])
{
    auto opcli = settings::init_settings(argc, argv);
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
        std::vector<std::string> mainargs = tvm.get(main_args).or_(std::vector<std::string>{});
        if (tvm.get(app_options::enable_debug))
        {
            // 禁用 stdout 缓冲，确保崩溃时输出不丢失
            std::cout << std::unitbuf;
            std::setvbuf(stdout, nullptr, _IONBF, 0);
        }
        auto ret = complier::process(tvm.get_direct(input_files));

        if (tvm.get(print_asm))
        {
            for (int i = 0; i < ret.size(); i++)
            {
                std::cout << "[" << i << "]:" << ret[i] << '\n';
            }
        }
        VM vm{ret};
        vm.enable_debug = tvm.get(app_options::enable_debug);
        auto retval = vm.run();
        if (retval)
        {
            if (tvm.get(print_ret_value))
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