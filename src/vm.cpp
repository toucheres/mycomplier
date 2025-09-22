#include "vm.h"

std::optional<int> VM::run()
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
    std::cout << "next ins: " << vcpu.asms[vcpu.ip] << '\n';
    std::cout << "ip: " << vcpu.ip << '\n';
    std::cout << "ax: " << *vcpu.ax << '\n';
    std::cout << "stack:\n";
    for (int i = 0; i < &vcpu.mem.back() - vcpu.sp; i++)
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
    for (int i = 0; i < 4; i++)
    {
        std::cout << "[" << &vcpu.mem[i] << "]: " << std::hex << vcpu.mem[i] << '\n';
    }
    std::cout << std::dec; // 恢复为十进制
    std::cout << '\n';
}
