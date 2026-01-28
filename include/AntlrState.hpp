// #pragma once
#include <string>
#include "scoped_map.hpp"
class xxxx
{
    // 这里的代码会直接加入生成的 Parser 类
    scoped_map<std::string, bool> TypedefedId{};
    bool currentDeclIsTypedef = false;

    bool hasTypeDef(const std::string& name)
    {
        return TypedefedId.contains(name);
    }

    bool addTypeDef(const std::string& name)
    {
        return TypedefedId.add(name, true);
    }
};