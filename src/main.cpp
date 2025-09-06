#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include <antlr4-runtime/antlr4-runtime.h>
#include <iostream>

// 创建自定义访问器类，继承BaseVisitor
class MyComplierVisitor : public ComplierBaseVisitor
{
};
void printAST(antlr4::tree::ParseTree* tree, int depth = 0)
{
    std::string indent(depth * 2, ' ');

    if (tree->children.empty())
    {
        // 叶子节点
        std::cout << indent << "Leaf: " << tree->getText() << std::endl;
        return;
    }

    // 非叶子节点
    std::cout << indent << "Node: " << tree->getText() << std::endl;

    // 递归打印子节点
    for (auto child : tree->children)
    {
        printAST(child, depth + 1);
    }
}

int main(int argc, const char* argv[])
{
    // 创建输入流
    std::ifstream in("/home/toucher/vscoderope/mycomplier/test/test1.c.pre");
    std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    antlr4::ANTLRInputStream inputStream(input);
    // antlr4::ANTLRInputStream inputStream(input);

    // 创建词法分析器
    ComplierLexer lexer(&inputStream);
    antlr4::CommonTokenStream tokens(&lexer);

    // 创建语法分析器
    ComplierParser parser(&tokens);

    // 使用正确的入口规则 - 根据你的语法确定
    // 可能是 translationUnit、compilationUnit 或 expression
    auto tree = parser.compilationUnit(); // 替换成你语法的入口规则
    std::cout << "ast: \n";
    // 在 main 函数中调用：
    printAST(tree);
    // 创建和使用自定义访问器
    MyComplierVisitor visitor;
    auto result = visitor.visit(tree);
    return 0;
}