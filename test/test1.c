#include <systemcall.h>
int main()
{
    char* ptr = __malloc(1);
    *ptr = 42;
    return *ptr;
}
