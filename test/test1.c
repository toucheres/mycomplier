#include <stdio.h>
int main()
{
    char src[100];
    char fmt[12] = {'a', '%', 'd', 'c', 0};
    return printf(fmt, 123);
}
