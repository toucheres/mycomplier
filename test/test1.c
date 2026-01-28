// #include <string.h>
typedef struct tests
{
    int a;
    char b;
    int c;
} sh;

int func(sh arg)
{
    return arg.a;
}

int main()
{
    int b = 12;
    int c = 13;
    return b;
}