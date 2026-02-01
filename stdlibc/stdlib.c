#include <systemcall.h>
#include <stdlib.h>
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
    for (int i = 0; i < count;i++)
    {
        ((char*)dest)[i] = ((char*)src)[i];
    }
    return dest;
}
