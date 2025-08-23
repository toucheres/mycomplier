#include "complier.hpp"
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
    if (ast->name == "Assignment" || ast->name == "Conditional" || ast->name == "LogicalOr" ||
        ast->name == "LogicalAnd" || ast->name == "BitwiseOr" || ast->name == "BitwiseXor" ||
        ast->name == "BitwiseAnd" || ast->name == "Equality" || ast->name == "Relational" ||
        ast->name == "Shift" || ast->name == "Additive" || ast->name == "Postfix" ||
        ast->name == "Multiplicative" || ast->name == "Cast" || ast->name == "Unary")
    {

        // 如果只有一个子节点，直接返回该子节点
        if (ast->nodes.size() == 1)
        {
            return ast->nodes[0];
        }
    }

    return ast;
}
std::expected<bool, error> OBJ::generate_code()
{
    visit_ast(this->program);
    return generate_code(this->program, "__global_init", 0);
}
std::expected<bool, error> OBJ::generate_code(std::shared_ptr<peg::Ast> astnode,
                                              std::string funname, size_t deep)
{
    if (!astnode)
    {
        return std::unexpected(error::empty_node);
    }
    auto& node = *astnode;
    if (node.name == "Program")
    {
        for (auto& each : node.nodes)
        {
            if (auto ret = generate_code(each, funname, deep + 1); !ret)
            {
                return ret;
            }
        }
        return true;
    }
    else if (node.name == "FuncDef")
    {
        funcDef func;
        func.is_defined = true;
        func.type = Type{node.nodes[0]};
        func.name = node.nodes[1]->token_to_string();
        if (node.nodes.size() == 4)
        {
            // 有参数
            for (auto eachargptr : node.nodes[2]->nodes)
            {
                func.args.push_back(argDef{eachargptr});
            }
        }
        funname_codes.emplace(func.name, func);
        return generate_code(node.nodes[2], func.name, 0);
    }
    else if (node.name == "Block")
    {
        symbol_table.enter_scope();
        for (auto eachStatement : node.nodes)
        {
            if (auto ret = generate_code(eachStatement, funname, deep + 1); !ret)
            {
                return ret;
            }
        }
        symbol_table.exit_scope();
    }
    else if (node.name == "Statement")
    {
        if (auto ret = generate_code(astnode->nodes[0], funname, 0); !ret)
        {
            return ret;
        }
    }
    else if (node.name == "ReturnStmt")
    {
        if (!node.nodes.size())
        {
            // 返回void, 返回0
            funname_codes[funname].asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            funname_codes[funname].asms.push_back(ASM{ASM::basic_asm::RET});
        }

        if (auto ret = generate_code(astnode->nodes[0], funname, 0); !ret)
        {
            return ret;
        }
        funname_codes[funname].asms.push_back(ASM{ASM::basic_asm::RET});
    }
    else if (node.name == "IfStmt")
    {
        
    }
    else if (node.name == "IfStmt")
    {
    }
    else if (node.name == "IfStmt")
    {
    }
    else if (node.name == "IfStmt")
    {
    }
    else if (node.name == "IfStmt")
    {
    }
    else if (node.name == "IfStmt")
    {
    }
    // std::cout << astnode->name << '\n';
    return true;
}

std::expected<std::vector<std::string>, error> complier::process(std::vector<std::string> paths)
{
    using namespace peg;
    std::string peg_rules;
    std::ifstream in("/home/toucher/vscoderope/mycomplier/src/myc_rule.peg");
    if (!in)
    {
        std::cerr << "无法打开规则文件: /home/toucher/vscoderope/mycomplier/src/myc_rule.peg"
                  << std::endl;
        return std::unexpected(error::file_not_exsist);
    }
    peg_rules.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    parser parser{peg_rules};
    parser.set_logger([](size_t line, size_t col, const std::string& msg)
                      { std::cerr << line << ":" << col << ": " << msg << std::endl; });
    // 确保解析器有效
    assert(static_cast<bool>(parser) == true);
    std::vector<OBJ> objs;
    parser.enable_ast();
    parser.enable_packrat_parsing();
    for (auto& each : paths)
    {
        std::string test_src;
        std::ifstream test_src_in(each);
        if (!test_src_in)
        {
            std::cerr << "无法打开源文件: " << each << std::endl;
            return std::unexpected(error::file_not_exsist);
        }
        test_src.assign((std::istreambuf_iterator<char>(test_src_in)),
                        std::istreambuf_iterator<char>());
        // 定义一个接收 AST 的共享指针
        std::shared_ptr<Ast> ast;
        // 解析输入并生成 AST
        if (parser.parse(test_src, ast))
        {
            OBJ obj{ast};
            if (auto ret = obj.generate_code())
            {
                objs.push_back(obj);
            }
            else
            {
                return std::unexpected{ret.error()};
            }
        }
    }
    return linker::process(objs);
}

std::expected<std::vector<std::string>, error> linker::process(std::vector<OBJ>& objs)
{
    return std::expected<std::vector<std::string>, error>();
}

Type::Type(std::shared_ptr<peg::Ast> astnode)
{
    if (!astnode)
    {
        throw;
    }
    auto node = *astnode;
    std::string basictypename = node.nodes[0]->token_to_string();
    if (basictypename == "int")
    {
        basic_type = BasicType::Int;
    }
    else if (basictypename == "char")
    {
        basic_type = BasicType::Char;
    }
    else if (basictypename == "void")
    {
        basic_type = BasicType::Void;
    }
    if (node.nodes.size() == 2)
    {
        // 有ptr
        auto& ptrs = *node.nodes[1];
        std::string ptr_str = ptrs.token_to_string();
        this->pointer_level = std::count(ptr_str.begin(), ptr_str.end(), '*');
    }
}

argDef::argDef(std::shared_ptr<peg::Ast> astnode)
{
    if (!astnode)
    {
        throw;
    }
    auto node = *astnode;
    type = Type{node.nodes[0]};
    name = node.nodes[1]->token_to_string();
}
