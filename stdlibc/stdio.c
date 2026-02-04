#include <def.h>
#include <stdarg.h>
#include <stdio.h>
#include <systemcall.h>
#include <string.h>
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

// 支持 %[width][.precision]s %[width]d %c %ld %p %%
// 例如: %8.4s (宽度8，精度4) %8d (宽度8)
long vsprintf(char* dst, char* fmt, va_list ap)
{
    char* src = dst;
    long charsnum = 0;

    while (*fmt != 0)
    {
        if (*fmt == '%')
        {
            fmt = fmt + 1;

            // 解析标志：左对齐 '-'
            int left_align = 0;
            if (*fmt == '-')
            {
                left_align = 1;
                fmt = fmt + 1;
            }

            // 解析宽度
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9')
            {
                width = width * 10 + (*fmt - '0');
                fmt = fmt + 1;
            }

            // 解析精度
            int precision = -1; // -1 表示未指定
            if (*fmt == '.')
            {
                fmt = fmt + 1;
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9')
                {
                    precision = precision * 10 + (*fmt - '0');
                    fmt = fmt + 1;
                }
            }

            if (*fmt == 'c')
            {
                char ch = va_arg(ap, int); // char 提升为 int
                // 宽度填充
                long pad = 0;
                if (width > 1) { pad = width - 1; }
                if (!left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                *src = ch;
                charsnum = charsnum + 1;
                src = src + 1;
                if (left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                fmt = fmt + 1;
            }
            else if (*fmt == 'd')
            {
                int val = va_arg(ap, int);
                char tmpbuf[32];
                long size = num_to_str(tmpbuf, val);
                tmpbuf[size] = '\0';

                long pad = 0;
                if (width > size) { pad = width - size; }
                if (!left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                for (long i = 0; i < size; i++)
                {
                    *src++ = tmpbuf[i];
                    charsnum++;
                }
                if (left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                fmt = fmt + 1;
            }
            else if (*fmt == 's')
            {
                char* str = va_arg(ap, char*);
                
                // 内联计算字符串长度，不依赖 strlen
                // long slen = 0;
                // char* tmp = str;
                // while (*tmp != '\0') { slen++; tmp++; }
                long slen = strlen(str);

                // precision 限制输出字符数
                long outlen = slen;
                if (precision >= 0 && precision < outlen)
                {
                    outlen = precision;
                }

                // 计算填充数
                long pad = 0;
                if (width > outlen)
                {
                    pad = width - outlen;
                }
                // 右对齐时先填充空格
                if (!left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                // 输出字符串内容
                for (long i = 0; i < outlen; i++)
                {
                    *src++ = str[i];
                    charsnum++;
                }
                // 左对齐时后填充空格
                if (left_align)
                {
                    long p = pad;
                    while (p > 0) { *src++ = ' '; charsnum++; p--; }
                }
                fmt = fmt + 1;
            }
            else if (*fmt == 'l')
            {
                fmt = fmt + 1;
                if (*fmt == 'd')
                {
                    long val = va_arg(ap, long);
                    char tmpbuf[32];
                    long size = num_to_str(tmpbuf, val);
                    tmpbuf[size] = '\0';

                    long pad = 0;
                    if (width > size) { pad = width - size; }
                    if (!left_align)
                    {
                        long p = pad;
                        while (p > 0) { *src++ = ' '; charsnum++; p--; }
                    }
                    for (long i = 0; i < size; i++)
                    {
                        *src++ = tmpbuf[i];
                        charsnum++;
                    }
                    if (left_align)
                    {
                        long p = pad;
                        while (p > 0) { *src++ = ' '; charsnum++; p--; }
                    }
                    fmt = fmt + 1;
                }
                else
                {
                    *src = 'l';
                    src = src + 1;
                    charsnum = charsnum + 1;
                    // 不移动 fmt，让下一轮处理
                }
            }
            else if (*fmt == 'p')
            {
                // 指针：输出为十六进制
                long val = va_arg(ap, long);
                *src++ = '0';
                *src++ = 'x';
                charsnum = charsnum + 2;
                // 简化：用十进制代替
                long size = num_to_str(src, val);
                charsnum = charsnum + size;
                src = src + size;
                fmt = fmt + 1;
            }
            else if (*fmt == '%')
            {
                *src = '%';
                src = src + 1;
                charsnum = charsnum + 1;
                fmt = fmt + 1;
            }
            else
            {
                // 未知格式：把 '%' 和随后字符都按字面输出
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

long sprintf(char* dst, char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    long ret = vsprintf(dst, fmt, ap);
    va_end(ap);
    return ret;
}

long vprintf(char* fmt, va_list ap)
{
    char buf[1024]; // 静态缓冲区
    long len = vsprintf(buf, fmt, ap);
    print_str(buf);
    return len;
}

long printf(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    long ret = vprintf(fmt, ap);
    va_end(ap);
    return ret;
}
FILE* fopen(char* filename, char* modes)
{
    return (FILE*)__open(filename, modes);
}

int fclose(FILE* fp)
{
    return __close((long)fp);
}
int fputc(int c, FILE* fp)
{
    return __putc(c, (long)fp);
}
int fputs(char* s, FILE* fp)
{
    int size = 0;
    for (int i = 0; i < strlen(s); i++)
    {
        if (fputc(s[i], fp) != EOF)
        {
            size++;
        }
        else
        {
            return EOF;
        }
    }
    return size;
}
int fgetc(FILE* fp)
{
    return __getc((long)fp);
}
char* fgets(char* buf, int n, FILE* fp)
{
    if (n <= 0)
    {
        return NULL;
    }
    int i = 0;
    int c;
    while (i < n - 1)
    {
        c = fgetc(fp);
        if (c == EOF)
        {
            if (i == 0)
            {
                return NULL; // 未读到任何字符
            }
            break;
        }
        buf[i] = (char)c;
        i++;
        if (c == '\n')
        {
            break;
        }
    }
    buf[i] = '\0';
    return buf;
}

size_t fread(void* ptr, size_t size_of_elements, size_t number_of_elements, FILE* a_file)
{
    size_t total = size_of_elements * number_of_elements;
    size_t bytes_read = __read((long)a_file, (char*)ptr, (long)total);
    return bytes_read / size_of_elements;
}

size_t fwrite(void* ptr, size_t size_of_elements, size_t number_of_elements, FILE* a_file)
{
    size_t total = size_of_elements * number_of_elements;
    size_t bytes_written = __write_file((long)a_file, (char*)ptr, total);
    return bytes_written / size_of_elements;
}