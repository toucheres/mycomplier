#include <stdio.h>

int arr[12];

void func()
{
    arr[0] = 'a';
}

int main()
{
    func();
    printf("%c\n",arr[0]);
    return 0;
}