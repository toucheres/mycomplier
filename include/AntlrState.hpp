// #pragma once
#include <string>
#include <vector>
class xxxx
{
    // 这里的代码会直接加入生成的 Parser 类
    std::vector<std::string> TypedefedId{};
    bool hasTypeDef(std::string name)
    {
        for (const auto& each : TypedefedId)
        {
            if (each == name)
            {
                return true;
            }
        }
        return false;
    }
    bool addTypeDef(std::string name)
    {
        if (hasTypeDef(name))
        {
            return false;
        }
        TypedefedId.push_back(name);
        return true;
    }
};