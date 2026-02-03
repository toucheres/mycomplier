#include <stdlib.h>
#include <systemcall.h>
char* malloc(long in)
{
    return __malloc(in);
}
void free(char* ptr)
{
    __free(ptr);
    return;
}

char* memcpy(char* dest, char* src, long count)
{
    return __memcpy(dest, src, count);
}

int memcmp(char* str1, char* str2, long n)
{
    char* s1 = str1;
    char* s2 = str2;

    while (n--)
    {
        if (*s1 != *s2)
        {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }
    return 0;
}

void exit(int code)
{
    return __exit(code);
}

char* memset(char* s, int c, long n)
{
    return __memset(s, c, n);
}