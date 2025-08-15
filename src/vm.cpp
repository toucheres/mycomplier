#include "vm.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

VM::VM() : debug_enabled(false), step_count(0)
{
    // 初始化外部函数映射
    external_functions["printf"] = static_cast<int>(systemcall::PRTF);
    external_functions["malloc"] = static_cast<int>(systemcall::MALC);
    external_functions["free"] = static_cast<int>(systemcall::FREE);
    external_functions["exit"] = static_cast<int>(systemcall::EXIT);
}

void VM::load_code(const std::vector<int>& code)
{
    // 保留兼容性，但现在不推荐使用
    cpu.assembly_code.clear();
    // 这里可以添加从int数组转换为string数组的逻辑，但现在不实现
    reset();
}

void VM::load_assembly_vector(const std::vector<std::string>& assembly)
{
    cpu.assembly_code = assembly;
    reset();
}

void VM::load_assembly_stack(std::stack<std::string> assembly_stack)
{
    std::vector<std::string> assembly;

    // 将 stack 转换为 vector (注意顺序是反的)
    std::vector<std::string> temp;
    while (!assembly_stack.empty())
    {
        temp.push_back(assembly_stack.top());
        assembly_stack.pop();
    }

    // 反转顺序以保持正确的执行顺序
    for (auto it = temp.rbegin(); it != temp.rend(); ++it)
    {
        std::cout << *it << '\n';
        assembly.push_back(*it);
    }

    load_assembly_vector(assembly);
}

void VM::load_assembly_string(const std::vector<std::string>& assembly)
{
    // 这个函数现在被弃用，直接使用load_assembly_vector
    load_assembly_vector(assembly);
}

void VM::reset()
{
    cpu.pc = 0;
    cpu.sp = 0;
    cpu.bp = 0;
    cpu.ax = 0;
    cpu.cycle = 0;
    step_count = 0;
    exec.status = Execution::Status::STOPPED;
    exec.exit_code = 0;
    exec.error_message.clear();

    // 清空栈和数据段
    std::fill(cpu.stack.begin(), cpu.stack.end(), 0);
    // std::fill(cpu.data.begin(), cpu.data.end(), 0);

    // 记录重置信息到调试日志
    if (debug_enabled && debug_log.is_open())
    {
        debug_log << "=== VM Reset ===" << std::endl;
        debug_log << std::endl;
    }
}

void VM::push(int value)
{
    if (cpu.sp >= static_cast<int>(cpu.stack.size()))
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Stack overflow";
        return;
    }
    cpu.stack[cpu.sp++] = value;
}

int VM::pop()
{
    if (cpu.sp <= 0)
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Stack underflow";
        return 0;
    }
    return cpu.stack[--cpu.sp];
}

bool VM::check_bounds(int address, int size)
{
    if (address < 0 || address + size > static_cast<int>(cpu.stack.size()))
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Memory access out of bounds: address " + std::to_string(address);
        return false;
    }
    return true;
}

int VM::start()
{
    exec.status = Execution::Status::RUNNING;

    while (exec.status == Execution::Status::RUNNING)
    {
        if (!step())
        {
            break;
        }
    }

    return exec.exit_code;
}

bool VM::step()
{
    if (exec.status != Execution::Status::RUNNING)
    {
        return false;
    }

    if (cpu.pc >= static_cast<int>(cpu.assembly_code.size()))
    {
        exec.status = Execution::Status::STOPPED;
        return false;
    }

    // 记录调试信息（在执行指令之前）
    if (debug_enabled)
    {
        step_count++;
        log_step_info();
    }

    execute_instruction();
    cpu.cycle++;

    return exec.status == Execution::Status::RUNNING;
}

void VM::execute_instruction()
{
    if (cpu.pc >= static_cast<int>(cpu.assembly_code.size()))
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Program counter out of bounds";
        return;
    }

    std::string line = cpu.assembly_code[cpu.pc++];

    // 解析指令和参数
    int space_pos = line.find(' ');
    std::string instruction;
    std::string arg_str;
    int arg = 0;
    bool has_arg = false;

    if (space_pos != std::string::npos)
    {
        instruction = line.substr(0, space_pos);
        arg_str = line.substr(space_pos + 1);
        try
        {
            arg = std::stoi(arg_str);
            has_arg = true;
        }
        catch (const std::exception&)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "Invalid argument format: " + line;
            return;
        }
    }
    else
    {
        instruction = line;
    }

    // 执行指令
    if (instruction == "IMM")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "IMM: Missing immediate value";
            return;
        }
        push(arg);
    }
    else if (instruction == "NVAR")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "IMM: Missing immediate value";
            return;
        }
        for (int i = 0; i < arg; i++)
        {
            push(0);
        }
    }
    else if (instruction == "LEA")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "LEA: Missing address";
            return;
        }
        push(arg + cpu.bp);
    }
    else if (instruction == "UP")
    {
        if (!has_arg)
        {
            arg = pop();
        }
        cpu.bp += arg;
        cpu.sp += arg;
    }
    else if (instruction == "LI")
    {
        int value;
        int addr;
        if (has_arg)
        {
            addr = arg;
            value = *reinterpret_cast<int*>(&cpu.stack[addr]);
            push(value);
            return;
        }
        else
        {
            // 从栈中获取地址
            if (cpu.sp > 0)
            {
                int addr = pop();
                if (check_bounds(addr, 1))
                {
                    int value = *reinterpret_cast<int*>(&cpu.stack[addr]);
                    push(value);
                }
            }
            else
            {
                exec.status = Execution::Status::ERROR;
                exec.error_message = "LI: No address available";
            }
        }
    }
    else if (instruction == "SI")
    {
        int value;
        int addr;
        if (has_arg)
        {
            addr = arg;
            value = pop();
            *reinterpret_cast<int*>(&cpu.stack[addr]) = value;
            return;
        }
        else
        {
            addr = pop();
            value = pop();
            *reinterpret_cast<int*>(&cpu.stack[addr]) = value;
        }
    }
    else if (instruction == "HOLD")
    {
        // exec.status = Execution::Status::WARING;
        // exec.error_message = "HOLD: not been instead!";
    }
    else if (instruction == "ADD")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "ADD: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a + b);
    }
    else if (instruction == "SUB")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "SUB: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a - b);
    }
    else if (instruction == "MUL")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "MUL: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a * b);
    }
    else if (instruction == "DIV")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "DIV: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        if (b == 0)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "Division by zero";
            return;
        }
        push(a / b);
    }
    else if (instruction == "JMP")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "JMP: Missing target address";
            return;
        }
        cpu.pc = arg;
    }
    else if (instruction == "JZ")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "JZ: Missing target address";
            return;
        }
        if (cpu.sp > 0)
        {
            int value = pop();
            if (value == 0)
            {
                cpu.pc = arg;
            }
        }
        else
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "JZ: No value on stack";
        }
    }
    else if (instruction == "JNZ")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "JNZ: Missing target address";
            return;
        }
        if (cpu.sp > 0)
        {
            int value = pop();
            if (value != 0)
            {
                cpu.pc = arg;
            }
        }
        else
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "JNZ: No value on stack";
        }
    }
    else if (instruction == "CALL")
    {
        if (!has_arg)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "CALL: Missing function address";
            return;
        }
        int tp = cpu.bp;
        cpu.bp = cpu.sp;
        push(cpu.pc); // 保存下一条返回地址,pc先自增再执行指令，无需加1
        push(tp);     // 保存旧bp
        cpu.pc = arg;
    }
    // call addr<fun> 压入pc+1 压入bp bp=sp jump-addr<fun>  stack: a   b  ...  opc+1  obp
    //                                                                          bp
    else if (instruction == "RET")
    {
        int tp_retaddr = cpu.stack[cpu.bp];
        int tp_oldbp = cpu.stack[cpu.bp + 1];
        int retv = cpu.ax = pop();
        cpu.pc = tp_retaddr;
        cpu.sp = cpu.bp;
        cpu.bp = tp_oldbp;
        push(retv);
        if (cpu.bp == 0)
        {
            // main函数ret
            exec.exit_code = pop();
            exec.status = Execution::Status::STOPPED;
        }
    }
    else if (instruction == "DARG")
    {
        // 删除参数 - 这里实现为空操作
        // 在实际编译器中，这可能涉及栈指针的调整
    }
    else if (instruction == "EQ")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "EQ: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a == b ? 1 : 0);
    }
    else if (instruction == "NE")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "NE: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a != b ? 1 : 0);
    }
    else if (instruction == "LT")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "LT: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a < b ? 1 : 0);
    }
    else if (instruction == "GT")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "GT: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a > b ? 1 : 0);
    }
    else if (instruction == "LE")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "LE: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a <= b ? 1 : 0);
    }
    else if (instruction == "GE")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "GE: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a >= b ? 1 : 0);
    }
    else if (instruction == "AND")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "AND: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a && b ? 1 : 0);
    }
    else if (instruction == "OR")
    {
        if (cpu.sp < 2)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "OR: Not enough operands";
            return;
        }
        int b = pop();
        int a = pop();
        push(a || b ? 1 : 0);
    }
    else
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Unknown instruction: " + instruction;
    }
}

void VM::handle_syscall(int syscall_id)
{
    systemcall syscall = static_cast<systemcall>(syscall_id);

    switch (syscall)
    {
    case systemcall::PRTF:
    {
        // 简化的 printf 实现
        if (cpu.sp < 1)
        {
            exec.status = Execution::Status::ERROR;
            exec.error_message = "printf: No format string";
            return;
        }
        int format_addr = pop();
        if (check_bounds(format_addr))
        {
            // 简单实现：假设格式字符串是 "%d\n"，打印一个整数
            if (cpu.sp >= 1)
            {
                int value = pop();
                std::cout << value << std::endl;
            }
            else
            {
                std::cout << "(no value)" << std::endl;
            }
        }
        push(0); // printf 返回值
        break;
    }

    case systemcall::EXIT:
    {
        if (cpu.sp >= 1)
        {
            exec.exit_code = pop();
        }
        exec.status = Execution::Status::STOPPED;
        break;
    }

    default:
    {
        exec.status = Execution::Status::ERROR;
        exec.error_message = "Unimplemented system call: " + std::to_string(syscall_id);
        break;
    }
    }
}

void VM::dump_registers() const
{
    std::cout << "=== CPU Registers ===" << std::endl;
    std::cout << "PC: " << cpu.pc << std::endl;
    std::cout << "SP: " << cpu.sp << std::endl;
    std::cout << "BP: " << cpu.bp << std::endl;
    std::cout << "AX: " << cpu.ax << std::endl;
    std::cout << "Cycle: " << cpu.cycle << std::endl;
    std::cout << "Status: ";
    switch (exec.status)
    {
    case Execution::Status::RUNNING:
        std::cout << "RUNNING";
        break;
    case Execution::Status::STOPPED:
        std::cout << "STOPPED";
        break;
    case Execution::Status::ERROR:
        std::cout << "ERROR - " << exec.error_message;
        break;
    case Execution::Status::SYSCALL_PENDING:
        std::cout << "SYSCALL_PENDING";
        break;
    }
    std::cout << std::endl;
}

void VM::dump_stack(int count) const
{
    std::cout << "=== Stack (top " << count << " entries) ===" << std::endl;
    int start = std::max(0, cpu.sp - count);
    for (int i = start; i < cpu.sp; i++)
    {
        std::cout << "[" << std::setw(3) << i << "] " << cpu.stack[i];
        if (i == cpu.sp - 1)
            std::cout << " <- SP";
        if (i == cpu.bp)
            std::cout << " <- BP";
        std::cout << std::endl;
    }
}

void VM::dump_code(int start, int count) const
{
    std::cout << "=== Assembly Code Segment ===" << std::endl;
    int end = std::min(start + count, static_cast<int>(cpu.assembly_code.size()));
    for (int i = start; i < end; i++)
    {
        std::cout << "[" << std::setw(3) << i << "] " << cpu.assembly_code[i];
        if (i == cpu.pc)
            std::cout << " <- PC";
        std::cout << std::endl;
    }
}

void VM::register_external_function(const std::string& name, int address)
{
    external_functions[name] = address;
}

void VM::enable_debug(const std::string& log_filename)
{
    debug_enabled = true;
    step_count = 0;

    if (debug_log.is_open())
    {
        debug_log.close();
    }

    debug_log.open(log_filename, std::ios::out | std::ios::trunc);
    if (debug_log.is_open())
    {
        debug_log << "=== VM Debug Log Started ===" << std::endl;
        debug_log << "Log file: " << log_filename << std::endl;
        debug_log << std::endl;
    }
    else
    {
        std::cerr << "Warning: Could not open debug log file: " << log_filename << std::endl;
        debug_enabled = false;
    }
}

void VM::disable_debug()
{
    if (debug_enabled && debug_log.is_open())
    {
        debug_log << "=== VM Debug Log Ended ===" << std::endl;
        debug_log.close();
    }
    debug_enabled = false;
}

std::string VM::get_instruction_name(int instruction_code)
{
    switch (static_cast<ASM>(instruction_code))
    {
    case ASM::SYSTEMCALL:
        return "SYSTEMCALL";
    case ASM::IMM:
        return "IMM";
    case ASM::LEA:
        return "LEA";
    case ASM::JMP:
        return "JMP";
    case ASM::JZ:
        return "JZ";
    case ASM::JNZ:
        return "JNZ";
    case ASM::CALL:
        return "CALL";
    case ASM::NVAR:
        return "NVAR";
    case ASM::DARG:
        return "DARG";
    case ASM::RET:
        return "RET";
    case ASM::LI:
        return "LI";
    case ASM::LC:
        return "LC";
    case ASM::SI:
        return "SI";
    case ASM::SC:
        return "SC";
    case ASM::PUSH:
        return "PUSH";
    case ASM::OR:
        return "OR";
    case ASM::XOR:
        return "XOR";
    case ASM::AND:
        return "AND";
    case ASM::EQ:
        return "EQ";
    case ASM::NE:
        return "NE";
    case ASM::LT:
        return "LT";
    case ASM::GT:
        return "GT";
    case ASM::LE:
        return "LE";
    case ASM::GE:
        return "GE";
    case ASM::SHL:
        return "SHL";
    case ASM::SHR:
        return "SHR";
    case ASM::ADD:
        return "ADD";
    case ASM::SUB:
        return "SUB";
    case ASM::MUL:
        return "MUL";
    case ASM::DIV:
        return "DIV";
    case ASM::MOD:
        return "MOD";
    default:
        return "UNKNOWN(" + std::to_string(instruction_code) + ")";
    }
}

void VM::log_step_info()
{
    if (!debug_enabled || !debug_log.is_open())
    {
        return;
    }

    debug_log << "=== Step " << step_count << " ===" << std::endl;

    // 记录即将执行的指令
    if (cpu.pc < static_cast<int>(cpu.assembly_code.size()))
    {
        debug_log << "Next Instruction: [" << cpu.pc << "] " << cpu.assembly_code[cpu.pc]
                  << std::endl;
    }

    // 记录CPU寄存器状态
    debug_log << "CPU Registers:" << std::endl;
    debug_log << "  PC: " << cpu.pc << std::endl;
    debug_log << "  SP: " << cpu.sp << std::endl;
    debug_log << "  BP: " << cpu.bp << std::endl;
    debug_log << "  AX: " << cpu.ax << std::endl;
    debug_log << "  Cycle: " << cpu.cycle << std::endl;

    // 记录执行状态
    debug_log << "Execution Status: ";
    switch (exec.status)
    {
    case Execution::Status::RUNNING:
        debug_log << "RUNNING";
        break;
    case Execution::Status::STOPPED:
        debug_log << "STOPPED";
        break;
    case Execution::Status::ERROR:
        debug_log << "ERROR - " << exec.error_message;
        break;
    case Execution::Status::SYSCALL_PENDING:
        debug_log << "SYSCALL_PENDING";
        break;
    }
    debug_log << std::endl;

    // 记录栈状态 (显示前10个元素)
    debug_log << "Stack (top 10 entries):" << std::endl;
    if (cpu.sp == 0)
    {
        debug_log << "  (empty)" << std::endl;
    }
    else
    {
        int start = std::max(0, cpu.sp - 10);
        for (int i = start; i < cpu.sp; i++)
        {
            debug_log << "  [" << std::setw(3) << i << "] " << cpu.stack[i];
            if (i == cpu.sp - 1)
                debug_log << " <- SP";
            if (i == cpu.bp)
                debug_log << " <- BP";
            debug_log << std::endl;
        }
    }

    // 记录内存状态 (显示前16个字节的数据段)
    // debug_log << "Data Segment (first 16 bytes):" << std::endl;
    // for (int i = 0; i < 16 && i < static_cast<int>(cpu.data.size()); i += 4)
    // {
    //     if (i + 3 < static_cast<int>(cpu.data.size()))
    //     {
    //         int value = *reinterpret_cast<int*>(&cpu.data[i]);
    //         debug_log << "  [" << std::setw(3) << i << "] " << value << std::endl;
    //     }
    // }

    debug_log << std::endl;
}