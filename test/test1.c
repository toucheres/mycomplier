#include <stdio.h>
int main()
{
    int arr[3] = {122,13,45};
    int* ptr = arr;
    ptr++;
    return ptr[1];
}
