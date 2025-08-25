#include "complier.hpp"
#include "vm.h"
#include <iostream>
#include <peglib.h>

int main()
{
    // using namespace peg;
    // std::string peg_rules;
    // std::ifstream in("/home/toucher/vscoderope/mycomplier/src/myc_rule.peg");
    // std::string test_src;
    // std::ifstream test_src_in("/home/toucher/vscoderope/mycomplier/test/test1.c");

    // if (!in)
    // {
    //     std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/src/myc_rule.peg"
    //               << std::endl;
    //     return 1;
    // }
    // if (!test_src_in)
    // {
    //     std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/test/test1.c"
    //               << std::endl;
    //     return 1;
    // }
    // peg_rules.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // test_src.assign((std::istreambuf_iterator<char>(test_src_in)),
    //                 std::istreambuf_iterator<char>());
    // parser parser{peg_rules};
    // parser.set_logger([](size_t line, size_t col, const std::string& msg)
    //                   { std::cerr << line << ":" << col << ": " << msg << std::endl; });
    // // 确保解析器有效
    // assert(static_cast<bool>(parser) == true);
    // // 启用 AST 和 packrat 优化
    // parser.enable_ast();
    // parser.enable_packrat_parsing();
    // // 定义一个接收 AST 的共享指针
    // std::shared_ptr<Ast> ast;

    // // 解析输入并生成 AST
    // if (parser.parse(test_src, ast))
    // {
    //     std::cout << "解析成功！\n";

    //     // 1. 输出完整 AST（内置功能）
    //     std::cout << "\n===== AST 树形输出 =====\n";
    //     std::cout << ast_to_s(ast) << "\n";
    //     simplify_ast(ast);
    //     // 2. 自定义递归遍历 AST
    //     std::cout << "\n===== 自定义 AST 遍历 =====\n";
    //     visit_ast(ast);

    //     // 3. 直接访问根节点信息
    //     std::cout << "\n===== 根节点信息 =====\n";
    //     std::cout << "规则名: " << ast->name << "\n";
    //     if (!ast->nodes.empty())
    //     {
    //         std::cout << "子节点数: " << ast->nodes.size() << "\n";
    //     }
    // }
    // else
    // {
    //     std::cout << "解析失败！\n";
    // }
    auto ret = complier::process({"/home/toucher/vscoderope/mycomplier/test/test1.c"}).value();
    for (auto& each : ret)
    {
        std::cout << each << '\n';
    }
    return 0;
}
