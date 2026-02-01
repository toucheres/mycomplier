#ifndef STDARG_H
#define STDARG_H
long align_up(long num, long align);
char* va_arg_fun(char** ap, int size);
#define va_list char*

#define va_start(ap, last_named_arg)                                                               \
    (ap = (char*)((long)&last_named_arg + align_up(sizeof(last_named_arg), 8)))

#define va_arg(ap, type) (*((type*)va_arg_fun(&ap, sizeof(type))))

#define va_copy(dest, src) ((dest) = (src))

#define va_end(ap) 0
#endif