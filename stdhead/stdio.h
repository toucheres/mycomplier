#include "./stdarg.h"
#include "./string.h"
void write(long arg);
long print_str(char* str);
long num_to_str(char* src, long num);
// 仅支持%s %d %c %ld %p %%
long vsprintf(char* dst, char* fmt, va_list ap);

long sprintf(char* dst, char* fmt, ...);

long vprintf(char* fmt, va_list ap);
long printf(char* fmt, ...);
// copy from glibc [TODO] mutiplatform
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

FILE* fopen(char* filename, char* modes);

int fclose(FILE* fp);
int fputc(int c, FILE* fp);
int fputs(char* s, FILE* fp);
int fgetc(FILE* fp);
char* fgets(char* buf, int n, FILE* fp);

size_t fread(void* ptr, size_t size_of_elements, size_t number_of_elements, FILE* a_file);

size_t fwrite(void* ptr, size_t size_of_elements, size_t number_of_elements, FILE* a_file);