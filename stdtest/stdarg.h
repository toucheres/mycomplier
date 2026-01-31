long align_up(long num, long align)
{
    if (num % align == 0)
    {
        return num;
    }
    else
    {
        return num + (align - num % align);
    }
};
char* va_arg_fun(char** ap, int size)
{
    char* tp = *ap;
    (*ap) += align_up(size, 8); // 不满1字也占1字
    return tp;
}
#define va_list char*
#define va_start(ap, last_named_arg)                                                               \
    (ap = (char*)((long)&last_named_arg + align_up(sizeof(last_named_arg), 8)))
#define va_arg(ap, type) (*((type*)va_arg_fun(&ap, sizeof(type))))
#define va_copy(dest, src) ((dest) = (src))
#define va_end(ap) 0; // 无需清理