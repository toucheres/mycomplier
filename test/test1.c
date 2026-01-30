// #include <stdio.h>
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
    return arg;
}

int main()
{
    sh aaa = {1, 2, 3};
    aaa = func(aaa);
    return aaa.a;
}