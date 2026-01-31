
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
#define va_start(ap, last_named_arg) (ap = (long)&last_named_arg + align_up(sizeof(last_named_arg),8))
#define va_arg(ap, type) (*((type*)va_arg_fun(&ap, sizeof(type))))
#define va_end(ap) 0; // 无需清理

int varfunctest(char* base, ...)
{
    va_list ap;
    va_start(ap, base); // 初始化 ap 指向 base 后面

    int arg1 = va_arg(ap, int); // 取第 1 个可变参数
    int arg2 = va_arg(ap, int); // 取第 2 个

    va_end(ap); // 清理（某些平台需要）
    return arg2;
}
int main()
{
    return varfunctest("nothing", 1, 2);
}