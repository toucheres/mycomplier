#include <stdio.h>
typedef struct tests
{
    int a;
    char b;
    int c;
} sh;

sh func(sh arg)
{
    arg.a = 4;
    arg.b = 5;
    arg.c = 6;
    printf("arg.a=%d,arg.b=%c,arg.c=%c\n", arg.a, arg.b, arg.c);
    return arg;
}

int main()
{
    return fopen("/home/toucher/vscoderope/mycomplier/test/test1.c", "r")->_fileno;
}