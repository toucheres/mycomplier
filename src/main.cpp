#include "complier.hpp"
#include "vm.h"
#include <iostream>
#include <peglib.h>
// int main()
// {
//     Complier com;
//     auto ret = com.process({"/home/toucher/vscoderope/mycomplier/test/test1.c"});
//     if (!ret)
//     {
//         std::cerr << "编译失败" << std::endl;
//         return -1;
//     }
//     // 使用新的vector接口
//     auto assembly_vector = ret.value().get_assembly_vector();
//     // 输出汇编代码
//     std::cout << "Generated Assembly:" << std::endl;
//     for (int i = 0; i < assembly_vector.size(); ++i)
//     {
//         std::cout << "[" << i << "] " << assembly_vector[i] << std::endl;
//     }
//     std::cout << std::endl;
//     VM vm;
//     // 启用调试功能
//     vm.enable_debug("vm_execution.log");
//     vm.load_assembly_vector(assembly_vector);
//     std::cout << "ret: " << vm.start() << '\n';
//     // 关闭调试功能
//     vm.disable_debug();
//     std::cout << "Debug log saved to: vm_execution.log" << std::endl;
//     return 0;
// }
// 修改当前代码

// 递归遍历 AST 的辅助函数
void visit_ast(const std::shared_ptr<peg::Ast>& ast, int depth = 0)
{
    // 打印当前节点
    std::string indent(depth * 2, ' ');
    std::cout << indent << "- " << ast->name;

    // 如果是 token（有具体值）
    if (ast->is_token)
    {
        std::cout << " [" << ast->token << "]";
        std::cout << " (位置: " << ast->line << ":" << ast->column << ")";
    }
    std::cout << "\n";

    // 递归访问所有子节点
    for (const auto& child : ast->nodes)
    {
        visit_ast(child, depth + 1);
    }
}
std::shared_ptr<peg::Ast> simplify_ast(std::shared_ptr<peg::Ast> ast)
{
    if (!ast)
        return ast;

    // 先递归处理所有子节点
    for (size_t i = 0; i < ast->nodes.size(); i++)
    {
        ast->nodes[i] = simplify_ast(ast->nodes[i]);
    }

    // 表达式路径压缩优化
    if (ast->name == "Assignment" || ast->name == "Conditional" ||
        ast->name == "LogicalOr" || ast->name == "LogicalAnd" || ast->name == "BitwiseOr" ||
        ast->name == "BitwiseXor" || ast->name == "BitwiseAnd" || ast->name == "Equality" ||
        ast->name == "Relational" || ast->name == "Shift" || ast->name == "Additive" ||
        ast->name == "Postfix" || ast->name == "Multiplicative" || ast->name == "Cast" ||
        ast->name == "Unary")
    {

        // 如果只有一个子节点，直接返回该子节点
        if (ast->nodes.size() == 1)
        {
            return ast->nodes[0];
        }
    }

    return ast;
}
int main()
{
    using namespace peg;
    std::string peg_rules;
    std::ifstream in("/home/toucher/vscoderope/mycomplier/src/myc_rule.peg");
    std::string test_src;
    std::ifstream test_src_in("/home/toucher/vscoderope/mycomplier/test/test1.c");

    if (!in)
    {
        std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/src/myc_rule.peg"
                  << std::endl;
        return 1;
    }
    if (!test_src_in)
    {
        std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/test/test1.c"
                  << std::endl;
        return 1;
    }
    peg_rules.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    test_src.assign((std::istreambuf_iterator<char>(test_src_in)),
                    std::istreambuf_iterator<char>());
    parser parser{peg_rules};
    parser.set_logger([](size_t line, size_t col, const std::string& msg)
                      { std::cerr << line << ":" << col << ": " << msg << std::endl; });
    // 确保解析器有效
    assert(static_cast<bool>(parser) == true);
    // 启用 AST 和 packrat 优化
    parser.enable_ast();
    parser.enable_packrat_parsing();
    // 定义一个接收 AST 的共享指针
    std::shared_ptr<Ast> ast;

    // 解析输入并生成 AST
    if (parser.parse(test_src, ast))
    {
        std::cout << "解析成功！\n";

        // 1. 输出完整 AST（内置功能）
        std::cout << "\n===== AST 树形输出 =====\n";
        std::cout << ast_to_s(ast) << "\n";
        simplify_ast(ast);
        // 2. 自定义递归遍历 AST
        std::cout << "\n===== 自定义 AST 遍历 =====\n";
        visit_ast(ast);

        // 3. 直接访问根节点信息
        std::cout << "\n===== 根节点信息 =====\n";
        std::cout << "规则名: " << ast->name << "\n";
        if (!ast->nodes.empty())
        {
            std::cout << "子节点数: " << ast->nodes.size() << "\n";
        }
    }
    else
    {
        std::cout << "解析失败！\n";
    }

    return 0;
}
