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

void* memcpy(void* dest, void* src, long count)
{
    for (int i = 0; i < count; i++)
    {
        ((char*)dest)[i] = ((char*)src)[i];
    }
    return dest;
}

int memcmp(void* str1, void* str2, long n)
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
