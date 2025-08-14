#include "enums.h"
#include <map>
int Type::size_of_type(Basic_Type type)
{
    std::map<Basic_Type, int> map{{Basic_Type::INT, 1}};
    return map[type];
}