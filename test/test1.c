#include <stdio.h>
int main()
{
    char arr[5] = {'a', 'b', 'c', '\n', '\0'};
    char* ptr = arr;
    while (*ptr != '\0')
    {
        write(*ptr);
        ptr = ptr + 1;
    }
    return 0;
}