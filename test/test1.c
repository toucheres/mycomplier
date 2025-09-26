#include <string.h>
#include <stdio.h>
int main()
{
    char src[12];
    char from[12] = "hello!\n";
    strcpy(src, from);
    return printf(src);
}