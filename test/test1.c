#include <stdio.h>
struct tests
{
    int a;
    char b;
    int c;
};
int main()
{
    struct tests c = {1, 2, 3};
    return printf("c.a = %d, c.b = %c, c.c = %d\n", c.a, c.b, c.c);
}