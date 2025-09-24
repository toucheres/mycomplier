#include <stdio.h>
long fun(int arg, ...)
{
    char a;
    a = 97;
    // write(a);
    return 97;
}
int main()
{
    long a;
    a = fun(3, 6, 97, 8);
    return 1;
}