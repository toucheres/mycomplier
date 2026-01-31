#include "astVisit.h"
#include "ASM.hpp"
#include "ComplierBaseVisitor.h"
#include "ComplierLexer.h"
#include "ComplierParser.h"
#include "error.hpp"
#include "tools.hpp"
#include "type_utils.hpp"
#include "vm.h"
#include <functional>
#include <tree/TerminalNode.h>
#include <utility>
template <class T, class IN>
    requires requires(IN in) { dynamic_cast<T>(in); }
auto dc(IN&& in)
{
    return dynamic_cast<T>(in);
}
void astVisitor::visitCompilationUnit(ComplierParser::CompilationUnitContext* ctx)
{
    // ctx->getRuleIndex() == ComplierParser::RuleCompilationUnit;
    if (!ctx)
        return;
    auto tu = ctx->translationUnit();
    if (!tu)
        return;
    return visitTranslationUnit(tu);
}
void astVisitor::visitTranslationUnit(ComplierParser::TranslationUnitContext* ctx)
{
    if (!ctx)
        return;
    for (auto each : ctx->externalDeclaration())
    {
        if (!each)
            continue;
        visitExternalDeclaration(each);
    }
}

IDdef* astVisitor::record_ID_decl(const std::string& name, const Type& type,
                                  StorageClassSpecifier storageClassSpecifier, IDdef* func_ctx,
                                  std::optional<std::size_t> arg_index)
{
    IDdef def;
    def.name = name;
    def.type = type;
    def.is_defined = true;
    def.storageClassSpecifier = storageClassSpecifier;
    def.kind = func_ctx ? (arg_index ? IDdef::Kind::Arg : IDdef::Kind::Local) : IDdef::Kind::Global;

    if (storageClassSpecifier == StorageClassSpecifier::Static && def.link_label.empty())
    {
        def.link_label = def.name + "#" + std::to_string(obj.decls.static_label_counter++);
    }

    if (def.kind == IDdef::Kind::Arg && arg_index)
    {
        // 参数按实际字节大小向上对齐到 word 计算偏移：
        // [bp+0]=old bp, [bp+size_word]=ret addr, 之后依次为各实参
        const std::size_t base_arg_offset = 2 * VCPU<>::size_word;
        std::size_t offset = base_arg_offset;
        for (std::size_t i = 0; i < *arg_index; ++i)
        {
            offset += align_up(func_ctx->type.args[i].type.getsize(), VCPU<>::size_word);
        }
        def.addr = static_cast<int>(offset);
    }
    else if (def.kind == IDdef::Kind::Local &&
             storageClassSpecifier != StorageClassSpecifier::Extern &&
             storageClassSpecifier != StorageClassSpecifier::Static)
    {
        const auto new_pos =
            def.get_addr_in_stack(func_ctx ? func_ctx->funcInfo.stack_size_now : 0);
        def.addr = -static_cast<int>(new_pos);
        if (func_ctx)
        {
            func_ctx->funcInfo.stack_size_now = static_cast<std::size_t>(-def.addr);
            func_ctx->funcInfo.max_stack_size =
                std::max(func_ctx->funcInfo.max_stack_size, func_ctx->funcInfo.stack_size_now);
        }
    }

    obj.decls.add_ID_decl(def, storageClassSpecifier);
    return obj.decls.find_ID_decl(def.name, storageClassSpecifier);
}

IDdef* astVisitor::record_ID_decl(const IDdef& iddef, IDdef* func_ctx,
                                  std::optional<std::size_t> arg_index)
{
    return record_ID_decl(iddef.name, iddef.type, iddef.storageClassSpecifier, func_ctx, arg_index);
}

IDdef* astVisitor::lookup_ID_decl(const std::string& name,
                                  StorageClassSpecifier storageClassSpecifier)
{
    if (auto* p = obj.decls.find_ID_decl(name, storageClassSpecifier))
    {
        return p;
    }
    if (storageClassSpecifier == StorageClassSpecifier::VarDef)
    {
        if (auto* p = obj.decls.find_ID_decl(name, StorageClassSpecifier::Static))
        {
            return p;
        }
        if (auto* p = obj.decls.find_ID_decl(name, StorageClassSpecifier::Extern))
        {
            return p;
        }
    }
    return nullptr;
}

std::string astVisitor::global_label(const IDdef& v) const
{
    const bool is_static = v.storageClassSpecifier == StorageClassSpecifier::Static;
    const std::string& label = v.link_label.empty() ? v.name : v.link_label;
    if (is_static)
    {
        return "globalvar@" + obj.name + "@" + label;
    }
    return "globalvar@" + label;
}
// 用于有初始化的，会自动 add_var_def 并添加汇编
std::vector<IDdef> astVisitor::lowerDeclaration(ComplierParser::DeclarationSpecifiersContext* specs,
                                                ComplierParser::InitDeclaratorListContext* initList)
{
    Type basetype;
    StorageClassSpecifier storageClassType = StorageClassSpecifier::VarDef;
    std::vector<IDdef> vars;
    if (specs) // 前类型
    {
        auto [type, storageClassSpecifier] = visitDeclarationSpecifiers(specs);
        if (!type)
        {
            THROW_ERR(error::expected_type, specs);
        }
        else
        {
            basetype = *type;
        }
        storageClassType =
            storageClassSpecifier ? *storageClassSpecifier : StorageClassSpecifier::VarDef;
    }
    if (initList) // 声明符
    {
        vars = visitInitDeclaratorList(initList, basetype, storageClassType);
    }
    return vars;
}

std::tuple<IDdef*, std::string> astVisitor::madeConstString(
    std::vector<antlr4::tree::TerminalNode*> toks)
{
    std::string chars_after_transed;

    auto hexval = [](char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return 0;
    };

    for (auto tok : toks)
    {
        std::string s = tok->getText();
        // skip prefix like u8, U, L, u
        size_t p = 0;
        while (p < s.size() && (std::isalpha((unsigned char)s[p]) || s[p] == '8'))
            p++;

        // raw string literal: R"delim(... )delim"
        if (p + 1 < s.size() && s[p] == 'R' && s[p + 1] == '"')
        {
            size_t q = p + 2; // start of delimiter
            size_t delim_end = s.find('(', q);
            if (delim_end != std::string::npos)
            {
                std::string delim = s.substr(q, delim_end - q);
                size_t content_start = delim_end + 1;
                std::string close = std::string(")") + delim + '"';
                size_t close_pos = s.find(close, content_start);
                if (close_pos != std::string::npos)
                {
                    chars_after_transed.append(s, content_start, close_pos - content_start);
                }
            }
            continue;
        }

        // normal string literal: "..."
        if (p < s.size() && s[p] == '"')
        {
            size_t k = p + 1;
            while (k < s.size())
            {
                char c = s[k];
                if (c == '"')
                    break;
                if (c == '\\' && k + 1 < s.size())
                {
                    char esc = s[k + 1];
                    switch (esc)
                    {
                    case 'n':
                        chars_after_transed.push_back('\n');
                        k += 2;
                        break;
                    case 't':
                        chars_after_transed.push_back('\t');
                        k += 2;
                        break;
                    case 'r':
                        chars_after_transed.push_back('\r');
                        k += 2;
                        break;
                    case '\\':
                        chars_after_transed.push_back('\\');
                        k += 2;
                        break;
                    case '\'':
                        chars_after_transed.push_back('\'');
                        k += 2;
                        break;
                    case '"':
                        chars_after_transed.push_back('"');
                        k += 2;
                        break;
                    case 'a':
                        chars_after_transed.push_back('\a');
                        k += 2;
                        break;
                    case 'b':
                        chars_after_transed.push_back('\b');
                        k += 2;
                        break;
                    case 'f':
                        chars_after_transed.push_back('\f');
                        k += 2;
                        break;
                    case 'v':
                        chars_after_transed.push_back('\v');
                        k += 2;
                        break;
                    case '?':
                        chars_after_transed.push_back('?');
                        k += 2;
                        break;
                    case 'x':
                    {
                        // hex escape: \xhh...
                        k += 2;
                        int val = 0;
                        bool any = false;
                        while (k < s.size() && std::isxdigit((unsigned char)s[k]))
                        {
                            val = val * 16 + hexval(s[k]);
                            k++;
                            any = true;
                        }
                        if (any)
                            chars_after_transed.push_back(static_cast<char>(val));
                        break;
                    }
                    default:
                        if (esc >= '0' && esc <= '7')
                        {
                            // octal escape: up to 3 digits
                            int val = esc - '0';
                            k += 2;
                            int cnt = 1;
                            while (cnt < 3 && k < s.size() && s[k] >= '0' && s[k] <= '7')
                            {
                                val = val * 8 + (s[k] - '0');
                                k++;
                                cnt++;
                            }
                            chars_after_transed.push_back(static_cast<char>(val));
                        }
                        else
                        {
                            // unknown escape, keep raw following char
                            chars_after_transed.push_back(esc);
                            k += 2;
                        }
                    }
                }
                else
                {
                    chars_after_transed.push_back(c);
                    k++;
                }
            }
        }
    }

    // ensure terminating NUL for C string
    chars_after_transed.push_back('\0');
    // std::cout << "--------\n";
    // std::cout << chars << '\n';
    // std::cout << "--------\n";
    Type global_chars_arr_type;
    global_chars_arr_type.kind = Type::Kind::Array;
    global_chars_arr_type.arr_num = chars_after_transed.size();
    global_chars_arr_type.pushTop(Type{Type::Kind::Basic, Type::BasicType::Char});
    IDdef arrdef;
    arrdef.kind = IDdef::Kind::Global;
    arrdef.type = global_chars_arr_type;
    arrdef.is_defined = true;
    arrdef.storageClassSpecifier = StorageClassSpecifier::VarDef;
    static size_t index = 1;
    // 切勿将包含终止 NUL 或不可见字符的字符串内容直接拼入标识符，
    // 这会导致在符号表查找时匹配失败（字符串内的 '\0' 会使实际键与文本显示不一致）。
    // 使用索引与内容哈希来保证唯一性且只包含可打印字符。
    arrdef.name = "stringliteral_" + std::to_string(index++) + "_" +
                  std::to_string(std::hash<std::string>{}(chars_after_transed));
    auto* recorded =
        record_ID_decl(arrdef.name, arrdef.type, arrdef.storageClassSpecifier, nullptr);
    return {recorded, chars_after_transed};
}

std::optional<std::vector<int>> astVisitor::decodeStringLiteral(
    ComplierParser::AssignmentExpressionContext* expr)
{
    if (!expr)
    {
        return std::nullopt;
    }
    std::string text;
    try
    {
        text = expr->getText();
    }
    catch (...)
    {
        return std::nullopt;
    }
    if (text.empty())
    {
        return std::nullopt;
    }
    auto hexValue = [](char ch) -> int
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        return -1;
    };
    size_t pos = 0;
    std::vector<int> result;
    while (pos < text.size())
    {
        if (text.compare(pos, 2, "u8") == 0 && pos + 2 < text.size() && text[pos + 2] == '"')
        {
            pos += 2;
        }
        else if ((text[pos] == 'u' || text[pos] == 'U' || text[pos] == 'L') &&
                 pos + 1 < text.size() && text[pos + 1] == '"')
        {
            pos += 1;
        }
        if (pos >= text.size() || text[pos] != '"')
        {
            return std::nullopt;
        }
        pos++;
        while (pos < text.size() && text[pos] != '"')
        {
            if (text[pos] == '\\')
            {
                pos++;
                if (pos >= text.size())
                {
                    return std::nullopt;
                }
                char esc = text[pos];
                switch (esc)
                {
                case 'n':
                    result.push_back('\n');
                    pos++;
                    break;
                case 't':
                    result.push_back('\t');
                    pos++;
                    break;
                case 'r':
                    result.push_back('\r');
                    pos++;
                    break;
                case 'a':
                    result.push_back('\a');
                    pos++;
                    break;
                case 'b':
                    result.push_back('\b');
                    pos++;
                    break;
                case 'f':
                    result.push_back('\f');
                    pos++;
                    break;
                case 'v':
                    result.push_back('\v');
                    pos++;
                    break;
                case '\\':
                    result.push_back('\\');
                    pos++;
                    break;
                case '\'':
                    result.push_back('\'');
                    pos++;
                    break;
                case '"':
                    result.push_back('"');
                    pos++;
                    break;
                case 'x':
                {
                    pos++;
                    int value = 0;
                    bool hasDigit = false;
                    while (pos < text.size())
                    {
                        int hv = hexValue(text[pos]);
                        if (hv < 0)
                        {
                            break;
                        }
                        hasDigit = true;
                        value = (value << 4) + hv;
                        pos++;
                    }
                    if (!hasDigit)
                    {
                        return std::nullopt;
                    }
                    result.push_back(value & 0xFF);
                    break;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                {
                    int value = esc - '0';
                    pos++;
                    int count = 1;
                    while (count < 3 && pos < text.size() && text[pos] >= '0' && text[pos] <= '7')
                    {
                        value = value * 8 + (text[pos] - '0');
                        pos++;
                        count++;
                    }
                    result.push_back(value & 0xFF);
                    break;
                }
                default:
                    result.push_back(static_cast<unsigned char>(esc));
                    pos++;
                    break;
                }
            }
            else
            {
                result.push_back(static_cast<unsigned char>(text[pos]));
                pos++;
            }
        }
        if (pos >= text.size())
        {
            return std::nullopt;
        }
        pos++;
    }
    result.push_back(0);
    return result;
}

std::optional<Type> astVisitor::tryVisitType(std::function<Type()> expr)
{
    auto start = funcnow->funcInfo.asms.size();
    //[TODO] 目前回退了funasms, 应回退obj状态
    // auto objcopy = obj;
    std::optional<Type> rettype;
    try
    {
        rettype = expr();
    }
    catch (...)
    {
        funcnow->funcInfo.asms.resize(start);
    }
    funcnow->funcInfo.asms.resize(start);
    return rettype;
}

std::vector<IDdef> astVisitor::visitDeclaration(ComplierParser::DeclarationContext* ctx)
{
    return lowerDeclaration(ctx->declarationSpecifiers(), ctx->initDeclaratorList());
}
void astVisitor::visitFunctionDefinition(ComplierParser::FunctionDefinitionContext* ctx)
{
    IDdef functionType = visitDeclarator(ctx->declarator());
    StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::VarDef;
    if (ctx->declarationSpecifiers()) // 返回类型
    {
        auto [rettype, storageClass] = visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        if (!rettype)
            THROW_ERR(error::expected_type, ctx);
        storageClassSpecifier = storageClass ? *storageClass : storageClassSpecifier;
        functionType.type.pushTop(*rettype);
    }
    else
    {
        THROW_ERR(error::expected_type, ctx);
    }
    // IDdef def;
    // def.name = functionType.id;
    // def.type = functionType;
    // def.is_defined = true;
    // def.storageClassSpecifier = storageClassSpecifier;
    // def.kind = IDdef::Kind::Global;
    functionType.is_defined = true;
    functionType.storageClassSpecifier = storageClassSpecifier;
    functionType.kind = IDdef::Kind::Global;

    // 检查返回类型是否为结构体，如果是则添加隐式返回指针参数
    const Type rettype = (functionType.type.subType) ? *functionType.type.subType : Type{};
    const bool is_struct_return = (rettype.kind == Type::Kind::Struct);

    if (is_struct_return)
    {
        // 在参数列表最前面添加隐式返回指针参数
        IDdef ret_ptr_arg;
        ret_ptr_arg.name = "__struct_ret_ptr";
        ret_ptr_arg.type = Type{Type::Kind::Pointer, 1};
        ret_ptr_arg.type.subType = value_ptr<Type>::make_value_ptr(rettype);
        ret_ptr_arg.storageClassSpecifier = StorageClassSpecifier::VarDef;
        functionType.type.args.insert(functionType.type.args.begin(), ret_ptr_arg);
    }

    auto gfunptr_local = funcnow;
    auto* funnowptr = record_ID_decl(functionType);
    funcnow = funnowptr;

    obj.enter_decl_scope(functionType.name);
    std::size_t arg_index = 0;
    for (const auto& arg : funcnow->type.args)
    {
        record_ID_decl(arg, funcnow, arg_index);
        ++arg_index;
    }
    funcnow->funcInfo.asms.push_back("HOLD");
    visitCompoundStatement(ctx->compoundStatement());
    const Type funcrettype = (funcnow->type.subType) ? *funcnow->type.subType : Type{};
    if (funcrettype.kind == Type::Kind::Basic && funcrettype.basic_type == Type::BasicType::Void)
    {
        // 为void func添加自动return
        if (funcnow->funcInfo.asms.back() != (std::string)ASM{ASM::basic_asm::RET})
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RET});
        }
    }
    funcnow->funcInfo.asms[0] =
        ASM{ASM::basic_asm::NVAR,
            align_up(funcnow->funcInfo.max_stack_size, VCPU<>::size_word) / VCPU<>::size_word};
    obj.exit_decl_scope();
    funcnow = gfunptr_local;
}
void astVisitor::visitExternalDeclaration(ComplierParser::ExternalDeclarationContext* ctx)
{
    // 检查是否为函数定义
    if (ctx->functionDefinition())
    {
        // 在visitFunctionDefinition内部处理
        visitFunctionDefinition(ctx->functionDefinition());
        return;
    }

    // 检查是否为变量定义
    else if (ctx->declaration())
    {
        // addDeclarations(visitDeclaration(ctx->declaration()));
        visitDeclaration(ctx->declaration());
    }
    else if (ctx->asmADDer()) // 拓展
    {
        visitAsmADDer(ctx->asmADDer());
    }

    // 检查是否为单独的分号（空语句）
    else if (ctx->children.size() == 1 && ctx->children[0]->getText() == ";")
    {
        // 空语句不需要特别处理
        return;
    }
    return;
}
std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>> astVisitor::
    visitDeclarationSpecifiers(ComplierParser::DeclarationSpecifiersContext* ctx)
{
    std::optional<Type> type;
    std::optional<StorageClassSpecifier> storageC;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        // [TODO] typeQualifier functionSpecifier alignmentSpecifier
        Type result = visitDeclarationSpecifier(each);
        if (result.storageClassSpecifier != StorageClassSpecifier::VarDef)
        {
            if (storageC)
            {
                THROW_ERR(error::double_StorageClassSpecifier, ctx);
            }
            storageC = result.storageClassSpecifier;
        }
        else
        {
            if (type)
            {
                THROW_ERR(error::double_type, ctx);
            }
            type = result;
        }
    }
    return {type, storageC};
}
Type astVisitor::visitDeclarationSpecifier(ComplierParser::DeclarationSpecifierContext* ctx)
{
    // 检查是否为类型说明符
    if (ctx->typeSpecifier())
    {
        // 直接返回typeSpecifier的结果
        return visitTypeSpecifier(ctx->typeSpecifier());
    }
    else if (ctx->storageClassSpecifier()) // typedef / extern
    {
        Type tp{};
        tp.kind = Type::Kind::Undefined;
        if (ctx->getText() == "typedef")
        {
            tp.storageClassSpecifier = StorageClassSpecifier::Typedef;
        }
        else if (ctx->getText() == "static")
        {
            tp.storageClassSpecifier = StorageClassSpecifier::Static;
        }
        else if (ctx->getText() == "extern")
        {
            tp.storageClassSpecifier = StorageClassSpecifier::Extern;
        }
        else
        {
            THROW_ERR(error::unsurpport_StorageClassSpecifier, ctx);
        }
        return tp;
    }
    THROW_ERR(error::unsurpport_DeclarationSpecifier, ctx);
}
Type astVisitor::visitTypeSpecifier(ComplierParser::TypeSpecifierContext* ctx)
{
    Type rettype;
    if (ctx->typedefName()) // 是类型别名
    {
        if (auto* tp =
                lookup_ID_decl(ctx->typedefName()->getText(), StorageClassSpecifier::Typedef))
        {
            rettype = tp->type;
            return rettype;
        }
        THROW_ERR(error::undifined_type, ctx);
    }
    if (ctx->structOrUnionSpecifier()) // 是结构体类型声明 / 结构体变量声明
    {
        if (ctx->structOrUnionSpecifier()->structOrUnion()->getText() == "struct") // struct
        {
            std::string nonameID;
            if (ctx->structOrUnionSpecifier()->structDeclarationList()) // 同时定义struct
            {
                IDdef thisStruct;
                thisStruct.storageClassSpecifier = Type::StorageClassSpecifier::StructDef;
                static long structIndex = 0;
                nonameID = thisStruct.name = thisStruct.type.structID =
                    ctx->structOrUnionSpecifier()->Identifier()
                        ? ctx->structOrUnionSpecifier()->Identifier()->getText()
                        : "__nuname_struct_" + std::to_string(structIndex++);
                thisStruct.type.kind = Type::Kind::Struct;
                thisStruct.type.structInfo.members = visitStructDeclarationList(
                    ctx->structOrUnionSpecifier()->structDeclarationList());
                record_ID_decl(thisStruct);
            }
            auto* st = lookup_ID_decl(ctx->structOrUnionSpecifier()->Identifier()
                                          ? ctx->structOrUnionSpecifier()->Identifier()->getText()
                                          : nonameID,
                                      StorageClassSpecifier::StructDef);
            if (!st)
            {
                THROW_ERR(error::undifined_struct, ctx);
            }
            return st->type;
        }
        else // [TODO] union
        {
            THROW_ERR(error::unsurpport_union, ctx);
        }
    }
    // 是基本类型
    rettype.kind = Type::Kind::Basic;
    if (ctx->getText() == "int")
    {
        rettype.basic_type = Type::BasicType::Int;
        return rettype;
    }
    else if (ctx->getText() == "char")
    {
        rettype.basic_type = Type::BasicType::Char;
        return rettype;
    }
    else if (ctx->getText() == "void")
    {
        rettype.basic_type = Type::BasicType::Void;
        return rettype;
    }
    else if (ctx->getText() == "float")
    {
        rettype.basic_type = Type::BasicType::Float;
        return rettype;
    }
    else if (ctx->getText() == "double")
    {
        rettype.basic_type = Type::BasicType::Double;
        return rettype;
    }
    else if (ctx->getText() == "long")
    {
        rettype.basic_type = Type::BasicType::Long;
        return rettype;
    }
    else if (ctx->getText() == "short")
    {
        rettype.basic_type = Type::BasicType::Short;
        return rettype;
    }
    else if (ctx->getText() == "unsigned")
    {
        rettype.basic_type = Type::BasicType::Unsigned;
        return rettype;
    }
    else if (ctx->getText() == "signed")
    {
        rettype.basic_type = Type::BasicType::Signed;
        return rettype;
    }
    THROW_ERR(error::unsurpport_basictype, ctx);
}

std::vector<IDdef> astVisitor::visitInitDeclaratorList(
    ComplierParser::InitDeclaratorListContext* ctx, Type basetype,
    StorageClassSpecifier storageClassSpecifier)
{
    std::vector<IDdef> vars;
    for (auto& each : ctx->initDeclarator())
    {
        vars.push_back(visitInitDeclarator(each, basetype, storageClassSpecifier));
    }
    return vars;
}

// [IMPORTANT]
// TODO 优化为 load_var
// TODO 将初始化 移入 visitDeclarator
IDdef astVisitor::visitInitDeclarator(ComplierParser::InitDeclaratorContext* ctx, Type basetype,
                                      StorageClassSpecifier storageClassSpecifier)
{
    auto ret = visitDeclarator(ctx->declarator());
    ret.type.pushTop(basetype);
    ret.storageClassSpecifier = storageClassSpecifier;
    ret.valueType = ValueType::Left;
    ret.type.valueType = ValueType::Left;
    IDdef* recorded;
    if (ret.type.kind == Type::Kind::Function)
    {
        ret.storageClassSpecifier = storageClassSpecifier =
            StorageClassSpecifier::Extern; // 函数声明默认extern
        record_ID_decl(ret);
        recorded = lookup_ID_decl(ret.name, storageClassSpecifier);
    }
    else
    {
        IDdef* func_ctx = (funcnow != gfuncptr) ? funcnow : nullptr;
        record_ID_decl(ret, func_ctx);
        recorded = lookup_ID_decl(ret.name, storageClassSpecifier);
    }
    // [TODO] 部分初始化的一般化处理
    std::function<bool(Type, ComplierParser::InitializerContext*)> func =
        [&func, this, ctx](Type arg, ComplierParser::InitializerContext* init) -> bool
    {
        // addr通过运行时栈传递
        auto asmholder = funcnow;
        if (arg.kind == Type::Kind::Basic || arg.kind == Type::Kind::Pointer)
        {
            if (init->assignmentExpression()) // = expr
            {
                visitAssignmentExpression(init->assignmentExpression());
                if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
                {
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SC});
                }
                else if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
                {
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SI});
                }
                else if (arg.getsize() == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
                {
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SW});
                }
            }
        }
        // [TODO] 考虑StrogeClass
        else if (arg.kind == Type::Kind::Struct)
        {
            // struct tests c = {1, 2, 3};
            // struct tests d = c; // 不允许
            if (init->assignmentExpression())
            {
                THROW_ERR(error::expected_initializerList, ctx);
            }
            size_t initListSize = init->initializerList()->initializer().size();
            if (initListSize > arg.structInfo.members.size())
            {
                THROW_ERR(error::initializerList_too_long, ctx);
            }
            size_t limit = std::min(arg.structInfo.members.size(), initListSize);
            for (size_t i = 0; i + 1 < limit; i++)
            {
                asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            }
            for (size_t i = 0; i < limit; i++)
            {
                if (i != 0)
                {
                    asmholder->funcInfo.asms.push_back(
                        ASM{ASM::basic_asm::IMM, arg.structInfo.members[i].second.addr});
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
                }
                if (init->initializerList()->initializer(i))
                {
                    func(arg.structInfo.members[i].second.type,
                         init->initializerList()->initializer(i));
                }
            }
        }
        else if (arg.kind == Type::Kind::Array)
        {
            // arr = {'',''} 格式
            if (init->initializerList())
            {
                size_t initListSize = init->initializerList()->initializer().size();
                if (arg.arr_num > 0 && initListSize > static_cast<size_t>(arg.arr_num))
                {
                    THROW_ERR(error::initializerList_too_long, ctx);
                }
                size_t limit = arg.arr_num > 0
                                   ? std::min(static_cast<size_t>(arg.arr_num), initListSize)
                                   : initListSize;
                for (size_t i = 0; i + 1 < limit; i++)
                {
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
                }
                for (size_t i = 0; i < limit; i++)
                {
                    if (i != 0)
                    {
                        asmholder->funcInfo.asms.push_back(
                            ASM{ASM::basic_asm::IMM, arg.subType->getsize() * i});
                        asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
                    }
                    if (init->initializerList()->initializer(i))
                    {
                        func(*arg.subType, init->initializerList()->initializer(i));
                    }
                }
            }
            // arr = xxx; 一般为 char arr[12] = "xxx";
            else if (auto assign = init->assignmentExpression())
            {
                auto literal = decodeStringLiteral(assign); // 包含\0
                if (!literal)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                if (!arg.subType || arg.subType->kind != Type::Kind::Basic ||
                    arg.subType->basic_type != Type::BasicType::Char)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                if (arg.arr_num < 0) // [TODO] 未指定时由 "xxx" 长度隐式决定
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                auto data = *literal;
                size_t arrLen = static_cast<size_t>(arg.arr_num);
                if (data.size() > arrLen)
                {
                    THROW_ERR(error::expected_arr_initor, assign);
                }
                data.resize(arrLen, 0);
                if (arrLen == 0)
                {
                }
                for (size_t i = 0; i + 1 < arrLen; ++i)
                {
                    asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
                }
                size_t elemSize = arg.subType->getsize();
                Type charType{Type::Kind::Basic, Type::BasicType::Char};
                size_t charSize = charType.getsize();
                size_t intSize = Type{Type::Kind::Basic, Type::BasicType::Int}.getsize();
                size_t longSize = Type{Type::Kind::Basic, Type::BasicType::Long}.getsize();
                for (size_t i = 0; i < arrLen; ++i)
                {
                    if (i != 0)
                    {
                        asmholder->funcInfo.asms.push_back(
                            ASM{ASM::basic_asm::IMM, static_cast<int>(elemSize * i)});
                        asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
                    }
                    asmholder->funcInfo.asms.push_back(
                        ASM{ASM::basic_asm::IMM,
                            static_cast<int>(static_cast<unsigned char>(data[i]))});
                    if (elemSize == charSize)
                    {
                        asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SC});
                    }
                    else if (elemSize == intSize)
                    {
                        asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SI});
                    }
                    else if (elemSize == longSize)
                    {
                        asmholder->funcInfo.asms.push_back(ASM{ASM::basic_asm::SW});
                    }
                    else
                    {
                        THROW_ERR(error::expected_arr_initor, assign);
                    }
                }
            }
            else
            {
                THROW_ERR(error::expected_arr_initor, init);
            }
        }
        return false;
    };
    // 有初始化时:
    if (ctx->initializer())
    {
        load_var_or_func(recorded->name);
        madeTopIsLvalueAddr();
        func(ret.type, ctx->initializer());
    }
    return ret;
}
// 后序递归生成
IDdef astVisitor::visitDeclarator(ComplierParser::DeclaratorContext* ctx)
{
    IDdef var = visitDirectDeclarator(ctx->directDeclarator());
    if (ctx->pointer()) // 有ptr
    {
        auto str = ctx->pointer()->getText();
        int ptr_count = static_cast<int>(std::count(str.begin(), str.end(), '*'));
        // 嵌套多层 Pointer
        for (int i = 0; i < ptr_count; i++)
        {
            Type tp;
            tp.kind = Type::Kind::Pointer;
            var.type.pushTop(tp);
        }
    }
    return var;
}
// [REWRITE] ctx->getAltNumber()不是分支标识
IDdef astVisitor::visitDirectDeclarator(ComplierParser::DirectDeclaratorContext* ctx)
{
    IDdef var;
    if (ctx->Identifier() && ctx->DigitSequence()) // 位域
    {
        return var;
    }
    else if (ctx->Identifier()) // 变量名
    {
        var.name = ctx->Identifier()->getText();
        return var;
    }
    else if (ctx->LeftParen() && ctx->directDeclarator()) // 函数声明
    {
        var = visitDirectDeclarator(ctx->directDeclarator());
        std::vector<IDdef> args;
        if (ctx->parameterTypeList())
        {
            args = visitParameterTypeList(ctx->parameterTypeList());
        }
        var.type.pushTop(Type{Type::Kind::Function, args});
        return var;
    }
    else if (ctx->directDeclarator() && ctx->LeftBracket() &&
             ctx->assignmentExpression()) // base+ 数组
    {
        var = visitDirectDeclarator(ctx->directDeclarator());
        // parseConstexpr 返回 long long, 这里数组维度内部使用 int, 显式窄化避免警告
        var.type.pushTop(
            Type(Type::Kind::Array, static_cast<int>(parseConstexpr(ctx->assignmentExpression()))));
        return var;
    }
    else if (ctx->declarator()) // (dec)
    {
        return visitDeclarator(ctx->declarator());
    }
    THROW_ERR(error::unsurpport_directDeclarator, ctx);
}
std::vector<IDdef> astVisitor::visitParameterTypeList(ComplierParser::ParameterTypeListContext* ctx)
{
    return visitParameterList(ctx->parameterList());
}
void astVisitor::visitCompoundStatement(ComplierParser::CompoundStatementContext* ctx)
{
    if (auto ptr = ctx->blockItemList())
    {
        obj.enter_decl_scope("compound");
        visitBlockItemList(ctx->blockItemList());
        obj.exit_decl_scope();
    }
}
std::vector<IDdef> astVisitor::visitParameterList(ComplierParser::ParameterListContext* ctx)
{
    std::vector<IDdef> vars;
    for (auto& each : ctx->parameterDeclaration())
    {
        vars.push_back(visitParameterDeclaration(each));
    }
    return vars;
}
IDdef astVisitor::visitParameterDeclaration(ComplierParser::ParameterDeclarationContext* ctx)
{
    Type basetype;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto [type, storageClassSpecifier] =
            visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        if (!type)
        {
            THROW_ERR(error::expected_type, ctx->declarationSpecifiers());
        }
        if (storageClassSpecifier)
        {
            THROW_ERR(error ::unexpected_storageClassSpecifier, ctx->declarationSpecifiers());
        }
        basetype = *type;
    }
    else if (ctx->declarationSpecifiers2()) // 前类型
    {
        auto [type, storageClassSpecifier] =
            visitDeclarationSpecifiers2(ctx->declarationSpecifiers2());
        if (!type)
        {
            THROW_ERR(error ::expected_type, ctx->declarationSpecifiers());
        }
        if (storageClassSpecifier)
        {
            THROW_ERR(error ::unexpected_storageClassSpecifier, ctx->declarationSpecifiers());
        }
        basetype = *type;
    }
    static long noname_para_index = 0;
    if (ctx->declarator()) // (有名的)数组/函数/指针的组合s
    {
        IDdef var = visitDeclarator(ctx->declarator());
        var.type.pushTop(basetype);
        return var;
    }
    else if (ctx->abstractDeclarator()) // (无名的)数组/函数/指针的组合s
    {
        IDdef iddef;
        iddef.type = visitAbstractDeclarator(ctx->abstractDeclarator());
        iddef.type.pushTop(basetype);
        iddef.name = "__noname_para_" + noname_para_index++;
        return iddef;
    }
    else
    {
        IDdef tp;
        tp.type = basetype;
        tp.name = "__noname_para_" + noname_para_index++;
        return tp;
    }
    throw;
}
void astVisitor::visitBlockItemList(ComplierParser::BlockItemListContext* ctx)
{
    for (auto each : ctx->blockItem())
    {
        visitBlockItem(each);
    }
}
void astVisitor::visitStatement(ComplierParser::StatementContext* ctx)
{
    if (ctx->compoundStatement())
    {
        return visitCompoundStatement(ctx->compoundStatement());
    }
    else if (ctx->expressionStatement())
    {
        return visitExpressionStatement(ctx->expressionStatement());
    }
    else if (ctx->selectionStatement())
    {
        return visitSelectionStatement(ctx->selectionStatement());
    }
    else if (ctx->iterationStatement())
    {
        return visitIterationStatement(ctx->iterationStatement());
    }
    else if (ctx->jumpStatement())
    {
        return visitJumpStatement(ctx->jumpStatement());
    }
    // [TODO] lable system
    // else if (ctx->labeledStatement())
    // {
    //     return visitLabeledStatement(ctx->labeledStatement());
    // }
}
void astVisitor::visitExpressionStatement(ComplierParser::ExpressionStatementContext* ctx)
{
    if (ctx->expression())
    {
        (void)(visitExpression(ctx->expression()));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
    }
}
Type astVisitor::visitExpression(ComplierParser::ExpressionContext* ctx)
{
    for (int i = 0; i < ctx->assignmentExpression().size(); i++)
    {
        auto ret = (visitAssignmentExpression(ctx->assignmentExpression()[i]));
        if (i != ctx->assignmentExpression().size() - 1)
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        }
        else
        {
            return ret;
        }
    }
    throw;
}
Type astVisitor::visitAssignmentExpression(ComplierParser::AssignmentExpressionContext* ctx)
{
    // [TODO] DigitSequence
    if (ctx->conditionalExpression())
    {
        return visitConditionalExpression(ctx->conditionalExpression());
    }
    else if (ctx->assignmentOperator())
    {
        auto uret = visitUnaryExpression(ctx->unaryExpression());
        if (!stackTopIsLvalue()) // 不是左值
        {
            THROW_ERR(error::expected_lvalue, ctx->unaryExpression());
        }
        madeTopIsLvalueAddr();
        if (ctx->assignmentOperator()->getText() == "=")
        {
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::COPY}); // 拷贝一份左值地址实现返回值
            (void)(visitAssignmentExpression(ctx->assignmentExpression()));
            saveStackTopAddrValueByType(uret);
            loadStackTopAddrByType(uret);
        }
        else
        {
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::COPY}); // 拷贝一份左值地址实现返回值
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::COPY}); // 再拷贝一份用于加载左操作数

            loadStackTopAddrByType(uret); // 加载左操作数值

            (void)(visitAssignmentExpression(ctx->assignmentExpression()));

            if (ctx->assignmentOperator()->getText() == "+=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            }
            else if (ctx->assignmentOperator()->getText() == "-=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            }
            else if (ctx->assignmentOperator()->getText() == "*=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            }
            else if (ctx->assignmentOperator()->getText() == "/=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::DIV});
            }
            else if (ctx->assignmentOperator()->getText() == "%=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MOD});
            }
            else if (ctx->assignmentOperator()->getText() == "<<=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LSHIFT});
            }
            else if (ctx->assignmentOperator()->getText() == ">>=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RSHIFT});
            }
            else if (ctx->assignmentOperator()->getText() == "^=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::XOR});
            }
            saveStackTopAddrValueByType(uret);
            loadStackTopAddrByType(uret);
        }
        return uret;
    }
    throw;
}
Type astVisitor::visitConditionalExpression(ComplierParser::ConditionalExpressionContext* ctx)
{
    if (ctx->logicalOrExpression())
    {
        auto lret = (visitLogicalOrExpression(ctx->logicalOrExpression()));
        return lret;
    }
    if (ctx->expression() && ctx->conditionalExpression())
    {
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD");
        (void)(visitExpression(ctx->expression()));
        funcnow->funcInfo.asms[pos] =
            ASM{ASM::basic_asm::JZ, funcnow->funcInfo.asms.size() + 1}; // 跳过JMP
        int pos2 = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD");
        (void)(visitConditionalExpression(ctx->conditionalExpression()));
        funcnow->funcInfo.asms[pos2] = ASM{ASM::basic_asm::JMP, funcnow->funcInfo.asms.size()};
        return Type{Type::Kind::Basic, Type::BasicType::Char};
    }
    throw;
}
// same as visitDeclarationSpecifiers
std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>> astVisitor::
    visitDeclarationSpecifiers2(ComplierParser::DeclarationSpecifiers2Context* ctx)
{
    std::optional<Type> type;
    std::optional<StorageClassSpecifier> storageClassSpecifier;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        // [TODO] typeQualifier functionSpecifier alignmentSpecifier
        Type result = visitDeclarationSpecifier(each);
        if (result.storageClassSpecifier != StorageClassSpecifier::VarDef)
        {
            if (storageClassSpecifier)
            {
                THROW_ERR(error::double_StorageClassSpecifier, ctx);
            }
            storageClassSpecifier = result.storageClassSpecifier;
        }
        else
        {
            if (type)
            {
                THROW_ERR(error::double_type, ctx);
            }
            type = result;
        }
    }
    return {type, storageClassSpecifier};
}
Type astVisitor::visitLogicalOrExpression(ComplierParser::LogicalOrExpressionContext* ctx)
{
    // 注意处理多个||
    std::function<Type(std::span<ComplierParser::LogicalAndExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::LogicalAndExpressionContext*> in) -> Type
    {
        if (in.size() == 1)
        {
            return (visitLogicalAndExpression(in[0]));
        }
        auto lret = (visitLogicalAndExpression(in[0]));
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms[pos] = ASM{ASM::basic_asm::JNZ, funcnow->funcInfo.asms.size()};
        // 返回计算结果类型
        return rret; // 语义上应该是整型，保持右值类型沿用
    };
    return func(std::span<ComplierParser::LogicalAndExpressionContext*>(
        ctx->logicalAndExpression().data(), ctx->logicalAndExpression().size()));
}
Type astVisitor::visitLogicalAndExpression(ComplierParser::LogicalAndExpressionContext* ctx)
{
    // 注意处理多个&&
    std::function<Type(std::span<ComplierParser::InclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::InclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitInclusiveOrExpression(in[0]));
        }
        auto lret = (visitInclusiveOrExpression(in[0]));
        // 保存当前位置，用于生成条件跳转指令
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms[pos] = ASM{ASM::basic_asm::JZ, funcnow->funcInfo.asms.size()};
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::InclusiveOrExpressionContext*>(
        ctx->inclusiveOrExpression().data(), ctx->inclusiveOrExpression().size()));
}
Type astVisitor::visitInclusiveOrExpression(ComplierParser::InclusiveOrExpressionContext* ctx)
{
    // 注意处理多个|
    std::function<Type(std::span<ComplierParser::ExclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::ExclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitExclusiveOrExpression(in[0]));
        }
        auto lret = (visitExclusiveOrExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::OR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::ExclusiveOrExpressionContext*>(
        ctx->exclusiveOrExpression().data(), ctx->exclusiveOrExpression().size()));
}
Type astVisitor::visitExclusiveOrExpression(ComplierParser::ExclusiveOrExpressionContext* ctx)
{
    // 注意处理多个^
    std::function<Type(std::span<ComplierParser::AndExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::AndExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitAndExpression(in[0]));
        }
        auto lret = (visitAndExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::AndExpressionContext*>(ctx->andExpression().data(),
                                                                 ctx->andExpression().size()));
}
Type astVisitor::visitAndExpression(ComplierParser::AndExpressionContext* ctx)
{
    // 注意处理多个&
    std::function<Type(std::span<ComplierParser::EqualityExpressionContext*>)> func =
        [&func, this, ctx](std::span<ComplierParser::EqualityExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitEqualityExpression(in[0]));
        }
        auto lret = (visitEqualityExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::EqualityExpressionContext*>(
        ctx->equalityExpression().data(), ctx->equalityExpression().size()));
}
Type astVisitor::visitEqualityExpression(ComplierParser::EqualityExpressionContext* ctx)
{
    // 注意处理多个!= / ==
    std::function<Type(std::span<ComplierParser::RelationalExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::RelationalExpressionContext*> in,
                           int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitRelationalExpression(in[0]));
        }
        auto lret = (visitRelationalExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        if (ctx->children[index * 2 + 1]->getText() == "==")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CMP});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "!=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CMPN});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::RelationalExpressionContext*>(
                    ctx->relationalExpression().data(), ctx->relationalExpression().size()),
                0);
}
Type astVisitor::visitRelationalExpression(ComplierParser::RelationalExpressionContext* ctx)
{
    // 注意处理多个< > <= >=
    std::function<Type(std::span<ComplierParser::ShiftExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::ShiftExpressionContext*> in, int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitShiftExpression(in[0]));
        }
        auto lret = (visitShiftExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        if (ctx->children[index * 2 + 1]->getText() == "<")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SMALL});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::BIG});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "<=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SMALLE});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::BIGE});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::ShiftExpressionContext*>(ctx->shiftExpression().data(),
                                                                   ctx->shiftExpression().size()),
                0);
}
Type astVisitor::visitShiftExpression(ComplierParser::ShiftExpressionContext* ctx)
{
    // 注意处理多个<< >>
    std::function<Type(std::span<ComplierParser::AdditiveExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::AdditiveExpressionContext*> in,
                           int index) -> Type

    {
        if (in.size() == 1)
        {
            return (visitAdditiveExpression(in[0]));
        }
        auto lret = (visitAdditiveExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        if (ctx->children[index * 2 + 1]->getText() == "<<")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LSHIFT});
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">>")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RSHIFT});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::AdditiveExpressionContext*>(
                    ctx->additiveExpression().data(), ctx->additiveExpression().size()),
                0);
}
Type astVisitor::visitAdditiveExpression(ComplierParser::AdditiveExpressionContext* ctx)
{
    std::function<Type(std::span<ComplierParser::MultiplicativeExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::MultiplicativeExpressionContext*> in,
                           int index) -> Type
    {
        if (in.size() == 1)
        {
            return (visitMultiplicativeExpression(in[0]));
        }
        auto posnow = funcnow->funcInfo.asms.size();
        auto lret = (visitMultiplicativeExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        funcnow->funcInfo.asms.resize(posnow); // 之前只是为了拿到类型
        if (lret.kind == Type::Kind::Pointer && rret.kind == Type::Kind::Basic &&
            (rret.basic_type == Type::BasicType::Int || rret.basic_type == Type::BasicType::Char ||
             rret.basic_type == Type::BasicType::Long))
        {
            // 左指针右整形
            auto lret = (visitMultiplicativeExpression(in[0]));
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, lret.subType->getsize()});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
        }
        else if (rret.kind == Type::Kind::Pointer && lret.kind == Type::Kind::Basic &&
                 (lret.basic_type == Type::BasicType::Int ||
                  lret.basic_type == Type::BasicType::Char ||
                  lret.basic_type == Type::BasicType::Long))
        {
            // 左整形右指针
            auto lret = (visitMultiplicativeExpression(in[0]));
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, rret.subType->getsize()});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        else // 其他类型不做特殊处理
        {
            auto lret = (visitMultiplicativeExpression(in[0]));
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        std::string op_token = ctx->children[index * 2 + 1]->getText();
        if (op_token == "+")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            auto res = deduce_binary_type(lret, rret, BinOp::Add);
            return res;
        }
        else if (op_token == "-")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            auto res = deduce_binary_type(lret, rret, BinOp::Sub);
            return res;
        }
        THROW_ERR(error::unsurpported_op, ctx);
    };
    return func(std::span<ComplierParser::MultiplicativeExpressionContext*>(
                    ctx->multiplicativeExpression().data(), ctx->multiplicativeExpression().size()),
                0);
}
Type astVisitor::visitMultiplicativeExpression(ComplierParser::MultiplicativeExpressionContext* ctx)
{
    // 注意处理多个* / %
    std::function<Type(std::span<ComplierParser::CastExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<ComplierParser::CastExpressionContext*> in, int index) -> Type
    {
        if (in.size() == 1)
        {
            return (visitCastExpression(in[0]));
        }
        auto lret = (visitCastExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        if (ctx->children[index * 2 + 1]->getText() == "*")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "/")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::DIV});
        }
        else if (ctx->children[index * 2 + 1]->getText() == "%")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MOD});
        }
        // 返回计算结果类型
        return rret;
    };
    return func(std::span<ComplierParser::CastExpressionContext*>(ctx->castExpression().data(),
                                                                  ctx->castExpression().size()),
                0);
}
Type astVisitor::visitCastExpression(ComplierParser::CastExpressionContext* ctx)
{
    if (ctx->castExpression())
    {
        auto cret = visitCastExpression(ctx->castExpression());
        // [TODO] visitTypeName
        auto tret = visitTypeName(ctx->typeName());
        return tret;
    }

    return visitUnaryExpression(ctx->unaryExpression());

    //[TODO] DigitSequence 何意义?
    throw;
}

Type astVisitor::visitUnaryExpression(ComplierParser::UnaryExpressionContext* ctx)
{
    Type type;
    auto sizenow = funcnow->funcInfo.asms.size();
    if (ctx->postfixExpression())
    {
        auto pret = (visitPostfixExpression(ctx->postfixExpression()));
        type = pret;
    }
    else if (ctx->unaryOperator())
    {
        if (ctx->unaryOperator()->getText() == "&")
        {
            auto cret = visitCastExpression(ctx->castExpression());
            if (cret.kind == Type::Kind::Function ||
                cret.kind == Type::Kind::Array) // arr/function无LC/LI/LW,取地址与值相同，无需处理
            {
                type = Type{Type::Kind::Pointer, 1};
                type.pushTop(cret);
            }
            else
            {
                if (stackTopIsLvalue())
                {
                    madeTopIsLvalueAddr();
                }
                else
                {
                    THROW_ERR(error::expected_lvalue, ctx->castExpression());
                }
                // 在现有类型上再套一层 Pointer
                Type outer;
                outer.kind = Type::Kind::Pointer;
                outer.subType = cret;
                type = outer;
            }
        }
        else if (ctx->unaryOperator()->getText() == "*")
        {
            auto cret = (visitCastExpression(ctx->castExpression()));
            if (cret.kind != Type::Kind::Pointer)
            {
                THROW_ERR(error::expected_ptr, ctx->castExpression());
            }
            type = *cret.subType;
            loadStackTopAddrByType(type);
        }
        else if (ctx->unaryOperator()->getText() == "+") //+12
        {
            auto cret = (visitCastExpression(ctx->castExpression()));
            type = cret;
        }
        else if (ctx->unaryOperator()->getText() == "-") //-12
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            auto cret = (visitCastExpression(ctx->castExpression()));
            type = cret;
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
        }
    }
    else if (ctx->typeName())
    {
        // 找到 typeName 在 children 中的位置，检查前面是 sizeof 还是 _Alignof
        // 结构: ... sizeof/alignof '(' typeName ')'
        auto cret = visitTypeName(ctx->typeName());

        // 遍历 children 找到 '(' typeName ')' 前的关键字
        for (size_t i = 0; i < ctx->children.size(); i++)
        {
            // 找到 typeName 对应的 child
            if (ctx->children[i] == ctx->typeName())
            {
                // 向前找到对应的关键字 (跳过 '(')
                // children[i-1] 是 '(', children[i-2] 是 sizeof 或 _Alignof
                if (i >= 2)
                {
                    std::string keyword = ctx->children[i - 2]->getText();
                    if (keyword == "sizeof")
                    {
                        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, cret.getsize()});
                        type = Type{Type::Kind::Basic, Type::BasicType::Long};
                    }
                    else if (keyword == "_Alignof")
                    {
                        // [TODO] _Alignof 实现
                        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, 8});
                        type = Type{Type::Kind::Basic, Type::BasicType::Long};
                    }
                }
                break;
            }
        }
    }
    for (int i = ctx->children.size() - 1; i >= 0; i--)
    {
        if (ctx->children[i]->getText() == "++")
        {
            if (stackTopIsLvalue())
            {
                madeTopIsLvalueAddr();
            }
            else
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            loadStackTopAddrByType(type);
            long step = 1;
            if (type.kind == Type::Kind::Pointer)
            {
                step =
                    static_cast<long>(type.subType ? type.subType->getsize() : VCPU<>::size_word);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            saveStackTopAddrValueByType(type);
            loadStackTopAddrByType(type);
        }
        else if (ctx->children[i]->getText() == "--")
        {
            if (stackTopIsLvalue())
            {
                madeTopIsLvalueAddr();
            }
            else
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            loadStackTopAddrByType(type);
            long step = 1;
            if (type.kind == Type::Kind::Pointer)
            {
                step =
                    static_cast<long>(type.subType ? type.subType->getsize() : VCPU<>::size_word);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            saveStackTopAddrValueByType(type);
            loadStackTopAddrByType(type);
        }
        else if (ctx->children[i]->getText() == "sizeof" &&
                 ctx->children[i + 1]->getText() != "(") // 排除分支3的sizeof
        {
            // sizeof无副作用, 回退asm
            funcnow->funcInfo.asms.resize(sizenow);
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, type.getsize()});
            type = Type{Type::Kind::Basic, Type::BasicType::Int};
        }
    }
    return type;
}
Type astVisitor::visitPostfixExpression(ComplierParser::PostfixExpressionContext* ctx)
{
    // 注意处理多个后缀
    int idindex = ctx->Identifier().size() - 1;
    std::function<Type(int, int)> func = [&func, this, ctx, &idindex](int start, int end) -> Type
    {
        if (end == 0)
        {
            auto str = ctx->children[0]->getText();
            return visitPrimaryExpression(
                dc<ComplierParser::PrimaryExpressionContext*>(ctx->children[0]));
        }
        auto todo = ctx->children[end - 1];
        if (todo->getText() == "(") // func call
        {
            size_t args_words = 0;

            if (ctx->children[end]->getText() != ")") // 有expressionlist
            {
                args_words = visitArgumentExpressionList(
                    dc<ComplierParser::ArgumentExpressionListContext*>(ctx->children[end]));
            }
            auto funtype = tryVisitType([this, &func, end]() { return func(0, end - 1); });
            // 返回值为struct
            if (funtype && (*funtype).subType->kind == Type::Kind::Struct)
            {
                // args_words +=
                //     ((*funtype).subType->getsize() + VCPU<>::size_word - 1) / VCPU<>::size_word;
                args_words += 1; // 传入指针
                // 创建局部变量传入指针
                IDdef struct_ret;
                struct_ret.type = (*funtype).subType;
                struct_ret.kind = IDdef::Kind::Local;
                static size_t index = 0;
                struct_ret.name = "__struct_ret" + index++;
                auto def = record_ID_decl(struct_ret, funcnow);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, def->addr});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEA, def->addr});
            }
            auto funcaddr = func(0, end - 1); // 解析函数地址
            // funcaddr.removeID();
            // if (funcaddr.kind == Type::Kind::ID)
            // {
            //     auto tp = *funcaddr.subType;
            //     funcaddr = tp;
            // }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CALL});
            Type rettype = funcaddr;
            if (funcaddr.kind == Type::Kind::Pointer && funcaddr.subType &&
                funcaddr.subType->kind == Type::Kind::Function) // 函数指针
            {
                rettype = *funcaddr.subType->subType;
            }
            else if (funcaddr.kind == Type::Kind::Function) // 函数
            {
                rettype = *funcaddr.subType;
            }
            else
            {
                THROW_ERR(error::expected_func_or_funcptr, ctx);
            }
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::DARG, static_cast<int>(args_words)});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::PUSH});
            if (funtype && (*funtype).subType->kind == Type::Kind::Struct)
            {
                funcnow->funcInfo.asms.push_back(
                    ASM{ASM::basic_asm::LODS, (*funtype).subType->getsize()});
            }
            return rettype;
        }
        else if (todo->getText() == "[") // arr[] ptr[]
        {
            auto paret = func(0, end - 1);
            Type eleType;
            if (paret.kind == Type::Kind::Pointer && paret.subType)
            {
                eleType = *paret.subType;
            }
            else if (paret.kind == Type::Kind::Array)
            {
                eleType = *paret.subType;
            }
            (void)(visitExpression(dc<ComplierParser::ExpressionContext*>(ctx->children[end])));
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, eleType.getsize()});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            loadStackTopAddrByType(eleType);
            return eleType;
        }
        else if (todo->getText() == "++" || todo->getText() == "--")
        {
            // 后置 ++/--: 返回原值，变量自增/自减
            auto valType = func(0, end - 1);
            if (!stackTopIsLvalue())
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            madeTopIsLvalueAddr(); // 栈顶现在是地址

            // 栈: [addr]
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY}); // [addr][addr]
            loadStackTopAddrByType(valType);                             // [addr][orig]
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY}); // [addr][orig][orig]
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP}); // ax=orig, 栈: [addr][orig]

            long step = 1;
            if (valType.kind == Type::Kind::Pointer)
            {
                auto pointed = valType.subType ? valType.subType->getsize() : VCPU<>::size_word;
                step = static_cast<long>(pointed);
            }

            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->funcInfo.asms.push_back(
                ASM{todo->getText() == "++" ? ASM::basic_asm::ADD : ASM::basic_asm::SUB});
            // 栈: [addr][new_val]
            saveStackTopAddrValueByType(valType);                        // 栈: []
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::PUSH}); // [orig]
            return valType;
        }
        else if (todo->getText() == ".") // 结构体直接成员访问运算符
        {
            auto type = func(0, end - 1);
            auto membername = ctx->Identifier()[idindex--]->getText();
            if (type.kind != Type::Kind::Struct)
            {
                THROW_ERR(error::expected_struct, ctx);
            }
            if (stackTopIsLvalue())
            {
                madeTopIsLvalueAddr();
            }
            auto memvars = type.structInfo.getmember(membername);
            if (!memvars)
            {
                THROW_ERR(error::undifined_feild, ctx);
            }
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::IMM, type.structInfo.getmemberbias(membername)});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            loadStackTopAddrByType(memvars->type);
            return memvars->type;
        }
        else if (todo->getText() == "->") //[TODO] 结构体间接成员访问运算符
        {
            auto type = func(0, end - 1);
            auto membername = ctx->Identifier()[idindex--]->getText();
            if (type.kind != Type::Kind::Pointer || type.subType->kind != Type::Kind::Struct)
            {
                THROW_ERR(error::expected_struct_ptr, ctx);
            }
            auto memvars = type.subType->structInfo.getmember(membername);
            if (!memvars)
            {
                THROW_ERR(error::undifined_feild, ctx);
            }
            funcnow->funcInfo.asms.push_back(
                ASM{ASM::basic_asm::IMM, type.subType->structInfo.getmemberbias(membername)});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            loadStackTopAddrByType(memvars->type);
            return memvars->type;
        }
        else
        {
            return func(0, end - 1);
        }
        throw;
    };
    return func(0, ctx->children.size());
}
Type astVisitor::visitPrimaryExpression(ComplierParser::PrimaryExpressionContext* ctx)
{
    if (ctx->Identifier())
    {
        auto texts = ctx->Identifier()->getText();
        return load_var_or_func(ctx->Identifier()->getText());
    }
    else if (ctx->Constant()) // 常量处理
    {
        std::string text = ctx->Constant()->getText();

        // 处理字符常量
        if (isCharacterConstant(text))
        {
            try
            {
                int value = parseCharacterConstant(text);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, value});
                return Type{Type::Kind::Basic, Type::BasicType::Char};
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->Constant());
            }
        }
        // 处理整型常量
        else if (isIntegerConstant(text))
        {
            try
            {
                int value = parseIntegerConstant(text);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, value});
                return Type{Type::Kind::Basic, Type::BasicType::Int};
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->Constant());
            }
        }
        else // [TODO] 浮点数
        {
            THROW_ERR(error::unsurpported_num, ctx->Constant());
        }
    }
    else if (ctx->expression()) // (expr)
    {
        return visitExpression(ctx->expression());
    }
    else if (ctx->StringLiteral().size())
    {
        auto [arrptr, chars_after_transed] = madeConstString(ctx->StringLiteral());
        gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, "globalvar@" + arrptr->name});
        gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEAD});
        for (int i = 0; i < chars_after_transed.size() - 1; i++) // 复制n-1次
        {
            gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
        }
        for (int i = 0; i < chars_after_transed.size(); i++)
        {
            gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, i});
            gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, chars_after_transed[i]});
            gfuncptr->funcInfo.asms.push_back(ASM{ASM::basic_asm::SC});
        }
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, "globalvar@" + arrptr->name});
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEAD});
        // 返回指向字符的指针类型
        Type t{Type::Kind::Basic, Type::BasicType::Char};
        t.pushTop(Type{Type::Kind::Pointer, 1});
        return t;
    }
    else
    {
        auto str = ctx->getText();
        THROW_ERR(error::invalid_constant, ctx);
    }
    throw;
}
Type astVisitor::visitTypeName(ComplierParser::TypeNameContext* ctx)
{
    // typeName由specifierQualifierList和可选的abstractDeclarator组成
    // [TODO] 没有标识符, 修饰符的申明
    auto [typeQualifiers, basetype] = visitSpecifierQualifierList(ctx->specifierQualifierList());
    if (ctx->abstractDeclarator())
    {
        auto tp = visitAbstractDeclarator(ctx->abstractDeclarator());
        tp.pushTop(*basetype);
        return tp;
    }
    return *basetype;
}

void astVisitor::visitBlockItem(ComplierParser::BlockItemContext* ctx)
{
    if (ctx->statement())
    {
        return visitStatement(ctx->statement());
    }
    else if (ctx->declaration())
    {
        // addDeclarations(visitDeclaration(ctx->declaration()));
        visitDeclaration(ctx->declaration());
    }
    else if (ctx->asmADDer())
    {
        visitAsmADDer(ctx->asmADDer());
    }
}

std::tuple<std::vector<Type::TypeQualifier>, std::optional<Type>> astVisitor::
    visitSpecifierQualifierList(ComplierParser::SpecifierQualifierListContext* ctx,
                                std::vector<Type::TypeQualifier> typeQualifier)
{
    std::optional<Type> base;
    if (ctx->typeQualifier())
    {
        auto str = ctx->typeQualifier()->getText();
        if (str == "const")
        {
            typeQualifier.push_back(Type::TypeQualifier::Const);
        }
        else if (str == "restrict")
        {
            typeQualifier.push_back(Type::TypeQualifier::Restrict);
        }
        else if (str == "volatile")
        {
            typeQualifier.push_back(Type::TypeQualifier::Volatile);
        }
        else if (str == "_Atomic")
        {
            typeQualifier.push_back(Type::TypeQualifier::_Atomic);
        }
    }
    else if (ctx->typeSpecifier())
    {
        base = visitTypeSpecifier(ctx->typeSpecifier());
    }

    if (ctx->specifierQualifierList())
    {
        auto [typeQualifiers, type] =
            visitSpecifierQualifierList(ctx->specifierQualifierList(), typeQualifier);
        if (type && base_type)
        {
            THROW_ERR(error::double_type, ctx);
        }
        base = type ? type : base;
    }
    return {typeQualifier, base};
}

void astVisitor::visitSelectionStatement(ComplierParser::SelectionStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "if")
    {
        (void)(visitExpression(ctx->expression()));
        int after_expr = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // for jz
        visitStatement(ctx->statement()[0]);
        if (ctx->statement().size() == 2) // 有else
        {
            int after_id = funcnow->funcInfo.asms.size();
            funcnow->funcInfo.asms.push_back(
                "HOLD"); // for 'if' statments jump through 'else' to end
            funcnow->funcInfo.asms[after_expr] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};

            visitStatement(ctx->statement()[1]);
            funcnow->funcInfo.asms[after_id] = ASM{
                ASM::basic_asm::JMP, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        }
        else // 无else
        {
            funcnow->funcInfo.asms[after_expr] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        }
    }
    // [TODO] switch
}

size_t astVisitor::visitArgumentExpressionList(ComplierParser::ArgumentExpressionListContext* ctx)
{
    // 实参从右向左入栈，返回占用的 word 数（按 8 字节向上取整）
    size_t words = 0;
    for (int i = static_cast<int>(ctx->assignmentExpression().size()) - 1; i >= 0; --i)
    {
        Type arg = visitAssignmentExpression(ctx->assignmentExpression()[i]);

        if (arg.kind == Type::Kind::Struct)
        {
            if (!stackTopIsLvalue())
            {
                THROW_ERR(error::expected_lvalue, ctx->assignmentExpression()[i]);
            }
            madeTopIsLvalueAddr();
            // 目前仅支持可取地址的结构体实参（左值）。假定栈顶为地址。
            const size_t n = arg.getsize();
            const size_t word = VCPU<>::size_word;
            const size_t chunks = align_up(n, word) / word;
            // 地址已在栈顶（绝对地址），直接批量拷贝。
            // funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, static_cast<int>(n)});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LODS, n});
            words += chunks;
        }
        else
        {
            const size_t sz = arg.getsize();
            if (sz == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
            {
                words += 1; // char promoted to word on stack (already word)
            }
            else if (sz == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
            {
                words += 1;
            }
            else if (sz == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
            {
                words += 1;
            }
            else
            {
                // 其他类型按 word 对齐
                words += (sz + VCPU<>::size_word - 1) / VCPU<>::size_word;
            }
        }
    }
    return words;
}

void astVisitor::visitIterationStatement(ComplierParser::IterationStatementContext* ctx)
{
    if (ctx->children[0]->getText() == "while")
    {
        int startppos = funcnow->funcInfo.asms.size();
        visitExpression(ctx->expression());
        int after_expr = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // for jz end
        visitStatement(ctx->statement());
        funcnow->funcInfo.asms.push_back(
            ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(startppos)});
        funcnow->funcInfo.asms[after_expr] =
            ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};

        for (int i = startppos; i < funcnow->funcInfo.asms.size(); i++)
        {
            if (funcnow->funcInfo.asms[i] == "lable@break")
            {
                funcnow->funcInfo.asms[i] =
                    ASM{ASM::basic_asm::JMP,
                        "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
            }
            else if (funcnow->funcInfo.asms[i] == "lable@continue")
            {
                funcnow->funcInfo.asms[i] =
                    ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(startppos)};
            }
        }
    }
    else if (ctx->children[0]->getText() == "for")
    {
        obj.enter_decl_scope("for");
        auto* forCond = ctx->forCondition();
        if (!forCond)
        {
            THROW_ERR(error::unsurpported_op, ctx);
        }

        auto visitforExpression = [this](ComplierParser::ForExpressionContext* exprCtx) -> Type
        {
            if (!exprCtx)
            {
                throw error::invalid_constant;
            }
            Type ret;
            for (int i = 0; i < exprCtx->assignmentExpression().size(); i++)
            {
                ret = (visitAssignmentExpression(exprCtx->assignmentExpression()[i]));
                if (i != exprCtx->assignmentExpression().size() - 1)
                {
                    funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
                }
            }
            return ret;
        };

        if (auto* decl = forCond->forDeclaration())
        {
            // addDeclarations(lowerDeclaration(decl->declarationSpecifiers(),
            // decl->initDeclaratorList()));
            lowerDeclaration(decl->declarationSpecifiers(), decl->initDeclaratorList());
        }
        else if (auto* initExpr = forCond->expression())
        {
            (void)(visitExpression(initExpr));
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        }

        auto condExprs = forCond->forExpression();
        ComplierParser::ForExpressionContext* condExpr = nullptr;
        ComplierParser::ForExpressionContext* postExpr = nullptr;
        if (!condExprs.empty())
        {
            condExpr = condExprs[0];
            if (condExprs.size() > 1)
            {
                postExpr = condExprs[1];
            }
        }

        int loop_condition_pos = funcnow->funcInfo.asms.size();
        int after_condition = -1;
        if (condExpr)
        {
            (void)(visitforExpression(condExpr));
            after_condition = funcnow->funcInfo.asms.size();
            funcnow->funcInfo.asms.push_back("HOLD"); // for jz end
        }

        visitStatement(ctx->statement());

        int update_pos = funcnow->funcInfo.asms.size();
        if (postExpr)
        {
            (void)(visitforExpression(postExpr));
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        }

        funcnow->funcInfo.asms.push_back(
            ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(loop_condition_pos)});

        if (after_condition != -1)
        {
            funcnow->funcInfo.asms[after_condition] =
                ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        }

        int continue_target = postExpr ? update_pos : loop_condition_pos;
        for (int i = loop_condition_pos; i < funcnow->funcInfo.asms.size(); i++)
        {
            if (funcnow->funcInfo.asms[i] == "lable@break")
            {
                funcnow->funcInfo.asms[i] =
                    ASM{ASM::basic_asm::JMP,
                        "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
            }
            else if (funcnow->funcInfo.asms[i] == "lable@continue")
            {
                funcnow->funcInfo.asms[i] =
                    ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(continue_target)};
            }
        }
        obj.exit_decl_scope();
    }
    // [TODO] 'do-while' statement
}

void astVisitor::visitJumpStatement(ComplierParser::JumpStatementContext* ctx)
{
    // [TODO] goto statement
    if (ctx->children[0]->getText() == "continue")
    {
        funcnow->funcInfo.asms.push_back("lable@continue");
    }
    else if (ctx->children[0]->getText() == "break")
    {
        funcnow->funcInfo.asms.push_back("lable@break");
    }
    else if (ctx->children[0]->getText() == "return")
    {
        // 获取当前函数的返回类型
        const Type rettype = (funcnow->type.subType) ? *funcnow->type.subType : Type{};
        const bool is_struct_return = (rettype.kind == Type::Kind::Struct);

        if (ctx->expression())
        {
            Type exprType = visitExpression(ctx->expression());

            if (is_struct_return)
            {
                // 结构体返回：将返回值写入隐式传入的返回指针
                // 隐式返回指针是第一个参数，偏移为 2 * size_word（跳过old bp和ret addr）
                const size_t ret_ptr_offset = 2 * VCPU<>::size_word;

                // 如果表达式结果是左值，取其地址
                if (stackTopIsLvalue())
                {
                    madeTopIsLvalueAddr();
                }
                // 栈顶现在是源地址

                // 加载隐式返回指针（目标地址）
                funcnow->funcInfo.asms.push_back(
                    ASM{ASM::basic_asm::IMM, static_cast<int>(ret_ptr_offset)});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEA});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LW}); // 解引用获取实际目标地址

                // 交换使得 栈: [dest][src]
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SWAP});

                // 拷贝结构体内容
                const size_t struct_size = rettype.getsize();
                funcnow->funcInfo.asms.push_back(
                    ASM{ASM::basic_asm::MOVS, static_cast<int>(struct_size)});

                // 返回隐式返回指针的值（用于链式调用）
                funcnow->funcInfo.asms.push_back(
                    ASM{ASM::basic_asm::IMM, static_cast<int>(ret_ptr_offset)});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEA});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LW});
            }
            // 非结构体返回：表达式值已在栈顶
        }
        else
        {
            // 空返回按0返回
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, 0});
        }
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RET});
    }
}

std::vector<std::pair<std::string, IDdef>> astVisitor::visitStructDeclarationList(
    ComplierParser::StructDeclarationListContext* ctx)
{
    long nonameindex = 0;
    std::vector<std::pair<std::string, IDdef>> members;
    std::vector<IDdef> tps;
    for (auto each : ctx->structDeclaration())
    {
        // [TODO]  目前忽略 const volatile restrict _Atomic
        auto [typeQualifier, basetype] =
            visitSpecifierQualifierList(each->specifierQualifierList());
        if (!basetype)
        {
            THROW_ERR(error::expected_type, ctx);
        }
        if (!each->structDeclaratorList()) // 成员无名
        {
            IDdef tpvar;
            tpvar.type = *basetype;
            tpvar.name = "__noname_member_" + nonameindex;
            tpvar.valueType = ValueType::Left;
            tps.push_back(tpvar);
        }
        else
        {
            std::vector<IDdef> eachstructDeclarationTypes;
            for (auto eachstructDeclarator : each->structDeclaratorList()->structDeclarator())
            {

                auto declarator = visitDeclarator(eachstructDeclarator->declarator());
                declarator.valueType = ValueType::Left;
                eachstructDeclarationTypes.push_back(declarator);
            }
            for (auto& eachtype : eachstructDeclarationTypes)
            {
                eachtype.type.pushTop(*basetype);
            }
            for (auto& et : eachstructDeclarationTypes)
            {
                tps.push_back(et);
            }
        }
    }
    // 分配成员空间
    for (int i = 0; i < tps.size(); i++)
    {
        IDdef tpvar = tps[i];
        if (i == 0)
        {
            tpvar.addr = 0;
        }
        else
        {
            tpvar.addr = align_up(members[i - 1].second.addr + members[i - 1].second.type.getsize(),
                                  std::min((size_t)8, tps[i].type.getsize()));
        }
        members.push_back({tpvar.name, tpvar});
    }
    return members;
}

Type astVisitor::visitAbstractDeclarator(ComplierParser::AbstractDeclaratorContext* ctx)
{
    Type rettype;
    if (ctx->directAbstractDeclarator())
    {
        rettype = visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
    }
    if (ctx->pointer())
    {
        rettype.pushTop(Type(Type::Kind::Pointer,
                             static_cast<int>(std::count(ctx->pointer()->getText().begin(),
                                                         ctx->pointer()->getText().end(), '*'))));
    }
    return rettype;
}

Type astVisitor::visitDirectAbstractDeclarator(ComplierParser::DirectAbstractDeclaratorContext* ctx)
{
    Type basetype;
    if (ctx->LeftParen() && ctx->abstractDeclarator()) // (abst)
    {
        basetype = visitAbstractDeclarator(ctx->abstractDeclarator());
        return basetype;
    }
    if (ctx->directAbstractDeclarator())
    {
        basetype = visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
    }
    if (ctx->LeftBracket() && ctx->assignmentExpression()) // arr
    {
        // parseConstexpr 返回 long long, 这里数组维度内部使用 int, 显式窄化避免警告
        basetype.pushTop(
            Type(Type::Kind::Array, static_cast<int>(parseConstexpr(ctx->assignmentExpression()))));
        return basetype;
    }
    else if (ctx->LeftParen()) // func
    {
        std::vector<IDdef> args;
        if (ctx->parameterTypeList())
        {
            args = visitParameterTypeList(ctx->parameterTypeList());
        }
        basetype.pushTop(Type{Type::Kind::Function, args});
        return basetype;
    }
    THROW_ERR(error::unsurpport_abstractDeclarator, ctx);
}

long long astVisitor::parseConstexpr(ComplierParser::AssignmentExpressionContext* expr)
{
    // 递归下降求值，仅用于编译期整型常量表达式（如数组维度）
    // 支持：括号、整型/字符常量、单目 + - ~
    // !、*,/,%、+,-、<<,>>、<,<=,>,>=、==,!=、&,^,|、&&,||、?:、逗号表达式
    // 不支持：标识符、函数调用、下标、sizeof/_Alignof、赋值类运算等

    // 前置声明一组局部 lambda，互相递归
    std::function<long long(ComplierParser::PrimaryExpressionContext*)> evalPrimary;
    std::function<long long(ComplierParser::PostfixExpressionContext*)> evalPostfix;
    std::function<long long(ComplierParser::UnaryExpressionContext*)> evalUnary;
    std::function<long long(ComplierParser::CastExpressionContext*)> evalCast;
    std::function<long long(ComplierParser::MultiplicativeExpressionContext*)> evalMul;
    std::function<long long(ComplierParser::AdditiveExpressionContext*)> evalAdd;
    std::function<long long(ComplierParser::ShiftExpressionContext*)> evalShift;
    std::function<long long(ComplierParser::RelationalExpressionContext*)> evalRel;
    std::function<long long(ComplierParser::EqualityExpressionContext*)> evalEq;
    std::function<long long(ComplierParser::AndExpressionContext*)> evalBitAnd;
    std::function<long long(ComplierParser::ExclusiveOrExpressionContext*)> evalBitXor;
    std::function<long long(ComplierParser::InclusiveOrExpressionContext*)> evalBitOr;
    std::function<long long(ComplierParser::LogicalAndExpressionContext*)> evalLogAnd;
    std::function<long long(ComplierParser::LogicalOrExpressionContext*)> evalLogOr;
    std::function<long long(ComplierParser::ExpressionContext*)> evalExpr;
    std::function<long long(ComplierParser::ConditionalExpressionContext*)> evalCond;
    std::function<long long(ComplierParser::AssignmentExpressionContext*)> evalAssign;

    auto truth = [](long long v) -> long long { return v != 0 ? 1LL : 0LL; };

    evalPrimary = [this, &evalExpr](ComplierParser::PrimaryExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->Identifier())
        {
            // 常量表达式不允许普通标识符（尚未实现宏常量等）
            throw error::invalid_constant;
        }
        if (ctx->Constant())
        {
            const std::string t = ctx->Constant()->getText();
            if (isCharacterConstant(t))
                return parseCharacterConstant(t);
            if (isIntegerConstant(t))
                return parseIntegerConstant(t);
            throw error::invalid_constant;
        }
        if (ctx->expression())
        {
            // 括号表达式
            return evalExpr(ctx->expression());
        }
        // 其他扩展（如__builtin等）不支持
        throw error::invalid_constant;
    };

    evalPostfix = [&evalPrimary](ComplierParser::PostfixExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 仅接受“纯 primary”的情况；带 [], (), ., ->, ++/-- 等均不支持
        if (ctx->primaryExpression() && ctx->children.size() == 1)
            return evalPrimary(ctx->primaryExpression());
        // GNU 扩展、聚合初始化等均不在常量表达式支持范围
        throw error::invalid_constant;
    };

    evalCast = [&evalCast, &evalUnary](ComplierParser::CastExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 若为显式类型转换 '(' typeName ')' castExpression -> 忽略类型，直接求右侧值
        if (ctx->castExpression())
            return evalCast(ctx->castExpression());
        // 其余分支：unary 或 DigitSequence（for 场景）
        if (ctx->unaryExpression())
            return evalUnary(ctx->unaryExpression());
        if (ctx->DigitSequence())
        {
            // 纯数字序列，十进制
            return std::stoll(ctx->DigitSequence()->getText(), nullptr, 10);
        }
        throw error::invalid_constant;
    };

    evalUnary = [&evalPostfix, &evalCast](ComplierParser::UnaryExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->postfixExpression())
            return evalPostfix(ctx->postfixExpression());

        // 处理一元运算符：+ - ~ !
        if (ctx->unaryOperator() && ctx->castExpression())
        {
            const std::string op = ctx->unaryOperator()->getText();
            long long v = evalCast(ctx->castExpression());
            if (op == "+")
                return +v;
            if (op == "-")
                return -v;
            if (op == "~")
                return ~v;
            if (op == "!")
                return (v == 0) ? 1LL : 0LL;
            throw error::invalid_constant;
        }

        // sizeof/_Alignof等未实现
        throw error::invalid_constant;
    };

    auto evalBinaryByChildren =
        [](antlr4::ParserRuleContext* ctx, auto evalLhs,
           const std::function<long long(antlr4::tree::ParseTree*)>& evalRhs,
           const std::function<long long(long long, const std::string&, long long)>& apply)
        -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        const auto& kids = ctx->children;
        if (kids.empty())
            throw error::invalid_constant;

        // 第一个子节点是一个子表达式上下文
        long long acc = evalLhs(kids[0]);
        // 之后按 [op, rhs, op, rhs, ...] 交替
        for (size_t i = 1; i + 1 < kids.size(); i += 2)
        {
            std::string op = kids[i]->getText();
            long long rhs = evalRhs(kids[i + 1]);
            acc = apply(acc, op, rhs);
        }
        return acc;
    };

    // 以下 evalXxx 采用 children 轮询的通用策略，避免依赖 ANTLR 生成的 vector API 差异
    evalMul = [&evalBinaryByChildren,
               &evalCast](ComplierParser::MultiplicativeExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalCast](antlr4::tree::ParseTree* n)
        { return evalCast(dynamic_cast<ComplierParser::CastExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "*")
                return a * b;
            if (op == "/")
            {
                if (b == 0)
                    throw error::invalid_constant;
                return a / b;
            }
            if (op == "%")
            {
                if (b == 0)
                    throw error::invalid_constant;
                return a % b;
            }
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalAdd = [&evalBinaryByChildren,
               &evalMul](ComplierParser::AdditiveExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalMul](antlr4::tree::ParseTree* n)
        { return evalMul(dynamic_cast<ComplierParser::MultiplicativeExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "+")
                return a + b;
            if (op == "-")
                return a - b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalShift = [&evalBinaryByChildren,
                 &evalAdd](ComplierParser::ShiftExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalAdd](antlr4::tree::ParseTree* n)
        { return evalAdd(dynamic_cast<ComplierParser::AdditiveExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (b < 0)
                throw error::invalid_constant;
            if (op == "<<")
                return a << b;
            if (op == ">>")
                return a >> b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalRel = [&evalBinaryByChildren, &evalShift,
               &truth](ComplierParser::RelationalExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalShift](antlr4::tree::ParseTree* n)
        { return evalShift(dynamic_cast<ComplierParser::ShiftExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "<")
                return truth(a < b);
            if (op == "<=")
                return truth(a <= b);
            if (op == ">")
                return truth(a > b);
            if (op == ">=")
                return truth(a >= b);
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalEq = [&evalBinaryByChildren, &evalRel,
              &truth](ComplierParser::EqualityExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalRel](antlr4::tree::ParseTree* n)
        { return evalRel(dynamic_cast<ComplierParser::RelationalExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "==")
                return truth(a == b);
            if (op == "!=")
                return truth(a != b);
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitAnd = [&evalBinaryByChildren,
                  &evalEq](ComplierParser::AndExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalEq](antlr4::tree::ParseTree* n)
        { return evalEq(dynamic_cast<ComplierParser::EqualityExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "&")
                return a & b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitXor = [&evalBinaryByChildren,
                  &evalBitAnd](ComplierParser::ExclusiveOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitAnd](antlr4::tree::ParseTree* n)
        { return evalBitAnd(dynamic_cast<ComplierParser::AndExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "^")
                return a ^ b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalBitOr = [&evalBinaryByChildren,
                 &evalBitXor](ComplierParser::InclusiveOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitXor](antlr4::tree::ParseTree* n)
        { return evalBitXor(dynamic_cast<ComplierParser::ExclusiveOrExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "|")
                return a | b;
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalLogAnd = [&evalBinaryByChildren, &evalBitOr,
                  &truth](ComplierParser::LogicalAndExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalBitOr](antlr4::tree::ParseTree* n)
        { return evalBitOr(dynamic_cast<ComplierParser::InclusiveOrExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "&&")
                return truth(truth(a) && truth(b));
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalLogOr = [&evalBinaryByChildren, &evalLogAnd,
                 &truth](ComplierParser::LogicalOrExpressionContext* ctx) -> long long
    {
        auto evalL = [&evalLogAnd](antlr4::tree::ParseTree* n)
        { return evalLogAnd(dynamic_cast<ComplierParser::LogicalAndExpressionContext*>(n)); };
        auto evalR = evalL;
        auto apply = [&truth](long long a, const std::string& op, long long b) -> long long
        {
            if (op == "||")
                return truth(truth(a) || truth(b));
            throw error::invalid_constant;
        };
        return evalBinaryByChildren(ctx, evalL, evalR, apply);
    };

    evalExpr = [this](ComplierParser::ExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        // 逗号表达式的值为最后一个 assignmentExpression
        long long val = 0;
        for (auto* ae : ctx->assignmentExpression())
        {
            // 递归调用 parseConstexpr 以复用逻辑
            val = this->parseConstexpr(ae);
        }
        return val;
    };

    evalCond = [&evalLogOr, &evalExpr, &evalCond,
                &truth](ComplierParser::ConditionalExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        long long c = evalLogOr(ctx->logicalOrExpression());
        if (ctx->expression() && ctx->conditionalExpression())
        {
            if (truth(c))
                return evalExpr(ctx->expression());
            else
                return evalCond(ctx->conditionalExpression());
        }
        return c;
    };

    evalAssign = [&evalCond](ComplierParser::AssignmentExpressionContext* ctx) -> long long
    {
        if (!ctx)
            throw error::invalid_constant;
        if (ctx->conditionalExpression())
            return evalCond(ctx->conditionalExpression());
        // 赋值类表达式不属于常量表达式
        throw error::invalid_constant;
    };

    return evalAssign(expr);
};
// [TODO] addDeclarations 由 visitDeclartion -> lowerdecl完成 该函数废弃
// [[deprecated("addDeclarations 由 visitDeclartion -> lowerdecl完成 该函数废弃")]]
// std::vector<Type> astVisitor::addDeclarations(std::vector<Type> vars)
// {
//     for (auto& each : vars)
//     {
//         auto storage = StorageClassSpecifier::VarDef;
//         if (each.storageClassSpecifier != StorageClassSpecifier::VarDef)
//         {
//             storage = each.storageClassSpecifier;
//             if (storage == StorageClassSpecifier::Typedef)
//             {
//                 auto tp = each;
//                 IDdef def;
//                 def.name = each.id;
//                 def.type = tp.popTop();
//                 def.storageClassSpecifier = StorageClassSpecifier::Typedef;
//                 record_ID_decl(def.name, def.type, def.storageClassSpecifier, nullptr);
//                 continue;
//             }
//         }
//         IDdef* func_ctx = (funcnow != gfuncptr) ? funcnow : nullptr;
//         record_ID_decl(each.id, each, storage, func_ctx);
//     }
//     return vars;
// }
astVisitor::astVisitor(std::string name, OBJ& ob) : obj(ob)
{
    const std::string init_name = "__global_init" + name;
    IDdef global_init_fun;
    global_init_fun.name = init_name;
    global_init_fun.type = Type{Type::Kind::Function, std::vector<IDdef>{}};
    global_init_fun.type.pushTop(Type{Type::Kind::Basic, Type::BasicType::Void});
    record_ID_decl(global_init_fun, nullptr);
    gfuncptr = funcnow = lookup_ID_decl(init_name);
}
void astVisitor::visitAsmADDer(ComplierParser::AsmADDerContext* ctx)
{
    auto str = ctx->StringLiteral()->getText();
    std::string text = ctx->StringLiteral()->getText();
    std::string content;
    size_t first_quote = text.find('"');
    size_t last_quote = text.rfind('"');
    if (first_quote != std::string::npos && last_quote != std::string::npos &&
        last_quote > first_quote)
    {
        std::string raw = text.substr(first_quote + 1, last_quote - first_quote - 1);
        content.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i)
        {
            char c = raw[i];
            if (c == '\\' && i + 1 < raw.size())
            {
                char esc = raw[++i];
                if (esc == 'n')
                    content.push_back('\n');
                else if (esc == 't')
                    content.push_back('\t');
                else if (esc == 'r')
                    content.push_back('\r');
                else if (esc == '\\')
                    content.push_back('\\');
                else if (esc == '\'')
                    content.push_back('\'');
                else if (esc == '\"')
                    content.push_back('\"');
                else if (esc == '0')
                    content.push_back('\0');
                else if (esc == 'x') // hex escape: \xhh...
                {
                    int val = 0;
                    int cnt = 0;
                    while (i + 1 < raw.size() &&
                           std::isxdigit(static_cast<unsigned char>(raw[i + 1])) && cnt < 2)
                    {
                        ++i;
                        char hx = raw[i];
                        val = val * 16 +
                              (std::isdigit(static_cast<unsigned char>(hx))
                                   ? hx - '0'
                                   : (std::toupper(static_cast<unsigned char>(hx)) - 'A' + 10));
                        ++cnt;
                    }
                    content.push_back(static_cast<char>(val));
                }
                else if (esc >= '0' && esc <= '7') // octal \nnn
                {
                    int val = esc - '0';
                    int cnt = 1;
                    while (i + 1 < raw.size() && raw[i + 1] >= '0' && raw[i + 1] <= '7' && cnt < 3)
                    {
                        ++i;
                        val = val * 8 + (raw[i] - '0');
                        ++cnt;
                    }
                    content.push_back(static_cast<char>(val));
                }
                else
                {
                    // unknown escape, keep the character itself
                    content.push_back(esc);
                }
            }
            else
            {
                content.push_back(c);
            }
        }
    }
    else
    {
        // Fallback: no surrounding quotes found, use the raw token
        content = text;
    }
    funcnow->funcInfo.asms.push_back(content);
    return;
}
// void astVisitor::visitByTypeIndex(antlr4::ParserRuleContext* ctx)
// {
//     switch (ctx->getRuleIndex())
//     {
//     case ComplierParser::RuleCompilationUnit:
//         return visitCompilationUnit(
//             dynamic_cast<ComplierParser::ComplierParser::CompilationUnitContext*>(ctx));
//         break;
//     case ComplierParser::RuleTranslationUnit:
//         return visitTranslationUnit(
//             dynamic_cast<ComplierParser::ComplierParser::TranslationUnitContext*>(ctx));
//         break;
//     case ComplierParser::RuleExternalDeclaration:
//         return visitExternalDeclaration(
//             dynamic_cast<ComplierParser::ExternalDeclarationContext*>(ctx));
//         break;
//     case ComplierParser::RuleDeclaration:
//         return visitDeclaration(
//             dynamic_cast<ComplierParser::ComplierParser::DeclarationContext*>(ctx));
//         break;
//     case ComplierParser::RuleFunctionDefinition:
//         return visitFunctionDefinition(
//             dynamic_cast<ComplierParser::FunctionDefinitionContext*>(ctx));
//         break;
//     case ComplierParser::RuleDeclarator:
//         // 处理声明符，包括数组和函数指针等
//         // 这里暂不实现
//         break;
//     case ComplierParser::RuleDirectDeclarator:
//         // 处理直接声明符，包括数组维度等
//         // 这里暂不实现
//         break;
//     // 更多类型的处理...
//     default:
//         // 对于未明确处理的节点类型，返回默认值
//     }

//     // 默认返回成功
// }
// 处理整型常量，支持十进制、十六进制、八进制格式
int astVisitor::parseIntegerConstant(const std::string& text)
{
    try
    {
        // 检查十六进制格式 (0x 或 0X 前缀)
        if (text.size() > 2 && (text.substr(0, 2) == "0x" || text.substr(0, 2) == "0X"))
        {
            return std::stoi(text, nullptr, 16);
        }
        // 检查八进制格式 (0 前缀)
        else if (text.size() > 0 && text[0] == '0')
        {
            return std::stoi(text, nullptr, 8);
        }
        // 默认十进制
        else
        {
            return std::stoi(text, nullptr, 10);
        }
    }
    catch (const std::exception& e)
    {
        throw error::invalid_constant;
    }
}

// 处理字符常量，支持转义序列
int astVisitor::parseCharacterConstant(const std::string& text)
{
    // 移除前后的单引号
    std::string content = text.substr(1, text.size() - 2);

    // 处理转义序列
    if (content.size() > 0 && content[0] == '\\')
    {
        if (content.size() < 2)
        {
            throw error::invalid_constant;
        }

        switch (content[1])
        {
        case 'n':
            return '\n'; // 换行
        case 't':
            return '\t'; // 制表符
        case 'r':
            return '\r'; // 回车
        case '0':
            return '\0'; // 空字符
        case '\\':
            return '\\'; // 反斜杠
        case '\'':
            return '\''; // 单引号
        case '\"':
            return '\"'; // 双引号
        case 'x':
        { // 十六进制表示 \xhh
            if (content.size() < 4)
            {
                throw error::invalid_constant;
            }
            std::string hexValue = content.substr(2, 2);
            try
            {
                return std::stoi(hexValue, nullptr, 16);
            }
            catch (...)
            {
                throw error::invalid_constant;
            }
        }
        default:
            throw error::invalid_constant;
        }
    }
    else if (content.size() == 1)
    {
        // 普通字符
        return static_cast<int>(content[0]);
    }

    throw error::invalid_constant;
}

// 检查是否为整型常量
bool astVisitor::isIntegerConstant(const std::string& text)
{
    // 判断首字符是否为数字或负号
    if (text.empty() || (text[0] != '-' && text[0] != '+' && !std::isdigit(text[0])))
    {
        return false;
    }

    // 检查是否包含单引号（字符常量特征）
    if (text[0] == '\'' || text.find('\'') != std::string::npos)
    {
        return false;
    }
    return true;
}

// 检查是否为字符常量
bool astVisitor::isCharacterConstant(const std::string& text)
{
    // 字符常量格式: 'x' 或 '\x'
    return text.size() >= 3 && text[0] == '\'' && text[text.size() - 1] == '\'';
}

bool astVisitor::stackTopIsLvalue()
{
    return funcnow->funcInfo.asms.back() == "LC" || funcnow->funcInfo.asms.back() == "LI" ||
           funcnow->funcInfo.asms.back() == "LW" ||
           funcnow->funcInfo.asms.back().starts_with("LODS");
}

bool astVisitor::madeTopIsLvalueAddr()
{
    if (stackTopIsLvalue())
    {
        if (funcnow->funcInfo.asms.back() == "LODS") // 栈顶还有lods的参数
        {
            funcnow->funcInfo.asms.pop_back();
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        }
        else
        {
            funcnow->funcInfo.asms.pop_back();
        }
        return true;
    }
    return false;
}

Type astVisitor::load_var_or_func(std::string name)
{
    if (auto* id = lookup_ID_decl(name))
    {
        if (id->type.kind == Type::Kind::Function)
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, "func@" + name});
            return id->type;
        }

        const bool is_global = id->kind == IDdef::Kind::Global ||
                               id->storageClassSpecifier == StorageClassSpecifier::Static;

        if (is_global)
        {
            const auto label = global_label(*id);
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, label});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEAD});
        }
        else
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, id->addr});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEA});
        }
        return loadStackTopAddrByType(id->type);
    }

    THROW_ERR_NOCTX(error::undifined_id);
}

Type astVisitor::loadStackTopAddrByType(Type type)
{
    if (type.kind == Type::Kind::Struct)
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LODS, type.getsize()});
        return type;
    }
    if (type.kind == Type::Kind::Array)
    {
        Type tp;
        tp.kind = Type::Kind::Pointer;
        tp.subType = type.subType;
        return tp;
    }
    loadStackTopAddrBySize(type.getsize());
    return type;
}

void astVisitor::saveStackTopAddrValueByType(Type type)
{
    if (type.kind == Type::Kind::Struct)
    {
        if (!funcnow->funcInfo.asms.back().starts_with("LODS"))
        {
            THROW_ERR_NOCTX(error::unsurpported_op);
        }
        funcnow->funcInfo.asms.pop_back();
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MOVS, type.getsize()});
        return;
    }
    if (type.kind == Type::Kind::Array)
    {
        Type tp;
        tp.kind = Type::Kind::Pointer;
        tp.subType = type.subType;
        SaveStackTopValueToAddr(tp.getsize());
        return;
    }
    SaveStackTopValueToAddr(type.getsize());
}

void astVisitor::loadStackTopAddrBySize(size_t size)
{
    if (size != 1 && size != 4 && size != 8)
    {
        THROW_ERR_NOCTX(error::expected_aligned_addr);
    }
    if (size == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LC});
    }
    else if (size == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LI});
    }
    else if (size == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LW});
    }
}

void astVisitor::SaveStackTopValueToAddr(size_t size)
{
    if (size != 1 && size != 4 && size != 8)
    {
        THROW_ERR_NOCTX(error::expected_aligned_addr);
    }
    if (size == Type{Type::Kind::Basic, Type::BasicType::Char}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SC});
    }
    else if (size == Type{Type::Kind::Basic, Type::BasicType::Int}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SI});
    }
    else if (size == Type{Type::Kind::Basic, Type::BasicType::Long}.getsize())
    {
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SW});
    }
}
