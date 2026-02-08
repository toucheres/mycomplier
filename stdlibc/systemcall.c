#include "systemcall.h"
char* __malloc(char in)
{
    char* ptr;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 16");
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 1");
    asm("DARG 1");
    asm("PUSH");
    asm("SW");
    return ptr;
}
void __free(char* ptr)
{
    asm("IMM 16");
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 2");
    asm("POP");
    return;
}
void __write(char in)
{
    asm("IMM 16");
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 0");
    asm("POP");
    return;
}
long __open(char* file, char* mode)
{
    long ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 24"); // mode
    asm("LEA");
    asm("LW");
    asm("IMM 16"); // file
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 4");
    asm("DARG 2");
    asm("PUSH");
    asm("SW");
    return ret;
}

int __close(long fp)
{
    int ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 16"); // fp
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 5");
    asm("DARG 1");
    asm("PUSH");
    asm("SI");
    return ret;
}

int __putc(int c, long fp)
{
    int ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 24"); // fp
    asm("LEA");
    asm("LW");
    asm("IMM 16"); // c
    asm("LEA");
    asm("LI");
    asm("SYSTEMCALL 6");
    asm("DARG 2");
    asm("PUSH");
    asm("SI");
    return ret;
}

int __getc(long fp)
{
    int ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 16"); // fp
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 7");
    asm("DARG 1");
    asm("PUSH");
    asm("SI");
    return ret;
}

long __read(long fp, char* buf, long size)
{
    long ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 32"); // size
    asm("LEA");
    asm("LW");
    asm("IMM 24"); // buf
    asm("LEA");
    asm("LW");
    asm("IMM 16"); // fp
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 8");
    asm("DARG 3");
    asm("PUSH");
    asm("SW");
    return ret;
}

long __write_file(long fp, char* buf, long size)
{
    long ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 32"); // size
    asm("LEA");
    asm("LW");
    asm("IMM 24"); // buf
    asm("LEA");
    asm("LW");
    asm("IMM 16"); // fp
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 9");
    asm("DARG 3");
    asm("PUSH");
    asm("SW");
    return ret;
}
void __exit(long code)
{
    asm("IMM 16"); // code
    asm("LEA");
    asm("LW");
    asm("POP");
    asm("EXIT");
}

char* __memcpy(char* dest, char* src, long n)
{
    char* ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 32"); // n
    asm("LEA");
    asm("LW");
    asm("IMM 24"); // src
    asm("LEA");
    asm("LW");
    asm("IMM 16"); // dest
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 11");
    asm("DARG 3");
    asm("PUSH");
    asm("SW");
    return ret;
}

char* __memset(char* s, int c, long n)
{
    char* ret;
    asm("IMM -8");
    asm("LEA");
    asm("IMM 32"); // n
    asm("LEA");
    asm("LW");
    asm("IMM 24"); // c
    asm("LEA");
    asm("LI");
    asm("IMM 16"); // s
    asm("LEA");
    asm("LW");
    asm("SYSTEMCALL 12");
    asm("DARG 3");
    asm("PUSH");
    asm("SW");
    return ret;
}