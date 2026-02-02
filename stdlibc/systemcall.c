#include "systemcall.h"
char* __malloc(char in)
{
    char* ptr;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 16");
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 1");
    _asm_("DARG 1");
    _asm_("PUSH");
    _asm_("SW");
    return ptr;
}
void __free(char* ptr)
{
    _asm_("IMM 16");
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 2");
    _asm_("POP");
    return;
}
void __write(char in)
{
    _asm_("IMM 16");
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 0");
    _asm_("POP");
    return;
}
long __open(char* file, char* mode)
{
    long ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 24"); // mode
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 16"); // file
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 4");
    _asm_("DARG 2");
    _asm_("PUSH");
    _asm_("SW");
    return ret;
}

int __close(long fp)
{
    int ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 16"); // fp
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 5");
    _asm_("DARG 1");
    _asm_("PUSH");
    _asm_("SI");
    return ret;
}

int __putc(int c, long fp)
{
    int ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 24"); // fp
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 16"); // c
    _asm_("LEA");
    _asm_("LI");
    _asm_("SYSTEMCALL 6");
    _asm_("DARG 2");
    _asm_("PUSH");
    _asm_("SI");
    return ret;
}

int __getc(long fp)
{
    int ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 16"); // fp
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 7");
    _asm_("DARG 1");
    _asm_("PUSH");
    _asm_("SI");
    return ret;
}

long __read(long fp, char* buf, long size)
{
    long ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 32"); // size
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 24"); // buf
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 16"); // fp
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 8");
    _asm_("DARG 3");
    _asm_("PUSH");
    _asm_("SW");
    return ret;
}

long __write_file(long fp, char* buf, long size)
{
    long ret;
    _asm_("IMM -8");
    _asm_("LEA");
    _asm_("IMM 32"); // size
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 24"); // buf
    _asm_("LEA");
    _asm_("LW");
    _asm_("IMM 16"); // fp
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 9");
    _asm_("DARG 3");
    _asm_("PUSH");
    _asm_("SW");
    return ret;
}
void __exit(long code)
{
    _asm_("IMM 16"); // code
    _asm_("LEA");
    _asm_("LW");
    _asm_("POP");
    _asm_("EXIT");
}