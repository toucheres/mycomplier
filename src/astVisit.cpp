#include "astVisit.h"
#include "ASM.hpp"
#include "CLexer.h"
#include "CParser.h"
#include "CParserBaseVisitor.h"
#include "error.hpp"
#include "tools.hpp"
#include "type_utils.hpp"
#include "vm.h"
#include <algorithm>
#include <format>
#include <functional>
#include <tree/TerminalNode.h>
#include <utility>
template <class T, class IN>
    requires requires(IN in) { dynamic_cast<T>(in); }
auto dc(IN&& in)
{
    return dynamic_cast<T>(in);
}
void astVisitor::visitCompilationUnit(CParser::CompilationUnitContext* ctx)
{
    // ctx->getRuleIndex() == CParser::RuleCompilationUnit;
    if (!ctx)
        return;
    auto tu = ctx->translationUnit();
    if (!tu)
        return;
    return visitTranslationUnit(tu);
}
void astVisitor::visitTranslationUnit(CParser::TranslationUnitContext* ctx)
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
        const std::size_t base_arg_offset = 2 * VCPU::size_word;
        std::size_t offset = base_arg_offset;
        for (std::size_t i = 0; i < *arg_index; ++i)
        {
            offset += align_up(func_ctx->type.args[i].type.getsize(), VCPU::size_word);
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
std::vector<IDdef> astVisitor::lowerDeclaration(CParser::DeclarationSpecifiersContext* specs,
                                                CParser::InitDeclaratorListContext* initList)
{
    Type basetype;
    StorageClassSpecifier storageClassType = StorageClassSpecifier::VarDef;
    std::vector<IDdef> vars;
    if (specs) // 前类型
    {
        auto [type, storageClassSpecifier, typeQualifiers] = visitDeclarationSpecifiers(specs);
        if (!type)
        {
            THROW_ERR(error::expected_type, specs);
        }
        else
        {
            basetype = *type;
        }
        for (auto each : typeQualifiers)
        {
            basetype.typeQualifiers[each] = true;
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

    auto decode_normal = [&](const std::string& s, size_t k, std::string& out,
                             size_t& newpos) -> bool
    {
        // assume s[k] == '"'
        size_t i = k + 1;
        while (i < s.size())
        {
            char c = s[i];
            if (c == '"')
            {
                newpos = i + 1;
                return true;
            }
            if (c == '\\' && i + 1 < s.size())
            {
                char esc = s[i + 1];
                switch (esc)
                {
                case 'n':
                    out.push_back('\n');
                    i += 2;
                    break;
                case 't':
                    out.push_back('\t');
                    i += 2;
                    break;
                case 'r':
                    out.push_back('\r');
                    i += 2;
                    break;
                case '\\':
                    out.push_back('\\');
                    i += 2;
                    break;
                case '\'':
                    out.push_back('\'');
                    i += 2;
                    break;
                case '"':
                    out.push_back('"');
                    i += 2;
                    break;
                case 'a':
                    out.push_back('\a');
                    i += 2;
                    break;
                case 'b':
                    out.push_back('\b');
                    i += 2;
                    break;
                case 'f':
                    out.push_back('\f');
                    i += 2;
                    break;
                case 'v':
                    out.push_back('\v');
                    i += 2;
                    break;
                case '?':
                    out.push_back('?');
                    i += 2;
                    break;
                case 'x':
                {
                    i += 2;
                    int val = 0;
                    bool any = false;
                    while (i < s.size() && std::isxdigit((unsigned char)s[i]))
                    {
                        val = val * 16 +
                              (std::isdigit(s[i])
                                   ? s[i] - '0'
                                   : (std::islower(s[i]) ? s[i] - 'a' + 10 : s[i] - 'A' + 10));
                        i++;
                        any = true;
                    }
                    if (any)
                        out.push_back(static_cast<char>(val));
                    break;
                }
                default:
                    if (esc >= '0' && esc <= '7')
                    {
                        int val = esc - '0';
                        i += 2;
                        int cnt = 1;
                        while (cnt < 3 && i < s.size() && s[i] >= '0' && s[i] <= '7')
                        {
                            val = val * 8 + (s[i] - '0');
                            i++;
                            cnt++;
                        }
                        out.push_back(static_cast<char>(val));
                    }
                    else
                    {
                        out.push_back(esc);
                        i += 2;
                    }
                }
            }
            else
            {
                out.push_back(c);
                i++;
            }
        }
        return false;
    };

    for (auto tok : toks)
    {
        std::string s = tok->getText();
        size_t p = 0;
        auto skip_space = [&](void)
        {
            while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
                p++;
        };

        while (p < s.size())
        {
            skip_space();
            if (p >= s.size())
                break;

            // parse optional encoding prefix: u8 | u | U | L
            size_t q = p;
            if (q + 1 < s.size() && s[q] == 'u' && s[q + 1] == '8')
                q += 2;
            else if (s[q] == 'u' || s[q] == 'U' || s[q] == 'L')
                q += 1;

            // raw string literal: R"delim(... )delim"
            if (q + 1 < s.size() && s[q] == 'R' && s[q + 1] == '"')
            {
                size_t delim_start = q + 2;
                size_t delim_end = s.find('(', delim_start);
                if (delim_end != std::string::npos)
                {
                    std::string delim = s.substr(delim_start, delim_end - delim_start);
                    size_t content_start = delim_end + 1;
                    std::string close = std::string(")") + delim + '"';
                    size_t close_pos = s.find(close, content_start);
                    if (close_pos != std::string::npos)
                    {
                        chars_after_transed.append(s, content_start, close_pos - content_start);
                        p = close_pos + close.size();
                        continue;
                    }
                }
                // malformed raw literal: bail out of this token
                break;
            }

            // normal string literal: '"..."'
            if (q < s.size() && s[q] == '"')
            {
                size_t newpos = 0;
                if (!decode_normal(s, q, chars_after_transed, newpos))
                    break;
                p = newpos;
                continue;
            }

            // unknown sequence -> avoid infinite loop
            break;
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
    CParser::AssignmentExpressionContext* expr)
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
    size_t pos = 0;
    std::vector<int> result;
    while (pos < text.size())
    {
        // skip whitespace between adjacent tokens
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
            pos++;
        if (pos >= text.size())
            break;
        std::string chunk;
        size_t start = pos;
        if (!decode_string_token_text(text, pos, chunk))
        {
            return std::nullopt;
        }
        for (unsigned char c : chunk)
            result.push_back(static_cast<int>(c));
    }
    if (result.empty())
        return std::nullopt;
    result.push_back(0);
    return result;
}

Type astVisitor::tryVisitType(std::function<Type()> expr)
{
    auto start = funcnow->funcInfo.asms.size();
    //[TODO] 目前回退了funasms, 应回退obj状态
    // auto objcopy = obj;
    Type rettype;
    rettype = expr();
    funcnow->funcInfo.asms.resize(start);
    return rettype;
}

std::vector<IDdef> astVisitor::visitDeclaration(CParser::DeclarationContext* ctx)
{
    return lowerDeclaration(ctx->declarationSpecifiers(), ctx->initDeclaratorList());
}
void astVisitor::visitFunctionDefinition(CParser::FunctionDefinitionContext* ctx)
{
    IDdef functionType = visitDeclarator(ctx->declarator());
    StorageClassSpecifier storageClassSpecifier = StorageClassSpecifier::VarDef;
    if (ctx->declarationSpecifiers()) // 返回类型
    {
        auto [rettype, storageClass, typeQualifiers] =
            visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        if (!rettype)
            THROW_ERR(error::expected_type, ctx);
        if (typeQualifiers.size())
        {
            THROW_ERR(error::unexpected_typeQualifiers, ctx);
        }
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
    if (ctx->functionBody() && ctx->functionBody()->compoundStatement())
        visitCompoundStatement(ctx->functionBody()->compoundStatement());
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
            align_up(funcnow->funcInfo.max_stack_size, VCPU::size_word) / VCPU::size_word};
    obj.exit_decl_scope();
    funcnow = gfunptr_local;
}
void astVisitor::visitExternalDeclaration(CParser::ExternalDeclarationContext* ctx)
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
std::tuple<std::optional<Type>, std::optional<StorageClassSpecifier>,
           std::vector<Type::TypeQualifier>>
astVisitor::visitDeclarationSpecifiers(CParser::DeclarationSpecifiersContext* ctx)
{
    std::optional<Type> type;
    std::optional<StorageClassSpecifier> storageC;
    std::vector<Type::TypeQualifier> typeQualifiers;
    // 遍历所有声明说明符
    for (auto& each : ctx->declarationSpecifier())
    {
        // 解析每个声明说明符
        // [TODO] functionSpecifier alignmentSpecifier
        std::variant<Type, Type::StorageClassSpecifier, Type::TypeQualifier> result =
            visitDeclarationSpecifier(each);
        if (result.index() == 0) // Type
        {
            if (type)
            {
                THROW_ERR(error::double_type, ctx);
            }
            type = std::get<0>(result);
        }
        else if (result.index() == 1) // StorageClassSpecifier
        {
            if (storageC)
            {
                THROW_ERR(error::double_StorageClassSpecifier, ctx);
            }
            storageC = std::get<1>(result);
        }
        else if (result.index() == 2) // TypeQualifier
        {
            typeQualifiers.push_back(std::get<2>(result));
        }
    }
    return {type, storageC, typeQualifiers};
}
std::variant<Type, Type::StorageClassSpecifier, Type::TypeQualifier> astVisitor::
    visitDeclarationSpecifier(CParser::DeclarationSpecifierContext* ctx)
{
    // 检查是否为类型说明符
    if (ctx->typeSpecifier())
    {
        // 直接返回typeSpecifier的结果
        return visitTypeSpecifier(ctx->typeSpecifier());
    }
    else if (ctx->storageClassSpecifier()) // typedef / extern
    {
        if (ctx->getText() == "typedef")
        {
            return StorageClassSpecifier::Typedef;
        }
        else if (ctx->getText() == "static")
        {
            return StorageClassSpecifier::Static;
        }
        else if (ctx->getText() == "extern")
        {
            return StorageClassSpecifier::Extern;
        }
        else
        {
            THROW_ERR(error::unsurpport_StorageClassSpecifier, ctx);
        }
    }
    else if (ctx->typeQualifier())
    {
        if (ctx->getText() == "const")
        {
            return Type::TypeQualifier::Const;
        }
        else if (ctx->getText() == "_Atomic")
        {
            return Type::TypeQualifier::Atomic;
        }
        else if (ctx->getText() == "volatile")
        {
            return Type::TypeQualifier::Volatile;
        }
        else if (ctx->getText() == "restrict")
        {
            return Type::TypeQualifier::Restrict;
        }
        else
        {
            THROW_ERR(error::unsurpport_StorageClassSpecifier, ctx);
        }
    }
    THROW_ERR(error::unsurpport_DeclarationSpecifier, ctx);
}
Type astVisitor::visitTypeSpecifier(CParser::TypeSpecifierContext* ctx)
{
    Type rettype;
    if (ctx->typeofSpecifier()) // typeof(xxx)
    {
        return visitTypeofSpecifier(ctx->typeofSpecifier());
    }
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
            if (ctx->structOrUnionSpecifier()->memberDeclarationList()) // 同时定义struct
            {
                IDdef thisStruct;
                thisStruct.storageClassSpecifier = Type::StorageClassSpecifier::StructDef;
                static long structIndex = 0;
                nonameID = thisStruct.name = thisStruct.type.structInfo.name =
                    ctx->structOrUnionSpecifier()->Identifier()
                        ? ctx->structOrUnionSpecifier()->Identifier()->getText()
                        : "__nuname_struct_" + std::to_string(structIndex++);
                thisStruct.type.kind = Type::Kind::Struct;
                thisStruct.type.structInfo.members = visitMemberDeclarationList(
                    ctx->structOrUnionSpecifier()->memberDeclarationList());
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
    if (ctx->enumSpecifier()) // 是枚举类型
    {
        std::string enumName;
        static long enumIndex = 0;

        if (ctx->enumSpecifier()->enumeratorList()) // 同时定义enum
        {
            IDdef thisEnum;
            thisEnum.storageClassSpecifier = StorageClassSpecifier::EnumDef;
            enumName = thisEnum.name = ctx->enumSpecifier()->Identifier()
                                           ? ctx->enumSpecifier()->Identifier()->getText()
                                           : "__unnamed_enum_" + std::to_string(enumIndex++);
            thisEnum.type.kind = Type::Kind::Basic;
            thisEnum.type.basic_type = Type::BasicType::Int; // enum底层类型为int
            record_ID_decl(thisEnum.name, thisEnum.type, StorageClassSpecifier::EnumDef, nullptr);

            // 处理枚举常量列表
            long long enumValue = 0;
            for (auto* enumerator : ctx->enumSpecifier()->enumeratorList()->enumerator())
            {
                std::string constName = enumerator->enumerationConstant()->Identifier()->getText();

                // 如果有赋值表达式，计算值
                if (enumerator->constantExpression())
                {
                    auto tesxts = enumerator->getText();
                    enumValue =
                        tryVisitType(
                            [this, enumerator]
                            {
                                return visitConditionalExpression(
                                    enumerator->constantExpression()->conditionalExpression());
                            })
                            .constexprVal.value();
                }

                // 将枚举常量注册为整型常量
                IDdef enumConst;
                enumConst.name = constName;
                enumConst.type.kind = Type::Kind::Basic;
                enumConst.type.basic_type = Type::BasicType::Int;
                enumConst.addr = static_cast<int>(enumValue); // 使用addr存储枚举值
                enumConst.storageClassSpecifier = StorageClassSpecifier::EnumConst;
                enumConst.is_defined = true;
                record_ID_decl(enumConst.name, enumConst.type, StorageClassSpecifier::EnumConst,
                               nullptr);
                // 更新枚举值以便为下一个枚举成员使用
                obj.decls.find_ID_decl(constName, StorageClassSpecifier::EnumConst)->addr =
                    static_cast<int>(enumValue);

                enumValue++;
            }
        }
        else if (ctx->enumSpecifier()->Identifier())
        {
            enumName = ctx->enumSpecifier()->Identifier()->getText();
        }

        // enum类型返回int
        rettype.kind = Type::Kind::Basic;
        rettype.basic_type = Type::BasicType::Int;
        return rettype;
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

std::vector<IDdef> astVisitor::visitInitDeclaratorList(CParser::InitDeclaratorListContext* ctx,
                                                       Type basetype,
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
IDdef astVisitor::visitInitDeclarator(CParser::InitDeclaratorContext* ctx, Type basetype,
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
    std::function<bool(Type, CParser::InitializerContext*)> func =
        [&func, this, ctx](Type arg, CParser::InitializerContext* init) -> bool
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
IDdef astVisitor::visitDeclarator(CParser::DeclaratorContext* ctx)
{
    IDdef var = visitDirectDeclarator(ctx->directDeclarator());
    if (!ctx->pointer().empty()) // 有ptr
    {
        int ptr_count = 0;
        for (auto p : ctx->pointer())
        {
            auto str = p->getText();
            ptr_count += static_cast<int>(std::count(str.begin(), str.end(), '*'));
        }
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
IDdef astVisitor::visitDirectDeclarator(CParser::DirectDeclaratorContext* ctx)
{
    IDdef var;
    if (ctx->Identifier() && ctx->DigitSequence()) // 位域
    {
        return var;
    }

    // 基础部分: 标识符或括号括起的 declarator
    if (ctx->Identifier())
    {
        var.name = ctx->Identifier()->getText();
    }
    else if (ctx->declarator())
    {
        var = visitDeclarator(ctx->declarator());
    }
    else
    {
        THROW_ERR(error::unsurpport_directDeclarator, ctx);
    }

    bool in_array = false;
    CParser::AssignmentExpressionContext* array_expr = nullptr;

    for (auto* child : ctx->children)
    {
        if (auto* param = dynamic_cast<CParser::ParameterTypeListContext*>(child))
        {
            auto args = visitParameterTypeList(param);
            var.type.pushTop(Type{Type::Kind::Function, args});
        }
        else if (auto* term = dynamic_cast<antlr4::tree::TerminalNode*>(child))
        {
            auto token_type = term->getSymbol()->getType();
            if (token_type == CLexer::LeftBracket)
            {
                in_array = true;
                array_expr = nullptr;
            }
            else if (token_type == CLexer::RightBracket && in_array)
            {
                int sz = 0;
                if (array_expr)
                {
                    auto ret = tryVisitType([this, array_expr]
                                            { return visitAssignmentExpression(array_expr); })
                                   .constexprVal;
                    if (!ret)
                    {
                        THROW_ERR(error::expected_constexpr, ctx);
                    }
                    sz = *ret;
                }
                var.type.pushTop(Type(Type::Kind::Array, sz));
                in_array = false;
            }
        }
        else if (in_array)
        {
            if (auto* ae = dynamic_cast<CParser::AssignmentExpressionContext*>(child))
            {
                array_expr = ae;
            }
        }
    }

    return var;
}
std::vector<IDdef> astVisitor::visitParameterTypeList(CParser::ParameterTypeListContext* ctx)
{
    return visitParameterList(ctx->parameterList());
}
void astVisitor::visitCompoundStatement(CParser::CompoundStatementContext* ctx)
{
    if (auto ptr = ctx->blockItemList())
    {
        obj.enter_decl_scope("compound");
        visitBlockItemList(ctx->blockItemList());
        obj.exit_decl_scope();
    }
}
std::vector<IDdef> astVisitor::visitParameterList(CParser::ParameterListContext* ctx)
{
    std::vector<IDdef> vars;
    for (auto& each : ctx->parameterDeclaration())
    {
        vars.push_back(visitParameterDeclaration(each));
    }
    return vars;
}
IDdef astVisitor::visitParameterDeclaration(CParser::ParameterDeclarationContext* ctx)
{
    Type basetype;
    if (ctx->declarationSpecifiers()) // 前类型
    {
        auto [type, storageClassSpecifier, typeQualifiers] =
            visitDeclarationSpecifiers(ctx->declarationSpecifiers());
        if (!type)
        {
            THROW_ERR(error::expected_type, ctx->declarationSpecifiers());
        }
        if (storageClassSpecifier)
        {
            THROW_ERR(error ::unexpected_storageClassSpecifier, ctx->declarationSpecifiers());
        }
        if (typeQualifiers.size())
        {
            for (auto each : typeQualifiers)
            {
                (*type).typeQualifiers[each] = true;
            }
        }
        basetype = *type;
    }
    // declarationSpecifiers2 removed in updated grammar; handled via declarationSpecifiers
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
        iddef.name = std::format("__noname_para_{}", noname_para_index++);
        return iddef;
    }
    else
    {
        IDdef tp;
        tp.type = basetype;
        tp.name = std::format("__noname_para_{}", noname_para_index++);
        return tp;
    }
    throw;
}
void astVisitor::visitBlockItemList(CParser::BlockItemListContext* ctx)
{
    for (auto each : ctx->blockItem())
    {
        visitBlockItem(each);
    }
}
void astVisitor::visitStatement(CParser::StatementContext* ctx)
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
void astVisitor::visitExpressionStatement(CParser::ExpressionStatementContext* ctx)
{
    if (ctx->expression())
    {
        (void)(visitExpression(ctx->expression()));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
    }
}
Type astVisitor::visitExpression(CParser::ExpressionContext* ctx)
{
    for (int i = 0; i < ctx->assignmentExpression().size(); i++)
    {
        auto ret = visitAssignmentExpression(ctx->assignmentExpression()[i]);
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
Type astVisitor::visitAssignmentExpression(CParser::AssignmentExpressionContext* ctx)
{
    // [TODO] DigitSequence
    if (ctx->conditionalExpression())
    {
        return visitConditionalExpression(ctx->conditionalExpression());
    }
    else if (ctx->assignementOperator)
    {
        auto uret = visitUnaryExpression(ctx->unaryExpression());
        if (!stackTopIsLvalue()) // 不是左值
        {
            THROW_ERR(error::expected_lvalue, ctx->unaryExpression());
        }
        madeTopIsLvalueAddr();
        if (ctx->assignementOperator->getText() == "=")
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

            if (ctx->assignementOperator->getText() == "+=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            }
            else if (ctx->assignementOperator->getText() == "-=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            }
            else if (ctx->assignementOperator->getText() == "*=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            }
            else if (ctx->assignementOperator->getText() == "/=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::DIV});
            }
            else if (ctx->assignementOperator->getText() == "%=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MOD});
            }
            else if (ctx->assignementOperator->getText() == "<<=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LSHIFT});
            }
            else if (ctx->assignementOperator->getText() == ">>=")
            {
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RSHIFT});
            }
            else if (ctx->assignementOperator->getText() == "^=")
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
Type astVisitor::visitConditionalExpression(CParser::ConditionalExpressionContext* ctx)
{
    auto contype = (visitLogicalOrExpression(ctx->logicalOrExpression()));
    if (ctx->expression() && ctx->conditionalExpression())
    {
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD");
        auto lrettype = visitExpression(ctx->expression());
        funcnow->funcInfo.asms[pos] =
            ASM{ASM::basic_asm::JZ,
                "thisfun@" + std::to_string(funcnow->funcInfo.asms.size() + 1)}; // 跳过JMP
        int pos2 = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD");
        auto rrettype = visitConditionalExpression(ctx->conditionalExpression());
        std::optional<long long> expr = std::nullopt;
        if (contype.constexprVal && lrettype.constexprVal && rrettype.constexprVal)
        {
            expr = *contype.constexprVal ? *lrettype.constexprVal : *rrettype.constexprVal;
        }
        funcnow->funcInfo.asms[pos2] =
            ASM{ASM::basic_asm::JMP, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        if (lrettype == rrettype)
        {
            Type tp = lrettype;
            tp.constexprVal = expr;
            return tp;
        }
        if (lrettype.kind != Type::Kind::Struct && rrettype.kind != Type::Kind::Struct &&
            lrettype.getsize() == rrettype.getsize())
        {
            Type tp{Type::Kind::Basic, Type::BasicType::Long};
            tp.constexprVal = expr;
            return tp;
        }
        THROW_ERR(error::expected_same_type, ctx);
    }
    else
    {
        return contype;
    }
    throw;
}
// DeclarationSpecifiers2 removed; use visitDeclarationSpecifiers for current grammar
Type astVisitor::visitLogicalOrExpression(CParser::LogicalOrExpressionContext* ctx)
{
    // 注意处理多个||
    std::function<Type(std::span<CParser::LogicalAndExpressionContext*>)> func =
        [&func, this, ctx](std::span<CParser::LogicalAndExpressionContext*> in) -> Type
    {
        if (in.size() == 1)
        {
            return visitLogicalAndExpression(in[0]);
        }
        auto lret = visitLogicalAndExpression(in[0]);
        // 保存当前位置，用于生成条件跳转指令
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY}); // 短路的jz/jnz会消耗栈顶
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms[pos] =
            ASM{ASM::basic_asm::JNZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        // 返回计算结果类型
        Type tp = rret;
        if (lret.constexprVal && rret.constexprVal)
        {
            tp.constexprVal = *lret.constexprVal || *rret.constexprVal;
        }
        return tp; // 语义上应该是整型，保持右值类型沿用
    };
    return func(std::span<CParser::LogicalAndExpressionContext*>(
        ctx->logicalAndExpression().data(), ctx->logicalAndExpression().size()));
}
Type astVisitor::visitLogicalAndExpression(CParser::LogicalAndExpressionContext* ctx)
{
    // 注意处理多个&&
    std::function<Type(std::span<CParser::InclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<CParser::InclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return visitInclusiveOrExpression(in[0]);
        }
        auto lret = visitInclusiveOrExpression(in[0]);
        // 保存当前位置，用于生成条件跳转指令
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY}); // 短路的jz/jnz会消耗栈顶
        int pos = funcnow->funcInfo.asms.size();
        funcnow->funcInfo.asms.push_back("HOLD"); // 占位，后面会替换为实际指令
        // 如果左操作数为true（非零），跳过右操作数的计算（短路）
        // JNZ指令：当栈顶值非零时跳转
        // 弹出左操作数结果，为右操作数腾出栈顶位置
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::POP});
        // 计算右侧表达式
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms[pos] =
            ASM{ASM::basic_asm::JZ, "thisfun@" + std::to_string(funcnow->funcInfo.asms.size())};
        // 返回计算结果类型
        Type tp = rret;
        if (lret.constexprVal && rret.constexprVal)
        {
            tp.constexprVal = *lret.constexprVal && *rret.constexprVal;
        }
        return tp;
    };
    return func(std::span<CParser::InclusiveOrExpressionContext*>(
        ctx->inclusiveOrExpression().data(), ctx->inclusiveOrExpression().size()));
}
Type astVisitor::visitInclusiveOrExpression(CParser::InclusiveOrExpressionContext* ctx)
{
    // 注意处理多个|
    std::function<Type(std::span<CParser::ExclusiveOrExpressionContext*>)> func =
        [&func, this, ctx](std::span<CParser::ExclusiveOrExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return visitExclusiveOrExpression(in[0]);
        }
        auto lret = visitExclusiveOrExpression(in[0]);
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::OR});
        // 返回计算结果类型
        Type tp = rret;
        if (lret.constexprVal && rret.constexprVal)
        {
            tp.constexprVal = *lret.constexprVal | *rret.constexprVal;
        }
        return tp;
    };
    return func(std::span<CParser::ExclusiveOrExpressionContext*>(
        ctx->exclusiveOrExpression().data(), ctx->exclusiveOrExpression().size()));
}
Type astVisitor::visitExclusiveOrExpression(CParser::ExclusiveOrExpressionContext* ctx)
{
    // 注意处理多个^
    std::function<Type(std::span<CParser::AndExpressionContext*>)> func =
        [&func, this, ctx](std::span<CParser::AndExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitAndExpression(in[0]));
        }
        auto lret = (visitAndExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        Type tp = rret;
        if (lret.constexprVal && rret.constexprVal)
        {
            tp.constexprVal = *lret.constexprVal ^ *rret.constexprVal;
        }
        return tp;
    };
    return func(std::span<CParser::AndExpressionContext*>(ctx->andExpression().data(),
                                                          ctx->andExpression().size()));
}
Type astVisitor::visitAndExpression(CParser::AndExpressionContext* ctx)
{
    // 注意处理多个&
    std::function<Type(std::span<CParser::EqualityExpressionContext*>)> func =
        [&func, this, ctx](std::span<CParser::EqualityExpressionContext*> in) -> Type

    {
        if (in.size() == 1)
        {
            return (visitEqualityExpression(in[0]));
        }
        auto lret = (visitEqualityExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1));
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::XOR});
        // 返回计算结果类型
        Type tp = rret;
        if (lret.constexprVal && rret.constexprVal)
        {
            tp.constexprVal = *lret.constexprVal & *rret.constexprVal;
        }
        return tp;
    };
    return func(std::span<CParser::EqualityExpressionContext*>(ctx->equalityExpression().data(),
                                                               ctx->equalityExpression().size()));
}
Type astVisitor::visitEqualityExpression(CParser::EqualityExpressionContext* ctx)
{
    // 注意处理多个!= / ==
    std::function<Type(std::span<CParser::RelationalExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<CParser::RelationalExpressionContext*> in, int index) -> Type

    {
        if (in.size() == 1)
        {
            return visitRelationalExpression(in[0]);
        }
        auto lret = visitRelationalExpression(in[0]);
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        Type tp = rret;
        if (ctx->children[index * 2 + 1]->getText() == "==")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CMP});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal == *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == "!=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CMPN});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal != *rret.constexprVal;
            }
        }
        // 返回计算结果类型
        return tp;
    };
    return func(std::span<CParser::RelationalExpressionContext*>(
                    ctx->relationalExpression().data(), ctx->relationalExpression().size()),
                0);
}
Type astVisitor::visitRelationalExpression(CParser::RelationalExpressionContext* ctx)
{
    // 注意处理多个< > <= >=
    std::function<Type(std::span<CParser::ShiftExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<CParser::ShiftExpressionContext*> in, int index) -> Type

    {
        if (in.size() == 1)
        {
            return visitShiftExpression(in[0]);
        }
        auto lret = visitShiftExpression(in[0]);
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        Type tp = rret;
        if (ctx->children[index * 2 + 1]->getText() == "<")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SMALL});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal < *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::BIG});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal > *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == "<=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SMALLE});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal <= *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">=")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::BIGE});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal >= *rret.constexprVal;
            }
        }
        // 返回计算结果类型
        return tp;
    };
    return func(std::span<CParser::ShiftExpressionContext*>(ctx->shiftExpression().data(),
                                                            ctx->shiftExpression().size()),
                0);
}
Type astVisitor::visitShiftExpression(CParser::ShiftExpressionContext* ctx)
{
    // 注意处理多个<< >>
    std::function<Type(std::span<CParser::AdditiveExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<CParser::AdditiveExpressionContext*> in, int index) -> Type

    {
        if (in.size() == 1)
        {
            return visitAdditiveExpression(in[0]);
        }
        auto lret = visitAdditiveExpression(in[0]);
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        Type tp = rret;
        if (ctx->children[index * 2 + 1]->getText() == "<<")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LSHIFT});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal << *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == ">>")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::RSHIFT});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal >> *rret.constexprVal;
            }
        }
        // 返回计算结果类型
        return tp;
    };
    return func(std::span<CParser::AdditiveExpressionContext*>(ctx->additiveExpression().data(),
                                                               ctx->additiveExpression().size()),
                0);
}
Type astVisitor::visitAdditiveExpression(CParser::AdditiveExpressionContext* ctx)
{
    std::function<Type(std::span<CParser::MultiplicativeExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<CParser::MultiplicativeExpressionContext*> in,
                           int index) -> Type
    {
        if (in.size() == 1)
        {
            return visitMultiplicativeExpression(in[0]);
        }
        std::string op_token = ctx->children[index * 2 + 1]->getText();
        auto posnow = funcnow->funcInfo.asms.size();
        auto lret = visitMultiplicativeExpression(in[0]);
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        funcnow->funcInfo.asms.resize(posnow); // 之前只是为了拿到类型
        if (lret.kind == Type::Kind::Struct || rret.kind == Type::Kind::Struct)
        {
            THROW_ERR(error::unsurpported_op, ctx);
        }
        if (lret.kind == Type::Kind::Pointer && rret.kind == Type::Kind::Pointer)
        {
            if (op_token == "+" || *lret.subType != *rret.subType)
            {
                THROW_ERR(error::unsurpported_op, ctx);
            }
        }
        if (lret.kind == Type::Kind::Pointer && rret.kind == Type::Kind::Basic &&
            (rret.basic_type == Type::BasicType::Int || rret.basic_type == Type::BasicType::Char ||
             rret.basic_type == Type::BasicType::Long))
        {
            // 左指针右整形
            auto lret = visitMultiplicativeExpression(in[0]);
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
            auto lret = visitMultiplicativeExpression(in[0]);
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, rret.subType->getsize()});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        else // 其他基础2类型不做特殊处理
        {
            auto lret = visitMultiplicativeExpression(in[0]);
            auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        }
        if (op_token == "+")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            auto res = deduce_binary_type(lret, rret, BinOp::Add);
            // 考虑指针常量? [TODO]
            if (lret.constexprVal && rret.constexprVal)
            {
                res.constexprVal = *lret.constexprVal + *rret.constexprVal;
            }
            return res;
        }
        else if (op_token == "-")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            auto res = deduce_binary_type(lret, rret, BinOp::Sub);
            // 考虑指针常量? [TODO]
            if (lret.constexprVal && rret.constexprVal)
            {
                res.constexprVal = *lret.constexprVal - *rret.constexprVal;
            }
            if (lret.kind == Type::Kind::Pointer && rret.kind == Type::Kind::Pointer)
            {
                // 指针相减
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, lret.subType->getsize()});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::DIV});
                if (res.constexprVal)
                {
                    *res.constexprVal /= lret.subType->getsize();
                }
            }
            return res;
        }
        THROW_ERR(error::unsurpported_op, ctx);
    };
    return func(std::span<CParser::MultiplicativeExpressionContext*>(
                    ctx->multiplicativeExpression().data(), ctx->multiplicativeExpression().size()),
                0);
}
Type astVisitor::visitMultiplicativeExpression(CParser::MultiplicativeExpressionContext* ctx)
{
    // 注意处理多个* / %
    std::function<Type(std::span<CParser::CastExpressionContext*>, int)> func =
        [&func, this, ctx](std::span<CParser::CastExpressionContext*> in, int index) -> Type
    {
        if (in.size() == 1)
        {
            return (visitCastExpression(in[0]));
        }
        auto lret = (visitCastExpression(in[0]));
        auto rret = func(in.subspan(1, in.size() - 1), index + 1);
        Type tp = rret;
        if (ctx->children[index * 2 + 1]->getText() == "*")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal * *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == "/")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::DIV});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal / *rret.constexprVal;
            }
        }
        else if (ctx->children[index * 2 + 1]->getText() == "%")
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MOD});
            if (lret.constexprVal && rret.constexprVal)
            {
                tp.constexprVal = *lret.constexprVal % *rret.constexprVal;
            }
        }
        // 返回计算结果类型
        return tp;
    };
    return func(std::span<CParser::CastExpressionContext*>(ctx->castExpression().data(),
                                                           ctx->castExpression().size()),
                0);
}
Type astVisitor::visitCastExpression(CParser::CastExpressionContext* ctx)
{
    if (ctx->castExpression())
    {
        auto cret = visitCastExpression(ctx->castExpression());
        // [TODO] visitTypeName
        auto tret = visitTypeName(ctx->typeName());
        tret.constexprVal = cret.constexprVal;
        return tret;
    }
    if (ctx->unaryExpression())
    {
        return visitUnaryExpression(ctx->unaryExpression());
    }
    //[TODO] DigitSequence 何意义?
    throw;
}

Type astVisitor::visitUnaryExpression(CParser::UnaryExpressionContext* ctx)
{
    Type type;
    auto sizenow = funcnow->funcInfo.asms.size();
    bool typeNameusesizeof = false;
    bool typeNameuseAlignof = false;

    if (ctx->postfixExpression())
    {
        // 分支4: postfixExpression
        auto pret = visitPostfixExpression(ctx->postfixExpression());
        type = pret;
    }
    else if (ctx->unaryOperator)
    {
        // 分支5: unaryOperator castExpression
        if (ctx->unaryOperator->getText() == "&")
        {
            // 必不可能是constexpr
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
        else if (ctx->unaryOperator->getText() == "*")
        {
            // 必不可能是constexpr
            auto cret = visitCastExpression(ctx->castExpression());
            if (cret.kind != Type::Kind::Pointer)
            {
                THROW_ERR(error::expected_ptr, ctx->castExpression());
            }
            type = *cret.subType;
            loadStackTopAddrByType(type);
        }
        else if (ctx->unaryOperator->getText() == "+") //+12
        {
            auto cret = visitCastExpression(ctx->castExpression());
            type = cret;
        }
        else if (ctx->unaryOperator->getText() == "-") //-12
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            auto cret = visitCastExpression(ctx->castExpression());
            type = cret;
            if (type.constexprVal)
            {
                type.constexprVal = -(*type.constexprVal);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
        }
        else if (ctx->unaryOperator->getText() == "~")
        {
            auto cret = visitCastExpression(ctx->castExpression());
            type = cret;
            if (type.constexprVal)
            {
                type.constexprVal = ~(*type.constexprVal);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::NOT});
        }
        else if (ctx->unaryOperator->getText() == "!")
        {
            auto cret = visitCastExpression(ctx->castExpression());
            type = Type{Type::Kind::Basic, Type::BasicType::Int};
            if (type.constexprVal)
            {
                type.constexprVal = !(*type.constexprVal);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, 0});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::CMP});
        }
    }
    else if (ctx->typeName() || ctx->unaryExpression())
    {
        // ('sizeof' | Alignof)('(' typeName ')' | unaryExpression)
        for (auto it = ctx->children.rbegin(); it != ctx->children.rend(); it++)
        {
            std::string text = (*it)->getText();
            if (text == "sizeof")
            {
                typeNameusesizeof = true;
                typeNameuseAlignof = false;
                break;
            }
            else if (text == "_Alignof")
            {
                typeNameusesizeof = false;
                typeNameuseAlignof = true;
                break;
            }
        }
        Type cret;
        if (ctx->typeName())
        {
            cret = visitTypeName(ctx->typeName());
        }
        else if (ctx->unaryExpression())
        {
            cret =
                tryVisitType([this, ctx] { return visitUnaryExpression(ctx->unaryExpression()); });
        }
        else
        {
            throw;
        }
        if (typeNameusesizeof)
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, cret.getsize()});
            type = Type{Type::Kind::Basic, Type::BasicType::Long};
            type.constexprVal = cret.getsize();
        }
        else if (typeNameuseAlignof) // TODO 补全 Align 实现
        {
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, VCPU::size_word});
            type = Type{Type::Kind::Basic, Type::BasicType::Long};
            type.constexprVal = VCPU::size_word;
        }
    }

    // 处理前缀 ++ , -- , sizeof
    for (int i = ctx->children.size() - 1; i >= 0; i--)
    {
        std::string child_text = ctx->children[i]->getText();
        if (child_text == "++")
        {
            // 必不可能是constexp
            if (stackTopIsLvalue())
            {
                madeTopIsLvalueAddr();
            }
            else
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            loadStackTopAddrByType(type);
            long step = 1;
            if (type.kind == Type::Kind::Pointer)
            {
                step = static_cast<long>(type.subType ? type.subType->getsize() : VCPU::size_word);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            saveStackTopAddrValueByType(type);
            loadStackTopAddrByType(type);
        }
        else if (child_text == "--")
        {
            // 必不可能是constexp
            if (stackTopIsLvalue())
            {
                madeTopIsLvalueAddr();
            }
            else
            {
                THROW_ERR(error::expected_lvalue, ctx);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::COPY});
            loadStackTopAddrByType(type);
            long step = 1;
            if (type.kind == Type::Kind::Pointer)
            {
                step = static_cast<long>(type.subType ? type.subType->getsize() : VCPU::size_word);
            }
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, step});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::SUB});
            saveStackTopAddrValueByType(type);
            loadStackTopAddrByType(type);
        }
        else if (child_text == "sizeof")
        {
            if (typeNameusesizeof)
            {
                typeNameusesizeof = false; // 消耗一次sizeof
                continue;
            }
            funcnow->funcInfo.asms.resize(sizenow);
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, type.getsize()});
            type = Type{Type::Kind::Basic, Type::BasicType::Long};
            type.constexprVal = type.getsize();
        }
    }
    return type;
}
Type astVisitor::visitPostfixExpression(CParser::PostfixExpressionContext* ctx)
{
    // 注意处理多个后缀
    int idindex = ctx->Identifier().size() - 1;
    std::function<Type(int, int)> func = [&func, this, ctx, &idindex](int start, int end) -> Type
    {
        if (end == 0)
        {
            auto str = ctx->children[0]->getText();
            return visitPrimaryExpression(dc<CParser::PrimaryExpressionContext*>(ctx->children[0]));
        }
        auto todo = ctx->children[end - 1];
        if (todo->getText() == "(") // func call
        {
            // 必不可能是constexp
            size_t args_words = 0;

            if (ctx->children[end]->getText() != ")") // 有expressionlist
            {
                args_words = visitArgumentExpressionList(
                    dc<CParser::ArgumentExpressionListContext*>(ctx->children[end]));
            }
            auto funtype = tryVisitType([this, &func, end]() { return func(0, end - 1); });
            // 返回值为struct
            if (funtype.subType->kind == Type::Kind::Struct)
            {
                // args_words +=
                //     ((*funtype).subType->getsize() + VCPU::size_word - 1) / VCPU::size_word;
                args_words += 1; // 传入指针
                // 创建局部变量传入指针
                IDdef struct_ret;
                struct_ret.type = funtype.subType;
                struct_ret.kind = IDdef::Kind::Local;
                static size_t index = 0;
                struct_ret.name = std::format("__struct_ret", index++);
                auto def = record_ID_decl(struct_ret, funcnow);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, def->addr});
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::LEA, def->addr});
            }
            auto funcaddr = func(0, end - 1); // 解析函数地址
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
            if (funtype.subType->kind == Type::Kind::Struct)
            {
                funcnow->funcInfo.asms.push_back(
                    ASM{ASM::basic_asm::LODS, funtype.subType->getsize()});
            }
            return rettype;
        }
        else if (todo->getText() == "[") // arr[] ptr[]
        {
            // 必不可能是constexp
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
            (void)(visitExpression(dc<CParser::ExpressionContext*>(ctx->children[end])));
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, eleType.getsize()});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::MUL});
            funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::ADD});
            loadStackTopAddrByType(eleType);
            return eleType;
        }
        else if (todo->getText() == "++" || todo->getText() == "--")
        {
            // 必不可能是constexp
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
                auto pointed = valType.subType ? valType.subType->getsize() : VCPU::size_word;
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
            // 必不可能是constexp
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
            // 必不可能是constexp
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
Type astVisitor::visitPrimaryExpression(CParser::PrimaryExpressionContext* ctx)
{
    if (ctx->Identifier())
    {
        auto texts = ctx->Identifier()->getText();
        return load_var_or_func(ctx->Identifier()->getText());
    }
    else if (ctx->constant()) // 常量处理
    {
        std::string text = ctx->constant()->getText();

        // 处理字符常量
        if (isCharacterConstant(text))
        {
            try
            {
                int value = parseCharacterConstant(text);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, value});
                Type rettype{Type::Kind::Basic, Type::BasicType::Char};
                rettype.constexprVal = value;
                return rettype;
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->constant());
            }
        }
        // 处理整型常量
        else if (isIntegerConstant(text))
        {
            try
            {
                int value = parseIntegerConstant(text);
                funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, value});
                Type rettype{Type::Kind::Basic, Type::BasicType::Int};
                rettype.constexprVal = value;
                return rettype;
            }
            catch (...)
            {
                THROW_ERR(error::invalid_constant, ctx->constant());
            }
        }
        else // [TODO] 浮点数
        {
            THROW_ERR(error::unsurpported_num, ctx->constant());
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
        Type t{Type::Kind::Pointer, 1};
        t.pushTop(Type{Type::Kind::Basic, Type::BasicType::Char});
        return t;
    }
    else
    {
        auto str = ctx->getText();
        THROW_ERR(error::invalid_constant, ctx);
    }
    throw;
}
Type astVisitor::visitTypeName(CParser::TypeNameContext* ctx)
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

Type astVisitor::visitTypeofSpecifier(CParser::TypeofSpecifierContext* ctx)
{
    if (ctx->typeofSpecifierArgument()->expression())
    {
        return tryVisitType(
            [this, ctx]()
            { return visitExpression(ctx->typeofSpecifierArgument()->expression()); });
    }
    else if (ctx->typeofSpecifierArgument()->typeName())
    {
        return visitTypeName(ctx->typeofSpecifierArgument()->typeName());
    }
    throw;
}

void astVisitor::visitBlockItem(CParser::BlockItemContext* ctx)
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
    visitSpecifierQualifierList(CParser::SpecifierQualifierListContext* ctx,
                                std::vector<Type::TypeQualifier> typeQualifier)
{
    std::optional<Type> base;
    for (auto tsq : ctx->typeSpecifierQualifier())
    {
        if (auto tq = tsq->typeQualifier())
        {
            auto str = tq->getText();
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
                typeQualifier.push_back(Type::TypeQualifier::Atomic);
            }
        }
        else if (auto ts = tsq->typeSpecifier())
        {
            if (base)
            {
                THROW_ERR(error::double_type, ctx);
            }
            base = visitTypeSpecifier(ts);
        }
    }
    return {typeQualifier, base};
}

void astVisitor::visitSelectionStatement(CParser::SelectionStatementContext* ctx)
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

size_t astVisitor::visitArgumentExpressionList(CParser::ArgumentExpressionListContext* ctx)
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
            const size_t word = VCPU::size_word;
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
                words += (sz + VCPU::size_word - 1) / VCPU::size_word;
            }
        }
    }
    return words;
}

void astVisitor::visitIterationStatement(CParser::IterationStatementContext* ctx)
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

        auto visitforExpression = [this](CParser::ForExpressionContext* exprCtx) -> Type
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
        CParser::ForExpressionContext* condExpr = nullptr;
        CParser::ForExpressionContext* postExpr = nullptr;
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

void astVisitor::visitJumpStatement(CParser::JumpStatementContext* ctx)
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
                const size_t ret_ptr_offset = 2 * VCPU::size_word;

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

std::vector<std::pair<std::string, IDdef>> astVisitor::visitMemberDeclarationList(
    CParser::MemberDeclarationListContext* ctx)
{
    long nonameindex = 0;
    std::vector<std::pair<std::string, IDdef>> members;
    std::vector<IDdef> tps;
    for (auto each : ctx->memberDeclaration())
    {
        // [TODO]  目前忽略 const volatile restrict _Atomic
        auto [typeQualifier, basetype] =
            visitSpecifierQualifierList(each->specifierQualifierList());
        if (!basetype)
        {
            THROW_ERR(error::expected_type, ctx);
        }
        if (!each->memberDeclaratorList()) // 成员无名
        {
            IDdef tpvar;
            tpvar.type = *basetype;
            tpvar.name = std::format("__noname_member_{}", nonameindex);
            tpvar.valueType = ValueType::Left;
            tps.push_back(tpvar);
        }
        else
        {
            std::vector<IDdef> eachstructDeclarationTypes;
            for (auto eachstructDeclarator : each->memberDeclaratorList()->memberDeclarator())
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

Type astVisitor::visitAbstractDeclarator(CParser::AbstractDeclaratorContext* ctx)
{
    Type rettype;
    if (ctx->directAbstractDeclarator())
    {
        rettype = visitDirectAbstractDeclarator(ctx->directAbstractDeclarator());
    }
    if (ctx->pointer())
    {
        int ptr_count = 0;
        auto p = ctx->pointer();
        auto s = p->getText();
        ptr_count = static_cast<int>(std::count(s.begin(), s.end(), '*'));
        for (int i = 0; i < ptr_count; i++)
        {
            Type ptr;
            ptr.kind = Type::Kind::Pointer;
            rettype.pushTop(ptr);
        }
    }
    return rettype;
}

Type astVisitor::visitDirectAbstractDeclarator(CParser::DirectAbstractDeclaratorContext* ctx)
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
            Type(Type::Kind::Array,
                 *tryVisitType([this, ctx]
                               { return visitAssignmentExpression(ctx->assignmentExpression()); })
                      .constexprVal));
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
void astVisitor::visitAsmADDer(CParser::AsmADDerContext* ctx)
{
    std::string text = ctx->StringLiteral()->getText();
    std::string content;
    size_t pos = 0;
    if (decode_string_token_text(text, pos, content))
    {
        funcnow->funcInfo.asms.push_back(content);
    }
    else
    {
        funcnow->funcInfo.asms.push_back(text);
    }
    return;
}
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
    int v = 0;
    if (!decode_character_token(text, v))
        throw error::invalid_constant;
    return v;
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
    // 先检查是否是枚举常量
    if (auto* enumConst = lookup_ID_decl(name, StorageClassSpecifier::EnumConst))
    {
        // 枚举常量作为立即数加载
        funcnow->funcInfo.asms.push_back(ASM{ASM::basic_asm::IMM, enumConst->addr});
        Type rettype = enumConst->type;
        rettype.constexprVal = enumConst->addr;
        return rettype;
    }

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
