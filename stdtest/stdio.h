#include <stdarg.h>
#include <systemcall.h>
void write(long arg)
{
    return __write(arg);
}
long print_str(char* str)
{
    long num = 0;
    while (*str != '\0')
    {
        write(*str);
        str++;
        num++;
    }
    return num;
}
long num_to_str(char* src, long num)
{
    // 处理特殊值 0 和 负数，避免无限递归
    if (num == 0)
    {
        *src = '0';
        return 1;
    }
    if (num < 0)
    {
        *src = '-';
        long len = num_to_str(src + 1, -num);
        return len + 1;
    }

    // 递归写高位，再写当前位
    if (num < 10)
    {
        *src = '0' + num;
        return 1;
    }
    else
    {
        long higher = num / 10;
        long rem = num % 10;
        long len = num_to_str(src, higher);
        *(src + len) = '0' + rem;
        return len + 1;
    }
}
// // 仅支持%s %d %c %ld
// long fprintf(char* src, char* fmt, ...)
// {
//     long arg_index = 1;
//     long charsnum = 0;
//     while (*fmt != 0)
//     {
//         if (*fmt == '%')
//         {
//             fmt = fmt + 1;
//             if (*fmt == 'c')
//             {
//                 *src = *load_arg_ptr(&fmt, arg_index);
//                 arg_index = arg_index + 1;
//                 charsnum = charsnum + 1;
//                 fmt = fmt + 1;
//                 src = src + 1;
//             }
//             else if (*fmt == 'd')
//             {
//                 long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
//                 arg_index = arg_index + 1;
//                 charsnum = charsnum + size;
//                 fmt = fmt + 1;
//                 src = src + size;
//             }
//             else if (*fmt == 'l')
//             {
//                 fmt = fmt + 1;
//                 if (*fmt == 'd')
//                 {
//                     long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
//                     arg_index = arg_index + 1;
//                     charsnum = charsnum + size;
//                     fmt = fmt + 1;
//                     src = src + size;
//                 }
//                 else
//                 {
//                     *src = *fmt;
//                     charsnum = charsnum + 1;
//                     fmt = fmt + 1;
//                     src = src + 1;
//                 }
//             }
//             else
//             {
//                 // 未知格式：把 '%' 和随后字符都按字面输出（若后面是 '\0' 则只输出 '%'）
//                 *src++ = '%';
//                 charsnum++;
//                 if (*fmt != '\0')
//                 {
//                     *src++ = *fmt;
//                     charsnum++;
//                     fmt++;
//                 }
//             }
//         }
//         else
//         {
//             *src = *fmt;
//             charsnum = charsnum + 1;
//             fmt = fmt + 1;
//             src = src + 1;
//         }
//     }
//     *src = '\0';
//     return charsnum;
// }
// // ...existing code...
// long fprintf(char* src, char* fmt, ...)
// {
//     long arg_index = 1;
//     long charsnum = 0;
//     while (*fmt != 0)
//     {
//         if (*fmt == '%')
//         {
//             fmt = fmt + 1;
//             if (*fmt == 'c')
//             {
//                 *src = *load_arg_ptr(&fmt, arg_index);
//                 arg_index = arg_index + 1;
//                 charsnum = charsnum + 1;
//                 fmt = fmt + 1;
//                 src = src + 1;
//             }
//             else if (*fmt == 'd')
//             {
//                 long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
//                 arg_index = arg_index + 1;
//                 charsnum = charsnum + size;
//                 fmt = fmt + 1;
//                 src = src + size;
//             }
//             else if (*fmt == 'l')
//             {
//                 fmt = fmt + 1;
//                 if (*fmt == 'd')
//                 {
//                     long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
//                     arg_index = arg_index + 1;
//                     charsnum = charsnum + size;
//                     fmt = fmt + 1;
//                     src = src + size;
//                 }
//                 else
//                 {
//                     *src = *fmt;
//                     charsnum = charsnum + 1;
//                     fmt = fmt + 1;
//                     src = src + 1;
//                 }
//             }
//             else
//             {
//                 // 未知格式：把 '%' 和随后字符都按字面输出（若后面是 '\0' 则只输出 '%'）
//                 *src++ = '%';
//                 charsnum++;
//                 if (*fmt != '\0')
//                 {
//                     *src++ = *fmt;
//                     charsnum++;
//                     fmt++;
//                 }
//             }
//         }
//         else
//         {
//             *src = *fmt;
//             charsnum = charsnum + 1;
//             fmt = fmt + 1;
//             src = src + 1;
//         }
//     }
//     *src = '\0';
//     return charsnum;
// }
// 仅支持%s %d %c %ld
long fprintf(char* src, char* fmt, ...)
{
    long arg_index = 1;
    long charsnum = 0;
    while (*fmt != 0)
    {
        if (*fmt == '%')
        {
            fmt = fmt + 1;
            if (*fmt == 'c')
            {
                *src = *load_arg_ptr(&fmt, arg_index);
                arg_index = arg_index + 1;
                charsnum = charsnum + 1;
                fmt = fmt + 1;
                src = src + 1;
            }
            else if (*fmt == 'd')
            {
                long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
                arg_index = arg_index + 1;
                charsnum = charsnum + size;
                fmt = fmt + 1;
                src = src + size;
            }
            else if (*fmt == 'l')
            {
                fmt = fmt + 1;
                if (*fmt == 'd')
                {
                    long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
                    arg_index = arg_index + 1;
                    charsnum = charsnum + size;
                    fmt = fmt + 1;
                    src = src + size;
                }
                else
                {
                    *src = *fmt;
                    charsnum = charsnum + 1;
                    fmt = fmt + 1;
                    src = src + 1;
                }
            }
            else
            {
                // 未知格式：把 '%' 和随后字符都按字面输出（若后面是 '\0' 则只输出 '%'）
                *src++ = '%';
                charsnum++;
                if (*fmt != '\0')
                {
                    *src++ = *fmt;
                    charsnum++;
                    fmt++;
                }
            }
        }
        else
        {
            *src = *fmt;
            charsnum = charsnum + 1;
            fmt = fmt + 1;
            src = src + 1;
        }
    }
    *src = '\0';
    return charsnum;
}
// ...existing code...
long printf(char* fmt, ...)
{
    long arg_index = 1;
    long charsnum = 0;
    while (*fmt != 0)
    {
        if (*fmt == '%')
        {
            fmt = fmt + 1;
            if (*fmt == 'c')
            {
                write(*load_arg_ptr(&fmt, arg_index));
                arg_index = arg_index + 1;
                charsnum = charsnum + 1;
                fmt = fmt + 1;
            }
            else if (*fmt == 's')
            {
                long size = print_str(*load_arg_ptr(&fmt, arg_index));
                arg_index = arg_index + 1;
                charsnum = charsnum + size;
                fmt = fmt + 1;
            }
            else if (*fmt == 'd')
            {
                char src[12];
                long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
                print_str(src);
                arg_index = arg_index + 1;
                charsnum = charsnum + size;
                fmt = fmt + 1;
            }
            else if (*fmt == 'l')
            {
                fmt = fmt + 1;
                if (*fmt == 'd')
                {
                    char src[12];
                    long size = num_to_str(src, *load_arg_ptr(&fmt, arg_index));
                    print_str(src);
                    arg_index = arg_index + 1;
                    charsnum = charsnum + size;
                    fmt = fmt + 1;
                }
                else
                {
                    write(*fmt);
                    charsnum = charsnum + 1;
                    fmt = fmt + 1;
                }
            }
            else
            {
                // 未知格式：把 '%' 和随后字符都按字面输出（若后面是 '\0' 则只输出 '%'）
                write('%');
                charsnum++;
                if (*fmt != '\0')
                {
                    write(*fmt);
                    charsnum++;
                    fmt++;
                }
            }
        }
        else
        {
            write(*fmt);
            charsnum = charsnum + 1;
            fmt = fmt + 1;
        }
    }
    return charsnum;
}
