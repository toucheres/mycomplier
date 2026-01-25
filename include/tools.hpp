#pragma once
inline long align_up(long num, long align)
{
    if (num % align == 0)
    {
        return num;
    }
    else
    {
        return num + (align - num % align);
    }
};