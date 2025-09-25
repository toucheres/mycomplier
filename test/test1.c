#include <stdio.h>
int main()
{
    long a;
    char src[10] = {0, 0, 0, 0, 0};
    char fmt[10] = {'a', '%', 'd', '%', 'c', '%', 'l', 'd', 'c', '\0'};
    a = fprintf(src, fmt, 123, 'h', 1234);
    // a = printf(fmt, 123, 'h', 1234);
    print_str(src);
    return a;
}
