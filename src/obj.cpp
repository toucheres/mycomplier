#include "obj.h"
#include "ASM.hpp"
#include "CLexer.h"
#include "CParser.h"
#include "CParserBaseVisitor.h"
#include "complier.hpp"
#include "obj.h"
#include "preprocessor.hpp"
#include "settings.h"
#include "tools.hpp"
#include <algorithm>
#include <antlr4-runtime/antlr4-runtime.h>
#include <astVisit.h>
#include <filesystem>
#include <format>
#include <iostream>
#include <platform.h>
#include <preprocessor.hpp>
#include <regex>
#include <settings.h>
#include <tools.hpp>
#include <utility>
size_t Type::getsize() const
{
    switch (kind)
    {
    case Kind::Pointer:
        return VCPU::size_word;
    case Kind::Basic:
        switch (basic_type)
        {
        case BasicType::Char:
            return 1;
        case BasicType::Short:
            return VCPU::size_word / 4;
        case BasicType::Int:
            return VCPU::size_word / 2;
        case BasicType::Long:
            return VCPU::size_word;
        default:
            break;
        }
        break;
    case Kind::Array:
        return static_cast<size_t>(arr_num) * subType->getsize();
    case Kind::Function:
        return VCPU::size_word;
    case Kind::Struct:
        return align_up(this->structInfo.members.back().second.addr +
                            this->structInfo.members.back().second.type.getsize(),
                        8);
    case Kind::Undefined:
        break;
    }
    throw;
}

bool Type::operator==(const Type& other_) const
{
    auto one = this;
    auto other = &other_;
    if (other->kind != one->kind)
    {
        return false;
    }
    auto& dkind = one->kind;
    if (dkind == Type::Kind::Basic)
    {
        return one->basic_type == other->basic_type;
    }
    else if (dkind == Type::Kind::Array || dkind == Type::Kind::Pointer)
    {
        return one->args == other->args && *one->subType == *other->subType;
    }
    else if (dkind == Type::Kind::Function)
    {
        if (one->args.size() != other->args.size())
        {
            return false;
        }
        for (size_t i = 0; i < one->args.size(); i++)
        {
            if (one->args[i].type != other->args[i].type)
            {
                return false;
            }
        }
        return one->basic_type == other->basic_type;
    }
    else if (dkind == Type::Kind::Undefined)
    {
        return false;
    }
    return false;
}

Type::Type(Kind kind_, BasicType arg_)
{
    assert(kind_ == Type::Kind::Basic);
    kind = kind_;
    basic_type = arg_;
}

Type::Type(Kind kind_, int arg_)
{
    assert(kind_ == Type::Kind::Array || kind_ == Type::Kind::Pointer);
    kind = kind_;
    if (kind_ == Type::Kind::Array)
    {
        arr_num = arg_;
    }
    // Pointer 不再使用 arr_num，通过 subType 嵌套表示
}

Type::Type(Kind kind_)
{
    kind = kind_;
}

Type::Type(Kind kind_, std::vector<IDdef> args_)
{
    assert(kind_ == Type::Kind::Function);
    kind = kind_;
    args = args_;
}

Type& Type::getTop()
{
    Type* now = this;
    while (now->kind != Type::Kind::Basic && now->kind != Type::Kind::Undefined && now->subType)
    {
        now = now->subType.get();
    }
    return *now;
}

bool Type::pushTop(const Type& what)
{
    auto& top = getTop();
    if (top.kind == Type::Kind::Undefined)
    {
        top = what;
    }
    else
    {
        top.subType = value_ptr<Type>::make_value_ptr(what);
    }
    return true;
}

Type Type::popTop()
{
    Type* p = nullptr;
    Type* now = this;
    while (now->kind != Type::Kind::Basic && now->kind != Type::Kind::Undefined && now->subType)
    {
        p = now;
        now = now->subType.get();
    }
    auto tp = *p->subType;
    p->subType.reset();
    return tp;
}

//[REWRITE] 修改 to_string() 方法
std::string Type::to_string() const
{
    return "";
}

size_t IDdef::get_addr_in_stack(size_t posnow)
{
    // [TODO] char的考虑
    // 考虑对齐要求
    auto ceiling = [](int n, int x)
    {
        // x 必须是 2 的幂
        return (n + x - 1) & ~(x - 1);
    };
    if (type.getsize() >= 1)
    {
        return ceiling(posnow + this->type.getsize(), VCPU::size_word); // 姑且对齐到size_word
    }
    return posnow;
}
bool IDdef::operator==(const IDdef& that) const
{
    return this->type == that.type && this->name == that.name &&
           this->storageClassSpecifier == that.storageClassSpecifier;
}
std::expected<std::vector<std::string>, error> complier::process(
    std::vector<std::string> paths, bool showASt, int tolerate, bool showFoldedNames,
    bool disableStd, std::vector<std::string> mainargs, bool preprocess_only)
{
    // override incoming params by reading singleton settings to centralize runtime config
    using namespace app_options;
    auto& tvm = settings::tvm();
    showASt = tvm.get(app_options::show_ast).or_(false);
    tolerate = tvm.get(app_options::tolerate).or_(4);
    showFoldedNames = tvm.get(app_options::show_folded_names).or_(false);
    disableStd = tvm.get(app_options::disable_std).or_(false);
    preprocess_only = tvm.get(app_options::preprocess_only).or_(false);
    try
    {
        mainargs = tvm.get_direct(main_args);
    }
    catch (...)
    {
        mainargs = {};
    }

    std::vector<OBJ> objs;
    if (!disableStd)
    {
        for (auto each : platform::get_default_libc_paths())
        {
            OBJ obj;
            obj.name = each;
            // 创建输入流
            Preprocessor{}.process(each);
            std::ifstream in(each + ".pre");
            if (preprocess_only)
            {
                continue;
            }
            std::string input((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
            antlr4::ANTLRInputStream inputStream(input);
            // 创建词法分析器
            CLexer lexer(&inputStream);
            antlr4::CommonTokenStream tokens(&lexer);
            // 创建自定义语法分析器
            CParser parser(&tokens);
            // 使用正确的入口规则
            CParser::CompilationUnitContext* tree = parser.compilationUnit();
            // fordebug
            if (showASt)
            {
                std::cout << each << ": \n";
                printAST(tree);
                std::cout << "\n";
            }
            // 创建和使用自定义访问器
            astVisitor visitor{each, obj};
            visitor.visitCompilationUnit(tree);
            obj.flush_global_decls();
            objs.push_back(std::move(obj));
        }
    }
    for (auto each : paths)
    {
        OBJ obj;
        obj.name = each;
        // 创建输入流
        Preprocessor{}.process(each);
        if (preprocess_only)
        {
            continue;
        }
        std::ifstream in(each + ".pre");
        std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        antlr4::ANTLRInputStream inputStream(input);
        // 创建词法分析器
        CLexer lexer(&inputStream);
        antlr4::CommonTokenStream tokens(&lexer);
        // 创建自定义语法分析器
        CParser parser(&tokens);
        // 使用正确的入口规则
        CParser::CompilationUnitContext* tree = parser.compilationUnit();
        // fordebug
        if (showASt)
        {
            std::cout << each << ": \n";
            printAST(tree);
            std::cout << "\n";
        }
        // 创建和使用自定义访问器
        astVisitor visitor{each, obj};
        visitor.visitCompilationUnit(tree);
        obj.flush_global_decls();
        objs.push_back(std::move(obj));
    }
    if (preprocess_only)
    {
        return {};
    }
    linker linker{objs, mainargs};
    return linker.process();
}

template <class MapT> void enter_scope_all(MapT& map, const std::string& label)
{
    auto info = map.current_scope_info();
    if (label.empty())
    {
        map.enter_scope_with_info(info);
    }
    else
    {
        map.enter_scope(label, info);
    }
}
void DeclRepository::enter_scope(const Label& label)
{
    enter_scope_all(var_decls, label);
    enter_scope_all(extern_decls, label);
    enter_scope_all(static_decls, label);
    enter_scope_all(typedef_decls, label);
    enter_scope_all(struct_decls, label);
    enter_scope_all(enum_decls, label);
    enter_scope_all(enum_const_decls, label);
}

void DeclRepository::exit_scope()
{
    var_decls.out_scope();
    extern_decls.out_scope();
    static_decls.out_scope();
    typedef_decls.out_scope();
    struct_decls.out_scope();
    enum_decls.out_scope();
    enum_const_decls.out_scope();
}

IDdef* DeclRepository::add_ID_decl(const IDdef& def, StorageClassSpecifier storageClassSpecifier)
{
    StorageClassSpecifier effective_storage = storageClassSpecifier;
    IDdef copy = def;
    tree_scoped_map<std::string, IDdef, Label, ScopeMeta>* where = nullptr;
    switch (storageClassSpecifier)
    {
    case StorageClassSpecifier::VarDef:
        if (def.kind == IDdef::Kind::Global)
        {
            if (var_decls.getwheredeeps(0)[0]->find(def.name) !=
                var_decls.getwheredeeps(0)[0]->end())
            {
                return nullptr;
            }
            var_decls.getwheredeeps(0)[0]->try_emplace(def.name, def);
        }
        where = &this->var_decls;
        break;
    case StorageClassSpecifier::Static:
        where = &this->static_decls;
        break;
    case StorageClassSpecifier::Typedef:
        where = &this->typedef_decls;
        break;
    case StorageClassSpecifier::Extern:
        where = &this->extern_decls;
        break;
    case StorageClassSpecifier::StructDef:
        where = &this->struct_decls;
        break;
    case StorageClassSpecifier::EnumDef:
        where = &this->enum_decls;
        break;
    case StorageClassSpecifier::EnumConst:
        where = &this->enum_const_decls;
        break;
    default:
        throw;
    }
    where->add(copy.name, copy);
    return find_ID_decl(copy.name, effective_storage);
}

IDdef* DeclRepository::find_ID_decl(const std::string& ID,
                                    StorageClassSpecifier storageClassSpecifier)
{
    switch (storageClassSpecifier)
    {
    case StorageClassSpecifier::Extern:
        return extern_decls.find(ID);
    case StorageClassSpecifier::Static:
        return static_decls.find(ID);
    case StorageClassSpecifier::Typedef:
        return typedef_decls.find(ID);
    case StorageClassSpecifier::StructDef:
        return struct_decls.find(ID);
    case StorageClassSpecifier::EnumDef:
        return enum_decls.find(ID);
    case StorageClassSpecifier::EnumConst:
        return enum_const_decls.find(ID);
    default:
        break;
    }

    if (auto* p = var_decls.find(ID))
    {
        return p;
    }
    if (auto* p = static_decls.find(ID))
    {
        return p;
    }
    return extern_decls.find(ID);
}

void OBJ::flush_global_decls()
{
    // variables: normal
    decls.var_decls.for_each_scope(
        [this](std::size_t depth, auto& entries, auto&, const auto&)
        {
            if (depth != 0)
            {
                return;
            }
            for (auto& [name, v] : entries)
            {
                symbol_table.globaldef[name] = v;
            }
        });

    // static variables: always emit, scoped to object to avoid cross-object collisions.
    // 名称修饰在 record 阶段完成
    decls.static_decls.for_each_scope(
        [this](std::size_t /*depth*/, auto& entries, auto&, const auto&)
        {
            for (auto& [name, v] : entries)
            {
                symbol_table.globaldef[name] = v;
            }
        });

    // extern declarations become decl entries
    decls.extern_decls.for_each_scope(
        [this](std::size_t /*depth*/, auto& entries, auto&, const auto&)
        {
            for (auto& [name, v] : entries)
            {
                symbol_table.globaldecl[name] = v;
            }
        });
}

size_t StructInfo::getmemberbias(std::string membername)
{
    return getmember(membername)->addr;
}

IDdef* StructInfo::getmember(std::string name)
{
    for (auto& each : members)
    {
        if (each.first == name)
        {
            return &each.second;
        }
    }
    return nullptr;
}
void OBJ::enter_decl_scope(const std::string& label)
{
    decls.enter_scope(label);
}

void OBJ::exit_decl_scope()
{
    decls.exit_scope();
}