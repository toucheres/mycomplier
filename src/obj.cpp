#include "complier.hpp"
#include <iostream>

void obj::pushASM(VM::ASM asm_code)
{
    switch (asm_code)
    {
    case VM::ASM::SYSTEMCALL:
        content.push_back("SYSTEMCALL");
        break;
    case VM::ASM::IMM:
        content.push_back("IMM");
        break;
    case VM::ASM::LEA:
        content.push_back("LEA");
        break;
    case VM::ASM::JMP:
        content.push_back("JMP");
        break;
    case VM::ASM::JZ:
        content.push_back("JZ");
        break;
    case VM::ASM::JNZ:
        content.push_back("JNZ");
        break;
    case VM::ASM::CALL:
        content.push_back("CALL");
        break;
    case VM::ASM::NVAR:
        content.push_back("NVAR");
        break;
    case VM::ASM::DARG:
        content.push_back("DARG");
        break;
    case VM::ASM::RET:
        content.push_back("RET");
        break;
    case VM::ASM::LI:
        content.push_back("LI");
        break;
    case VM::ASM::LC:
        content.push_back("LC");
        break;
    case VM::ASM::SI:
        content.push_back("SI");
        break;
    case VM::ASM::SC:
        content.push_back("SC");
        break;
    case VM::ASM::PUSH:
        content.push_back("PUSH");
        break;
    case VM::ASM::OR:
        content.push_back("OR");
        break;
    case VM::ASM::XOR:
        content.push_back("XOR");
        break;
    case VM::ASM::AND:
        content.push_back("AND");
        break;
    case VM::ASM::EQ:
        content.push_back("EQ");
        break;
    case VM::ASM::NE:
        content.push_back("NE");
        break;
    case VM::ASM::LT:
        content.push_back("LT");
        break;
    case VM::ASM::GT:
        content.push_back("GT");
        break;
    case VM::ASM::LE:
        content.push_back("LE");
        break;
    case VM::ASM::GE:
        content.push_back("GE");
        break;
    case VM::ASM::SHL:
        content.push_back("SHL");
        break;
    case VM::ASM::SHR:
        content.push_back("SHR");
        break;
    case VM::ASM::ADD:
        content.push_back("ADD");
        break;
    case VM::ASM::SUB:
        content.push_back("SUB");
        break;
    case VM::ASM::MUL:
        content.push_back("MUL");
        break;
    case VM::ASM::DIV:
        content.push_back("DIV");
        break;
    case VM::ASM::MOD:
        content.push_back("MOD");
        break;
    default:
        content.push_back("UNKNOWN");
        break;
    }
}

void obj::pushASM(VM::ASM asm_code, int arg)
{
    switch (asm_code)
    {
    case VM::ASM::IMM:
        content.push_back("IMM " + std::to_string(arg));
        break;
    case VM::ASM::LEA:
        content.push_back("LEA " + std::to_string(arg));
        break;
    case VM::ASM::JMP:
        content.push_back("JMP " + std::to_string(arg));
        break;
    case VM::ASM::JZ:
        content.push_back("JZ " + std::to_string(arg));
        break;
    case VM::ASM::JNZ:
        content.push_back("JNZ " + std::to_string(arg));
        break;
    case VM::ASM::CALL:
        content.push_back("CALL " + std::to_string(arg));
        break;
    case VM::ASM::LI:
        content.push_back("LI " + std::to_string(arg));
        break;
    case VM::ASM::NVAR:
        content.push_back("LI " + std::to_string(arg));
        break;
    case VM::ASM::LC:
        content.push_back("LC " + std::to_string(arg));
        break;
    case VM::ASM::SI:
        content.push_back("SI " + std::to_string(arg));
        break;
    case VM::ASM::SC:
        content.push_back("SC " + std::to_string(arg));
        break;
    default:
        // 对于其他指令，分别压入参数和指令
        pushASM(asm_code);
        content.push_back(std::to_string(arg));
        break;
    }
}

void obj::pushASM(VM::ASM asm_code, int src, int dest)
{
    content.push_back(std::to_string(dest));
    content.push_back(std::to_string(src));
    pushASM(asm_code);
}

const std::vector<std::string>& obj::get_assembly_vector() const
{
    // 现在content已经是vector了，直接返回引用
    return content;
}
