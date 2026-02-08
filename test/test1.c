#include <stdio.h>
typedef struct mystruct
{
    int aaa;
} mystruct_t;
int main()
{
    mystruct_t test;
    test.aaa = 12;
    return printf("test.aaa = %d\n", test.aaa);
}