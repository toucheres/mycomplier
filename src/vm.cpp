#include "vm.h"
VM::VM(const std::vector<std::string>& asms)
{
    vcpu.asms = asms;
    vcpu.systemcall_table[VM::systemcall::WRITE] = [](VCPU<>& thiscpu)
    {
        char tp = *thiscpu.sp;
        std::cout << tp;
    };
    vcpu.systemcall_table[VM::systemcall::MALLOC] = [](VCPU<>& thiscpu)
    {
        long tp = *thiscpu.sp;
        *thiscpu.ax = (long)malloc(tp);
    };
    vcpu.systemcall_table[VM::systemcall::FREE] = [](VCPU<>& thiscpu)
    {
        long tp = *thiscpu.sp;
        free((void*)tp);
    };
    vcpu.systemcall_table[VM::systemcall::BREAKPOINT] = [this](VCPU<>& thiscpu) { debug(); };
    vcpu.systemcall_table[VM::systemcall::OPEN] = [](VCPU<>& thiscpu)
    {
        long fileptr = *thiscpu.sp;
        long modeptr = *(thiscpu.sp + 1);
        *thiscpu.ax = (long)fopen((char*)fileptr, (char*)modeptr);
    };
    vcpu.systemcall_table[VM::systemcall::CLOSE] = [](VCPU<>& thiscpu)
    {
        long fp = *thiscpu.sp;
        *thiscpu.ax = fclose((FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::PUTC] = [](VCPU<>& thiscpu)
    {
        int c = (int)*thiscpu.sp;
        long fp = *(thiscpu.sp + 1);
        *thiscpu.ax = fputc(c, (FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::GETC] = [](VCPU<>& thiscpu)
    {
        long fp = *thiscpu.sp;
        *thiscpu.ax = fgetc((FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::READ] = [](VCPU<>& thiscpu)
    {
        long fp = *thiscpu.sp;
        long buf = *(thiscpu.sp + 1);
        long size = *(thiscpu.sp + 2);
        *thiscpu.ax = fread((void*)buf, 1, size, (FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::WRITE_FILE] = [](VCPU<>& thiscpu)
    {
        long fp = *thiscpu.sp;
        long buf = *(thiscpu.sp + 1);
        long size = *(thiscpu.sp + 2);
        *thiscpu.ax = fwrite((void*)buf, 1, size, (FILE*)fp);
    };
}

std::optional<int64_t> VM::run()
{
    if (enable_debug)
    {
        while (vcpu.state == VCPU<>::CpuState::OK)
        {
            debug();
            vcpu.step();
        }
        if (vcpu.state == VCPU<>::CpuState::OVER)
        {
            return *vcpu.ax;
        }
        else
        {
            return std::nullopt;
        }
    }
    else
    {
        vcpu.run();
        if (vcpu.state == VCPU<>::CpuState::OVER)
        {
            return *vcpu.ax;
        }
        else
        {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void VM::debug()
{
    if (print_asm)
        std::cout << "next ins: " << vcpu.asms[vcpu.ip] << '\n';
    std::cout << "ip: " << vcpu.ip << '\n';
    std::cout << "bp: " << vcpu.bp << '\n';
    std::cout << "ax: " << std::hex << *vcpu.ax << '\n';
    std::cout << "stack:\n";
    for (int i = &vcpu.mem.back() - vcpu.sp - 1; i >= 0; i--)
    {
        if (&vcpu.sp[i] == vcpu.bp)
        {
            std::cout << "[" << &vcpu.sp[i] << "]: " << std::hex << vcpu.sp[i] << "<- bp" << '\n';
        }
        else
        {
            std::cout << "[" << &vcpu.sp[i] << "]: " << std::hex << vcpu.sp[i] << '\n';
        }
    }
    std::cout << "data:\n";
    for (int i = 3; i >= 0; i--)
    {
        std::cout << "[" << &vcpu.mem[i] << "]: " << std::hex << vcpu.mem[i] << '\n';
    }
    std::cout << std::dec; // 恢复为十进制
    std::cout << '\n';
}