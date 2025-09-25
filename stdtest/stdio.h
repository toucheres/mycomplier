#include <stdarg.h>
#include <systemcall.h>
void print_str(char* str)
{
    while (*str != '\0')
    {
        __write(*str);
        str = str + 1;
    }
    return;
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

// 新增：printf，直接用 __write 输出字符
long printf(char* fmt, ...)
{
    long argindex = 1;
    long chars = 0;

    while (*fmt != '\0')
    {
        if (*fmt == '%')
        {
            fmt++;
            if (*fmt == '\0')
                break;

            // 处理 "%%" -> 输出单个 '%'
            if (*fmt == '%')
            {
                __write('%');
                chars++;
                fmt = fmt + 1;
                continue;
            }

            if (*fmt == 'c')
            {
                long c = *load_arg_ptr(&fmt, argindex);
                argindex = argindex + 1;
                __write(c);
                chars++;
                fmt = fmt + 1;
            }
            else if (*fmt == 'd')
            {
                long v = *load_arg_ptr(&fmt, argindex); // 按 long 处理
                argindex = argindex + 1;
                char buf[32];
                long n = num_to_str(buf, v);
                for (long i = 0; i < n; ++i)
                {
                    __write(buf[i]);
                }
                chars += n;
                fmt = fmt + 1;
            }
            else if (*fmt == 'l')
            {
                fmt = fmt + 1;
                if (*fmt == 'd')
                {
                    long v = *load_arg_ptr(&fmt, argindex);
                    argindex = argindex + 1;
                    char buf[32];
                    long n = num_to_str(buf, v);
                    for (long i = 0; i < n; ++i)
                    {
                        __write(buf[i]);
                    }
                    chars += n;
                    fmt = fmt + 1;
                }
                else
                {
                    // 未知 'l' 后缀，按字面输出 "%lX"
                    __write('%');
                    chars++;
                    __write('l');
                    chars++;
                    if (*fmt != '\0')
                    {
                        __write(*fmt);
                        chars++;
                        fmt = fmt + 1;
                    }
                }
            }
            else if (*fmt == 's')
            {
                char* s = *load_arg_ptr(&fmt, argindex);
                argindex = argindex + 1;
                while (s && *s)
                {
                    __write(*s);
                    chars++;
                    s++;
                }
                fmt = fmt + 1;
            }
            else
            {
                // 未知格式，按字面输出 "%x"
                __write('%');
                chars++;
                if (*fmt != '\0')
                {
                    __write(*fmt);
                    chars++;
                    fmt = fmt + 1;
                }
            }
        }
        else
        {
            __write(*fmt);
            chars++;
            fmt = fmt + 1;
        }
        fmt = fmt + 1;
    }
    return chars;
}
// ...existing code...