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
    size_t num_lay = 0;
};
