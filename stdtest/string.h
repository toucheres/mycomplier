#include <string.h>
long strlen(char* str)
{
    long lenth = 0;
    while (*str != 0)
    {
        lenth = lenth + 1;
        str = str + 1;
    }
    return lenth;
}