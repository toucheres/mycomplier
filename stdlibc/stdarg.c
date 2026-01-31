#include <stdarg.h>
long align_up(long num, long align)
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
char* va_arg_fun(char** ap, int size)
{
    char* tp = *ap;
    (*ap) += align_up(size, 8); // 不满1字也占1字
    return tp;
}