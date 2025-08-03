#include "vm.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>

VM::VM() {
    // 初始化外部函数映射
    external_functions["printf"] = static_cast<int>(systemcall::PRTF);
    external_functions["malloc"] = static_cast<int>(systemcall::MALC);
    external_functions["free"] = static_cast<int>(systemcall::FREE);
    external_functions["exit"] = static_cast<int>(systemcall::EXIT);
}

void VM::load_code(const std::vector<int>& code) {
    cpu.code = code;
    reset();
}

void VM::load_assembly_stack(std::stack<std::string> assembly_stack) {
    std::vector<std::string> assembly;
    
    // 将 stack 转换为 vector (注意顺序是反的)
    std::vector<std::string> temp;
    while (!assembly_stack.empty()) {
        temp.push_back(assembly_stack.top());
        assembly_stack.pop();
    }
    
    // 反转顺序以保持正确的执行顺序
    for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
        assembly.push_back(*it);
    }
    
    load_assembly_string(assembly);
}

void VM::load_assembly_string(const std::vector<std::string>& assembly) {
    cpu.code.clear();
    
    for (const auto& line : assembly) {
        // 尝试解析指令
        if (line == "SYSTEMCALL") cpu.code.push_back(static_cast<int>(ASM::SYSTEMCALL));
        else if (line == "IMM") cpu.code.push_back(static_cast<int>(ASM::IMM));
        else if (line == "LEA") cpu.code.push_back(static_cast<int>(ASM::LEA));
        else if (line == "JMP") cpu.code.push_back(static_cast<int>(ASM::JMP));
        else if (line == "JZ") cpu.code.push_back(static_cast<int>(ASM::JZ));
        else if (line == "JNZ") cpu.code.push_back(static_cast<int>(ASM::JNZ));
        else if (line == "CALL") cpu.code.push_back(static_cast<int>(ASM::CALL));
        else if (line == "NVAR") cpu.code.push_back(static_cast<int>(ASM::NVAR));
        else if (line == "DARG") cpu.code.push_back(static_cast<int>(ASM::DARG));
        else if (line == "RET") cpu.code.push_back(static_cast<int>(ASM::RET));
        else if (line == "LI") cpu.code.push_back(static_cast<int>(ASM::LI));
        else if (line == "LC") cpu.code.push_back(static_cast<int>(ASM::LC));
        else if (line == "SI") cpu.code.push_back(static_cast<int>(ASM::SI));
        else if (line == "SC") cpu.code.push_back(static_cast<int>(ASM::SC));
        else if (line == "PUSH") cpu.code.push_back(static_cast<int>(ASM::PUSH));
        else if (line == "OR") cpu.code.push_back(static_cast<int>(ASM::OR));
        else if (line == "XOR") cpu.code.push_back(static_cast<int>(ASM::XOR));
        else if (line == "AND") cpu.code.push_back(static_cast<int>(ASM::AND));
        else if (line == "EQ") cpu.code.push_back(static_cast<int>(ASM::EQ));
        else if (line == "NE") cpu.code.push_back(static_cast<int>(ASM::NE));
        else if (line == "LT") cpu.code.push_back(static_cast<int>(ASM::LT));
        else if (line == "GT") cpu.code.push_back(static_cast<int>(ASM::GT));
        else if (line == "LE") cpu.code.push_back(static_cast<int>(ASM::LE));
        else if (line == "GE") cpu.code.push_back(static_cast<int>(ASM::GE));
        else if (line == "SHL") cpu.code.push_back(static_cast<int>(ASM::SHL));
        else if (line == "SHR") cpu.code.push_back(static_cast<int>(ASM::SHR));
        else if (line == "ADD") cpu.code.push_back(static_cast<int>(ASM::ADD));
        else if (line == "SUB") cpu.code.push_back(static_cast<int>(ASM::SUB));
        else if (line == "MUL") cpu.code.push_back(static_cast<int>(ASM::MUL));
        else if (line == "DIV") cpu.code.push_back(static_cast<int>(ASM::DIV));
        else if (line == "MOD") cpu.code.push_back(static_cast<int>(ASM::MOD));
        else {
            // 尝试解析为数字（立即数）
            try {
                int value = std::stoi(line);
                cpu.code.push_back(value);
            } catch (const std::exception&) {
                // 未知指令或格式错误
                std::cerr << "Warning: Unknown instruction or invalid format: " << line << std::endl;
            }
        }
    }
    
    reset();
}

void VM::reset() {
    cpu.pc = 0;
    cpu.sp = 0;
    cpu.bp = 0;
    cpu.ax = 0;
    cpu.cycle = 0;
    exec.status = Execution::Status::STOPPED;
    exec.exit_code = 0;
    exec.error_message.clear();
    
    // 清空栈和数据段
    std::fill(cpu.stack.begin(), cpu.stack.end(), 0);
    std::fill(cpu.data.begin(), cpu.data.end(), 0);
}

void VM::push(int value) {
    if (cpu.sp >= static_cast<int>(cpu.stack.size())) {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Stack overflow";
        return;
    }
    cpu.stack[cpu.sp++] = value;
}

int VM::pop() {
    if (cpu.sp <= 0) {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Stack underflow";
        return 0;
    }
    return cpu.stack[--cpu.sp];
}

bool VM::check_bounds(int address, int size) {
    if (address < 0 || address + size > static_cast<int>(cpu.data.size())) {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Memory access out of bounds: address " + std::to_string(address);
        return false;
    }
    return true;
}

int VM::start() {
    exec.status = Execution::Status::RUNNING;
    
    while (exec.status == Execution::Status::RUNNING) {
        if (!step()) {
            break;
        }
    }
    
    return exec.exit_code;
}

bool VM::step() {
    if (exec.status != Execution::Status::RUNNING) {
        return false;
    }
    
    if (cpu.pc >= static_cast<int>(cpu.code.size())) {
        exec.status = Execution::Status::STOPPED;
        return false;
    }
    
    execute_instruction();
    cpu.cycle++;
    
    return exec.status == Execution::Status::RUNNING;
}

void VM::execute_instruction() {
    if (cpu.pc >= static_cast<int>(cpu.code.size())) {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Program counter out of bounds";
        return;
    }
    
    ASM instruction = static_cast<ASM>(cpu.code[cpu.pc++]);
    
    switch (instruction) {
        case ASM::IMM: {
            // 立即数压栈
            if (cpu.pc >= static_cast<int>(cpu.code.size())) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "Missing immediate value";
                return;
            }
            int value = cpu.code[cpu.pc++];
            push(value);
            break;
        }
        
        case ASM::LEA: {
            // 加载有效地址
            if (cpu.pc >= static_cast<int>(cpu.code.size())) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "Missing address for LEA";
                return;
            }
            int addr = cpu.code[cpu.pc++];
            push(addr);
            break;
        }
        
        case ASM::LI: {
            // 从地址加载整数
            if (cpu.sp <= 0) {
                // 如果有立即地址参数
                if (cpu.pc < static_cast<int>(cpu.code.size())) {
                    int addr = cpu.code[cpu.pc++];
                    if (check_bounds(addr, sizeof(int))) {
                        int value = *reinterpret_cast<int*>(&cpu.data[addr]);
                        push(value);
                    }
                } else {
                    exec.status = Execution::Status::ERROR;
                    exec.error_message = "LI: No address on stack or as parameter";
                }
            } else {
                int addr = pop();
                if (check_bounds(addr, sizeof(int))) {
                    int value = *reinterpret_cast<int*>(&cpu.data[addr]);
                    push(value);
                }
            }
            break;
        }
        
        case ASM::SI: {
            // 存储整数到地址
            if (cpu.sp < 1) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "SI: Not enough values on stack";
                return;
            }
            
            int value = pop();
            int addr;
            
            if (cpu.sp > 0) {
                addr = pop();
            } else if (cpu.pc < static_cast<int>(cpu.code.size())) {
                addr = cpu.code[cpu.pc++];
            } else {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "SI: No address available";
                return;
            }
            
            if (check_bounds(addr, sizeof(int))) {
                *reinterpret_cast<int*>(&cpu.data[addr]) = value;
            }
            break;
        }
        
        case ASM::ADD: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "ADD: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a + b);
            break;
        }
        
        case ASM::SUB: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "SUB: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a - b);
            break;
        }
        
        case ASM::MUL: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "MUL: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a * b);
            break;
        }
        
        case ASM::DIV: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "DIV: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            if (b == 0) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "Division by zero";
                return;
            }
            push(a / b);
            break;
        }
        
        case ASM::MOD: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "MOD: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            if (b == 0) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "Modulo by zero";
                return;
            }
            push(a % b);
            break;
        }
        
        case ASM::EQ: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "EQ: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a == b ? 1 : 0);
            break;
        }
        
        case ASM::NE: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "NE: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a != b ? 1 : 0);
            break;
        }
        
        case ASM::LT: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "LT: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a < b ? 1 : 0);
            break;
        }
        
        case ASM::GT: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "GT: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a > b ? 1 : 0);
            break;
        }
        
        case ASM::LE: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "LE: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a <= b ? 1 : 0);
            break;
        }
        
        case ASM::GE: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "GE: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a >= b ? 1 : 0);
            break;
        }
        
        case ASM::AND: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "AND: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a && b ? 1 : 0);
            break;
        }
        
        case ASM::OR: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "OR: Not enough operands";
                return;
            }
            int b = pop();
            int a = pop();
            push(a || b ? 1 : 0);
            break;
        }
        
        case ASM::JMP: {
            if (cpu.pc >= static_cast<int>(cpu.code.size())) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "JMP: Missing target address";
                return;
            }
            cpu.pc = cpu.code[cpu.pc];
            break;
        }
        
        case ASM::JZ: {
            if (cpu.sp < 1 || cpu.pc >= static_cast<int>(cpu.code.size())) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "JZ: Missing condition or target address";
                return;
            }
            int target = cpu.code[cpu.pc++];
            int condition = pop();
            if (condition == 0) {
                cpu.pc = target;
            }
            break;
        }
        
        case ASM::JNZ: {
            if (cpu.sp < 1 || cpu.pc >= static_cast<int>(cpu.code.size())) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "JNZ: Missing condition or target address";
                return;
            }
            int target = cpu.code[cpu.pc++];
            int condition = pop();
            if (condition != 0) {
                cpu.pc = target;
            }
            break;
        }
        
        case ASM::CALL: {
            // 保存返回地址
            push(cpu.pc + 1);
            push(cpu.bp);
            cpu.bp = cpu.sp;
            
            if (cpu.pc < static_cast<int>(cpu.code.size())) {
                int target = cpu.code[cpu.pc++];
                cpu.pc = target;
            } else {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "CALL: Missing target address";
            }
            break;
        }
        
        case ASM::RET: {
            if (cpu.sp < 2) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "RET: Stack corruption";
                return;
            }
            
            // 恢复栈帧
            cpu.sp = cpu.bp;
            cpu.bp = pop();
            cpu.pc = pop();
            break;
        }
        
        case ASM::SYSTEMCALL: {
            if (cpu.sp < 1) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "SYSTEMCALL: No syscall ID on stack";
                return;
            }
            int syscall_id = pop();
            handle_syscall(syscall_id);
            break;
        }
        
        case ASM::PUSH: {
            if (cpu.sp < 1) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "PUSH: Nothing to push";
                return;
            }
            // PUSH 指令本身不做任何事，值已经在栈上
            break;
        }
        
        default: {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "Unknown instruction: " + std::to_string(static_cast<int>(instruction));
            break;
        }
    }
}

void VM::handle_syscall(int syscall_id) {
    systemcall syscall = static_cast<systemcall>(syscall_id);
    
    switch (syscall) {
        case systemcall::PRTF: {
            // 简化的 printf 实现
            if (cpu.sp < 1) {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "printf: No format string";
                return;
            }
            int format_addr = pop();
            if (check_bounds(format_addr)) {
                // 简单实现：假设格式字符串是 "%d\n"，打印一个整数
                if (cpu.sp >= 1) {
                    int value = pop();
                    std::cout << value << std::endl;
                } else {
                    std::cout << "(no value)" << std::endl;
                }
            }
            push(0); // printf 返回值
            break;
        }
        
        case systemcall::EXIT: {
            if (cpu.sp >= 1) {
                exec.exit_code = pop();
            }
            exec.status = Execution::Status::STOPPED;
            break;
        }
        
        default: {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "Unimplemented system call: " + std::to_string(syscall_id);
            break;
        }
    }
}

void VM::dump_registers() const {
    std::cout << "=== CPU Registers ===" << std::endl;
    std::cout << "PC: " << cpu.pc << std::endl;
    std::cout << "SP: " << cpu.sp << std::endl;
    std::cout << "BP: " << cpu.bp << std::endl;
    std::cout << "AX: " << cpu.ax << std::endl;
    std::cout << "Cycle: " << cpu.cycle << std::endl;
    std::cout << "Status: ";
    switch (exec.status) {
        case Execution::Status::RUNNING: std::cout << "RUNNING"; break;
        case Execution::Status::STOPPED: std::cout << "STOPPED"; break;
        case Execution::Status::ERROR: std::cout << "ERROR - " << exec.error_message; break;
        case Execution::Status::SYSCALL_PENDING: std::cout << "SYSCALL_PENDING"; break;
    }
    std::cout << std::endl;
}

void VM::dump_stack(int count) const {
    std::cout << "=== Stack (top " << count << " entries) ===" << std::endl;
    int start = std::max(0, cpu.sp - count);
    for (int i = start; i < cpu.sp; i++) {
        std::cout << "[" << std::setw(3) << i << "] " << cpu.stack[i];
        if (i == cpu.sp - 1) std::cout << " <- SP";
        if (i == cpu.bp) std::cout << " <- BP";
        std::cout << std::endl;
    }
}

void VM::dump_code(int start, int count) const {
    std::cout << "=== Code Segment ===" << std::endl;
    int end = std::min(start + count, static_cast<int>(cpu.code.size()));
    for (int i = start; i < end; i++) {
        std::cout << "[" << std::setw(3) << i << "] " << cpu.code[i];
        if (i == cpu.pc) std::cout << " <- PC";
        std::cout << std::endl;
    }
}

void VM::register_external_function(const std::string& name, int address) {
    external_functions[name] = address;
}