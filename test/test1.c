#include <stdio.h>
typedef struct tests
{
    int a;
    char b;
    int c;
} sh;

// sh func(sh arg)
// {
//     arg.a = 4;
//     arg.b = 5;
//     arg.c = 6;
//     printf("arg.a=%d,arg.b=%c,arg.c=%c\n", arg.a, arg.b, arg.c);
//     return arg;
// }

int main()
{
    sh aaa = {1, 2, 3};
    printf("aaa.a=%d,aaa.b=%c,aaa.c=%d\n", aaa.a, aaa.b, aaa.c);
    sh bbb = {4, 5, 6};
    aaa = bbb;
    printf("aaa.a=%d,aaa.b=%c,aaa.c=%d\n", aaa.a, aaa.b, aaa.c);
    return 0;
}