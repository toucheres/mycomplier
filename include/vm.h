#pragma once
struct VCPU
{
    int* code;      // code segment
    int* code_dump; // for dump
    int* stack;     // stack segment
    char* data;     // data segment

    int* pc; // pc register
    int* sp; // rsp register
    int* bp; // rbp register

    int ax; // common register
    int cycle;
};
struct VMEM
{
    char* data = new char[10240];
    ~VMEM();
    char operator[](int addr);
};

struct Execution
{
    /* data */
};

class VM
{
  private:
    VCPU cpu;
  public:
    enum class ASM
    {
        SYSTEMCALL,
        IMM,
        LEA,
        JMP,
        JZ,
        JNZ,
        CALL,
        NVAR,
        DARG,
        RET,
        LI,
        LC,
        SI,
        SC,
        PUSH,
        OR,
        XOR,
        AND,
        EQ,
        NE,
        LT,
        GT,
        LE,
        GE,
        SHL,
        SHR,
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
    };
    enum class systemcall
    {
        OPEN,
        READ,
        CLOS,
        PRTF,
        MALC,
        FREE,
        MSET,
        MCMP,
        EXIT,
    };
    int start()
    {
    }
};