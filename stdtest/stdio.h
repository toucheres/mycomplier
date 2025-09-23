void write(char in)
{
    _asm_("LEA 16");
    _asm_("LW");
    _asm_("SYSTEMCALL 0");
    return;
}