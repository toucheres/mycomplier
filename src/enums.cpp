#include "enums.h"
#include <map>
size_t Type::size_of_type(Basic_Type type)
{
    std::map<Basic_Type, size_t> map{{Basic_Type::CHAR, 1}, {Basic_Type::INT, 4}};
    return map[type];
}