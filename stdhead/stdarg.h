#ifndef STDARG_H
#define STDARG_H
// x对齐到y整数倍
#define roundup(x, y)   ((((x) + (y - 1)) / y) * y)

#define va_list char*

#define va_start(ap, last_named_arg)                                                               \
    (ap = (char*)((long)&last_named_arg + roundup(sizeof(last_named_arg), 8)))

#define va_arg(ap, type) ((ap += roundup(sizeof(type), 8)), *(type*)(ap - roundup(sizeof(type), 8)))

#define va_copy(dest, src) ((dest) = (src))

#define va_end(ap) 0
#endif