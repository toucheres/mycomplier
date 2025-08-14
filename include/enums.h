#pragma once
#include <vector>
enum class Basic_Type
{
    INT,
};
struct Type
{
    Basic_Type bt;
    int ptr_lay = 0;
    static int size_of_type(Basic_Type type);
};
