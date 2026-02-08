/**
 * c_grammar_cpp_test — 独立测试程序
 *
 * 用法:
 *   c_grammar_cpp_test [options] <file.c>
 *
 * Options:
 *   --nopp     跳过预处理 (直接解析)
 *   --gcc      使用 gcc -E 预处理
 *   --clang    使用 clang -E 预处理
 *   --tree     打印 parse tree
 */

#include "antlr4-runtime.h"
#include "CLexer.h"
#include "CParser.h"
#include "CLexerBase.h"
#include "CParserBase.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void usage(const char *prog) {
    std::cerr << "Usage: " << prog
              << " [--nopp|--gcc|--clang] [--tree] <file.c>\n";
}

int main(int argc, char *argv[]) {
    std::vector<std::string> args;
    std::string inputFile;
    bool showTree = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tree") {
            showTree = true;
        } else if (a.starts_with("--")) {
            args.push_back(a);
        } else {
            inputFile = a;
        }
    }

    if (inputFile.empty()) {
        usage(argv[0]);
        return 1;
    }

    // 读取源文件
    std::ifstream ifs(inputFile);
    if (!ifs) {
        std::cerr << "Cannot open file: " << inputFile << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    // 配置 CLexerBase 参数 (预处理模式)
    CLexerBase::setArgs(args);

    // Lexer
    antlr4::ANTLRInputStream input(source);
    input.name = inputFile;
    CLexer lexer(&input);

    // 收集 lexer 错误
    antlr4::CommonTokenStream tokens(&lexer);

    // Parser
    CParser parser(&tokens);

    // 解析
    auto *tree = parser.compilationUnit();

    // 结果
    size_t syntaxErrors = parser.getNumberOfSyntaxErrors();

    if (showTree) {
        std::cout << tree->toStringTree(&parser, true) << "\n";
    }

    if (syntaxErrors > 0) {
        std::cerr << "[FAIL] " << inputFile << ": "
                  << syntaxErrors << " syntax error(s)\n";
        return 1;
    }

    std::cout << "[OK] " << inputFile << ": parsed successfully\n";
    return 0;
}
