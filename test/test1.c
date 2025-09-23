#include <stdio.h>
int main()
{
    char arr[15] = {'h', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!', '\n', '\0'};
    char* ptr = arr;
    while (*ptr != '\0')
    {
        write(*ptr);
        ptr = ptr + 1;
    }
    return 0;
}