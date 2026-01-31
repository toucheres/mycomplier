#include "./def.h"
#include "./stdarg.h"
#include "./string.h"
#include "./systemcall.h"
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
// 仅支持%s %d %c %ld %p %%
long vsprintf(char* dst, char* fmt, va_list ap)
{
    char* src = dst;
    long charsnum = 0;

    while (*fmt != 0)
    {
        if (*fmt == '%')
        {
            fmt = fmt + 1;
            if (*fmt == 'c')
            {
                char ch = va_arg(ap, int); // char 提升为 int
                *src = ch;
                charsnum = charsnum + 1;
                fmt = fmt + 1;
                src = src + 1;
            }
            else if (*fmt == 'd')
            {
                int val = va_arg(ap, int);
                long size = num_to_str(src, val);
                charsnum = charsnum + size;
                fmt = fmt + 1;
                src = src + size;
            }
            else if (*fmt == 's')
            {
                char* str = va_arg(ap, char*);
                while (*str != '\0')
                {
                    *src = *str;
                    src = src + 1;
                    str = str + 1;
                    charsnum = charsnum + 1;
                }
                fmt = fmt + 1;
            }
            else if (*fmt == 'l')
            {
                fmt = fmt + 1;
                if (*fmt == 'd')
                {
                    long val = va_arg(ap, long);
                    long size = num_to_str(src, val);
                    charsnum = charsnum + size;
                    fmt = fmt + 1;
                    src = src + size;
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

typedef struct FILE_struct
{
    int _flags; /* High-order word is _IO_MAGIC; rest is flags. */

    /* The following pointers correspond to the C++ streambuf protocol. */
    char* _IO_read_ptr;   /* Current read pointer */
    char* _IO_read_end;   /* End of get area. */
    char* _IO_read_base;  /* Start of putback+get area. */
    char* _IO_write_base; /* Start of put area. */
    char* _IO_write_ptr;  /* Current put pointer. */
    char* _IO_write_end;  /* End of put area. */
    char* _IO_buf_base;   /* Start of reserve area. */
    char* _IO_buf_end;    /* End of reserve area. */

    /* The following fields are used to support backing up and undo. */
    char* _IO_save_base;   /* Pointer to start of non-current get area. */
    char* _IO_backup_base; /* Pointer to first valid character of backup area */
    char* _IO_save_end;    /* Pointer to end of non-current get area. */

    long _markers;

    long _chain; // [TODO] 支持struct嵌套定义自身指针

    int _fileno;
    int _flags2;
    long _old_offset; /* This used to be _offset but it's too small.  */

    /* 1+column number of pbase(); 0 is unknown. */
    short _cur_column;
    char _vtable_offset;
    char _shortbuf[1];

    long _lock;
} FILE;

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