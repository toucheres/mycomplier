#include "complier.hpp"
#include <iostream>

void obj::pushASM(VM::ASM asm_code) {
    switch (asm_code) {
        case VM::ASM::SYSTEMCALL: content.push("SYSTEMCALL"); break;
        case VM::ASM::IMM: content.push("IMM"); break;
        case VM::ASM::LEA: content.push("LEA"); break;
        case VM::ASM::JMP: content.push("JMP"); break;
        case VM::ASM::JZ: content.push("JZ"); break;
        case VM::ASM::JNZ: content.push("JNZ"); break;
        case VM::ASM::CALL: content.push("CALL"); break;
        case VM::ASM::NVAR: content.push("NVAR"); break;
        case VM::ASM::DARG: content.push("DARG"); break;
        case VM::ASM::RET: content.push("RET"); break;
        case VM::ASM::LI: content.push("LI"); break;
        case VM::ASM::LC: content.push("LC"); break;
        case VM::ASM::SI: content.push("SI"); break;
        case VM::ASM::SC: content.push("SC"); break;
        case VM::ASM::PUSH: content.push("PUSH"); break;
        case VM::ASM::OR: content.push("OR"); break;
        case VM::ASM::XOR: content.push("XOR"); break;
        case VM::ASM::AND: content.push("AND"); break;
        case VM::ASM::EQ: content.push("EQ"); break;
        case VM::ASM::NE: content.push("NE"); break;
        case VM::ASM::LT: content.push("LT"); break;
        case VM::ASM::GT: content.push("GT"); break;
        case VM::ASM::LE: content.push("LE"); break;
        case VM::ASM::GE: content.push("GE"); break;
        case VM::ASM::SHL: content.push("SHL"); break;
        case VM::ASM::SHR: content.push("SHR"); break;
        case VM::ASM::ADD: content.push("ADD"); break;
        case VM::ASM::SUB: content.push("SUB"); break;
        case VM::ASM::MUL: content.push("MUL"); break;
        case VM::ASM::DIV: content.push("DIV"); break;
        case VM::ASM::MOD: content.push("MOD"); break;
        default: content.push("UNKNOWN"); break;
    }
}

void obj::pushASM(VM::ASM asm_code, int arg) {
    content.push(std::to_string(arg));
    pushASM(asm_code);
}

void obj::pushASM(VM::ASM asm_code, int src, int dest) {
    content.push(std::to_string(dest));
    content.push(std::to_string(src));
    pushASM(asm_code);
}
