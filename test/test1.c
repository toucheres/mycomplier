#include <stdio.h>
extern int a;

int func(int in)
{
    int tp = in;
    if (tp > 0)
    {
        return tp * func(tp - 1);
    }
    return 1;
}

int main()
{
    return printf("4! == %d\n", func(a));
}