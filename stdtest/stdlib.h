#include "./systemcall.h"
char* malloc(long in)
{
    return __malloc(in);
}
void free(char* ptr)
{
    __free(ptr);
    return;
}