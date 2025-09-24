#include <stdio.h>
// long* load_arg_ptr(long* ptr, int n)
// {
//     return ptr - n;
// }
// long fun(int arg, ...)
// {
//     _asm_("IMM 24");
//     _asm_("LEA");
//     _asm_("LW");
//     write();
//     return 0;
// }
int main()
{
    // return fun(3, '6', '\n');
    int arr[3] = {'a','b','c'};
    int* p = arr;
    write(*p);
    write(*(p+1));
    return 0;
}