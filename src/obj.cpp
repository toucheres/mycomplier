#include "obj.h"
#include "complier.hpp"
#include <algorithm>
#include <format>
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