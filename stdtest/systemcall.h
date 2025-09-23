void write(char in)
{
    _asm_("IMM 16");
    _asm_("LEA");
    _asm_("LW");
    _asm_("SYSTEMCALL 0");
    return;
}