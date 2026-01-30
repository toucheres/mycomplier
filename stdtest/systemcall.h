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