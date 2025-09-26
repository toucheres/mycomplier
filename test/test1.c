#include <stdio.h>
int main()
{
    char fmt[12] = "a%db";
    return printf(fmt, 123);
}
