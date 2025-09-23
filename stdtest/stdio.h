#include <systemcall.h>
void print_str(char* str)
{
    while (*str != '\0')
    {
        write(*str);
        str = str + 1;
    }
    return;
}