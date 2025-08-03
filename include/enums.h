#pragma once
#include <vector>
enum class Basic_Type
{
    INT,
    CHAR
};
struct Type
{
    Basic_Type bt;
    size_t ptr_lay = 0;
    static size_t size_of_type(Basic_Type type);
};
