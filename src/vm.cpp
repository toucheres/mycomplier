#include "vm.h"

#ifdef __linux__
#include <csignal>
#include <cstdlib>

namespace
{
    VM* g_current_vm = nullptr;

    void crash_signal_handler(int sig)
    {
        const char* sig_name = "Unknown signal";
        switch (sig)
        {
        case SIGSEGV:
            sig_name = "SIGSEGV (Segmentation fault)";
            break;
        case SIGBUS:
            sig_name = "SIGBUS (Bus error)";
            break;
        case SIGFPE:
            sig_name = "SIGFPE (Floating point exception)";
            break;
        case SIGABRT:
            sig_name = "SIGABRT (Abort)";
            break;
        case SIGILL:
            sig_name = "SIGILL (Illegal instruction)";
            break;
        }
        std::cerr << "\n=== FATAL SIGNAL: " << sig_name << " ===" << std::endl;
        if (g_current_vm)
        {
            g_current_vm->dump_debug_buffer(128);
        }
        // 恢复默认处理并重新触发信号，让程序正常终止
        signal(sig, SIG_DFL);
        raise(sig);
    }

    void setup_signal_handlers(VM* vm)
    {
        g_current_vm = vm;
        signal(SIGSEGV, crash_signal_handler);
        signal(SIGBUS, crash_signal_handler);
        signal(SIGFPE, crash_signal_handler);
        signal(SIGABRT, crash_signal_handler);
        signal(SIGILL, crash_signal_handler);
    }

    void cleanup_signal_handlers()
    {
        g_current_vm = nullptr;
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        signal(SIGFPE, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        signal(SIGILL, SIG_DFL);
    }
} // namespace
#endif
VM::VM(const std::vector<std::string>& asms)
{
    vcpu.asms = asms;
    vcpu.systemcall_table[VM::systemcall::WRITE] = [](VCPU& thiscpu)
    {
        char tp = *thiscpu.sp;
        std::cout << tp;
    };
    vcpu.systemcall_table[VM::systemcall::MALLOC] = [](VCPU& thiscpu)
    {
        long tp = *thiscpu.sp;
        *thiscpu.ax = (long)malloc(tp);
    };
    vcpu.systemcall_table[VM::systemcall::FREE] = [](VCPU& thiscpu)
    {
        long tp = *thiscpu.sp;
        free((void*)tp);
    };
    vcpu.systemcall_table[VM::systemcall::BREAKPOINT] = [this](VCPU& thiscpu) { debug(); };
    vcpu.systemcall_table[VM::systemcall::OPEN] = [](VCPU& thiscpu)
    {
        long fileptr = *thiscpu.sp;
        long modeptr = *(thiscpu.sp + 1);
        *thiscpu.ax = (long)fopen((char*)fileptr, (char*)modeptr);
    };
    vcpu.systemcall_table[VM::systemcall::CLOSE] = [](VCPU& thiscpu)
    {
        long fp = *thiscpu.sp;
        *thiscpu.ax = fclose((FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::PUTC] = [](VCPU& thiscpu)
    {
        int c = (int)*thiscpu.sp;
        long fp = *(thiscpu.sp + 1);
        *thiscpu.ax = fputc(c, (FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::GETC] = [](VCPU& thiscpu)
    {
        long fp = *thiscpu.sp;
        *thiscpu.ax = fgetc((FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::READ] = [](VCPU& thiscpu)
    {
        long fp = *thiscpu.sp;
        long buf = *(thiscpu.sp + 1);
        long size = *(thiscpu.sp + 2);
        *thiscpu.ax = fread((void*)buf, 1, size, (FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::WRITE_FILE] = [](VCPU& thiscpu)
    {
        long fp = *thiscpu.sp;
        long buf = *(thiscpu.sp + 1);
        long size = *(thiscpu.sp + 2);
        *thiscpu.ax = fwrite((void*)buf, 1, size, (FILE*)fp);
    };
    vcpu.systemcall_table[VM::systemcall::EXIT] = [](VCPU& thiscpu)
    {
        long retcode = *thiscpu.sp;
        *thiscpu.ax = retcode;
        thiscpu.state = VCPU::CpuState::OVER;
    };
    vcpu.systemcall_table[VM::systemcall::MEMCPY] = [](VCPU& thiscpu)
    {
        long dest = *thiscpu.sp;
        long src = *(thiscpu.sp + 1);
        long n = *(thiscpu.sp + 2);
        *thiscpu.ax = (long)std::memcpy((void*)dest, (void*)src, n);
    };
    vcpu.systemcall_table[VM::systemcall::MEMSET] = [](VCPU& thiscpu)
    {
        long s = *thiscpu.sp;
        int c = (int)*(thiscpu.sp + 1);
        long n = *(thiscpu.sp + 2);
        *thiscpu.ax = (long)std::memset((void*)s, c, n);
    };
}

std::optional<int64_t> VM::run()
try
{
#ifdef __linux__
    setup_signal_handlers(this);
#endif

    while (vcpu.state == VCPU::CpuState::OK)
    {
        debug();
        vcpu.step();
    }
    if (vcpu.state == VCPU::CpuState::OVER)
    {
#ifdef __linux__
        cleanup_signal_handlers();
#endif
        return *vcpu.ax;
    }
    else
    {
#ifdef __linux__
        cleanup_signal_handlers();
#endif
        return std::nullopt;
    }

    return std::nullopt;
}
catch (...)
{
    debug();
    dump_debug_buffer(128);
    throw;
}
void VM::debug()
{
    std::ostringstream out;
    if (print_asm)
        out << "next ins: " << vcpu.asms[vcpu.ip] << '\n';
    out << "ip: " << vcpu.ip << '\n';
    out << "bp: " << vcpu.bp << '\n';
    out << "ax: " << std::hex << *vcpu.ax << '\n';
    out << "stack:\n";
    for (int i = &vcpu.mem.back() - vcpu.sp - 1; i >= 0; i--)
    {
        if (&vcpu.sp[i] == vcpu.bp)
        {
            out << "[" << &vcpu.sp[i] << "]: " << std::hex << vcpu.sp[i] << "<- bp" << '\n';
        }
        else
        {
            out << "[" << &vcpu.sp[i] << "]: " << std::hex << vcpu.sp[i] << '\n';
        }
    }
    out << "data:\n";
    for (int i = 3; i >= 0; i--)
    {
        out << "[" << &vcpu.mem[i] << "]: " << std::hex << vcpu.mem[i] << '\n';
    }
    out << std::dec; // 恢复为十进制
    out << '\n';

    const std::string info = out.str();
    if (enable_debug)
    {
        std::cout << info;
    }
    else
    {
        debug_buffer.push_back(info);
        if (debug_buffer.size() > 10240)
        {
            debug_buffer.pop_front();
        }
    }
}

void VM::dump_debug_buffer(size_t lo)
{
    // for (const auto& info : debug_buffer)
    // {
    //     std::cout << info;
    // }
    if (lo == SIZE_MAX)
    {
        for (auto& each : debug_buffer)
        {
            std::cout << each;
        }
    }
    else
    {
        for (int i = 0; i < lo; i++)
        {
            std::cout << debug_buffer[debug_buffer.size() - lo + i];
        }
    }
    std::cout.flush();
}

void VCPU::do_ins(const std::string& in)
{
    // [TODO] call ret systemcall
    std::stringstream str(in);
    std::string ins;
    str >> ins;
    if (ins == "POP")
    {
        *ax = *sp;
        // pop: move toward higher address
        sp++;
        return;
    }
    else if (ins == "PUSH")
    {
        // push: move toward lower address
        sp--;
        *sp = *ax;
        return;
    }
    else if (ins == "IMM")
    {
        std::string arg;
        str >> arg;
        Word num = std::stol(arg);
        sp--;
        *sp = static_cast<Word>(num);
        return;
    }
    else if (ins == "LEA")
    {
        *sp = reinterpret_cast<Word>(reinterpret_cast<char*>(bp) + *sp);
        return;
    }
    else if (ins == "LEAD")
    {
        *sp = reinterpret_cast<Word>(reinterpret_cast<char*>(ds) + *sp);
        return;
    }
    else if (ins == "COPY")
    {
        // duplicate top: new top at lower address equals previous top
        sp--;
        *sp = *(sp + 1);
        return;
    }
    else if (ins == "SWAP")
    {
        // swap top and second-top
        Word tmp = *sp;
        *sp = *(sp + 1);
        *(sp + 1) = tmp;
        return;
    }
    else if (ins == "LI")
    {
        int32_t* addr = reinterpret_cast<int32_t*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "LC")
    {
        char* addr = reinterpret_cast<char*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "LW")
    {
        int64_t* addr = reinterpret_cast<int64_t*>(*sp);
        *sp = *addr;
        return;
    }
    else if (ins == "SI")
    {
        auto val = *sp;
        sp++;
        int32_t* addr = reinterpret_cast<int32_t*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "SC")
    {
        auto val = *sp;
        sp++;
        char* addr = reinterpret_cast<char*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "SW")
    {
        auto val = *sp;
        sp++;
        int64_t* addr = reinterpret_cast<int64_t*>(*sp);
        sp++;
        *addr = val;
        return;
    }
    else if (ins == "MOVS")
    {
        // stack: [dest][src][n] (all absolute addresses); n may be immediate
        std::size_t n = 0;
        if (std::string arg; str >> arg)
        {
            n = static_cast<std::size_t>(std::stoull(arg));
        }
        else
        {
            n = static_cast<std::size_t>(*sp);
            sp++;
        }

        auto src = reinterpret_cast<char*>(*sp);
        sp++;
        auto dst = reinterpret_cast<char*>(*sp);
        sp++;
        std::memmove(dst, src, n);
        return;
    }
    else if (ins == "LODS")
    {
        // stack: [addr][n]; n may be immediate
        std::size_t n = 0;
        if (std::string arg; str >> arg)
        {
            n = static_cast<std::size_t>(std::stoull(arg));
        }
        else
        {
            n = static_cast<std::size_t>(*sp);
            sp++;
        }

        auto addr = reinterpret_cast<char*>(*sp);
        sp++;
        const std::size_t word = sizeof(Word);
        const std::size_t words = (n + word - 1) / word;
        for (std::size_t i = words; i > 0; --i)
        {
            Word w = 0;
            const std::size_t chunk_off = (i - 1) * word;
            const std::size_t chunk = std::min(word, n - chunk_off);
            std::memcpy(&w, addr + chunk_off, chunk);
            sp--;
            *sp = w;
        }
        return;
    }
    else if (ins == "SAVS")
    {
        // stack: [addr][n] then packed data words on top; n may be immediate
        std::size_t n = 0;
        if (std::string arg; str >> arg)
        {
            n = static_cast<std::size_t>(std::stoull(arg));
        }
        else
        {
            n = static_cast<std::size_t>(*sp);
            sp++;
        }

        auto addr = reinterpret_cast<char*>(*sp);
        sp++;
        const std::size_t word = sizeof(Word);
        const std::size_t words = (n + word - 1) / word;
        for (std::size_t i = 0; i < words; ++i)
        {
            Word w = *sp;
            sp++;
            const std::size_t chunk = std::min(word, n - i * word);
            std::memcpy(addr + i * word, &w, chunk);
        }
        return;
    }
    else if (ins == "ADD")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left + right;
        return;
    }
    else if (ins == "SUB")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left - right;
        return;
    }
    else if (ins == "MUL")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left * right;
        return;
    }
    else if (ins == "DIV")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left / right;
        return;
    }
    else if (ins == "MOD")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left % right;
        return;
    }
    else if (ins == "AND")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left & right;
        return;
    }
    else if (ins == "OR")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left | right;
        return;
    }
    else if (ins == "LSHIFT")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left << right;
        return;
    }
    else if (ins == "RSHIFT")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left >> right;
        return;
    }
    else if (ins == "XOR")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left ^ right;
        return;
    }
    else if (ins == "SMALL")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left < right;
        return;
    }
    else if (ins == "BIG")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left > right;
        return;
    }
    else if (ins == "SMALLE")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left <= right;
        return;
    }
    else if (ins == "BIGE")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left >= right;
        return;
    }
    else if (ins == "CMP")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = left == right;
        return;
    }
    else if (ins == "CMPN")
    {
        auto right = *sp;
        sp++;
        auto left = *sp;
        *sp = !(left == right);
        return;
    }
    else if (ins == "NOT")
    {
        auto right = *sp;
        *sp = !right;
        return;
    }
    else if (ins == "JMP")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        ip = num - 1; // 抵消ip自增
        return;
    }
    else if (ins == "JZ")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        auto right = *sp;
        sp++;
        if (!right)
        {
            ip = num - 1; // 抵消ip自增
        }
        return;
    }
    else if (ins == "JNZ")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        auto right = *sp;
        sp++;
        if (right)
        {
            ip = num - 1; // 抵消ip自增
        }
        return;
    }
    else if (ins == "CALL")
    {
        // 约定：栈顶为目标地址，栈向低地址增长
        // CALL：pop 目标地址；push 返回地址；push 旧bp；bp=sp；跳转
        auto dest = static_cast<size_t>(*sp);
        sp++; // pop target

        // push return address
        sp--;
        *sp = static_cast<Word>(ip + 1);

        // push old bp
        sp--;
        *sp = reinterpret_cast<Word>(bp);

        // new frame
        // bp = reinterpret_cast<Word*>(*sp);
        bp = sp;

        ip = dest - 1; // 跳转（抵消 step 自增）
        return;
    }
    else if (ins == "RET")
    {
        // 约定：RET 时，ax为函数返回值
        *ax = *sp;
        sp++; // pop return value

        // 丢弃局部变量：sp 回到当前帧基址（旧bp 存在 [bp]）
        sp = bp;

        // 弹出旧bp（位于 [sp]）
        Word oldbp_val = *sp;
        sp++;
        // 弹出返回地址（位于 [sp]）
        Word retaddr = *sp;
        sp++;

        // 恢复 bp
        bp = reinterpret_cast<Word*>(oldbp_val);

        // // 把返回值压回给调用者
        // sp--;
        // *sp = retv;

        ip = static_cast<size_t>(retaddr) - 1;
        return;
    }
    else if (ins == "NVAR")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        while (num--)
        {
            sp--;
        }
        return;
    }
    else if (ins == "DARG")
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        while (num--)
        {
            sp++;
        }
        return;
    }
    else if (ins == "EXIT")
    {
        state = VCPU::CpuState::OVER;
        return;
    }
    else if (ins == "SYSTEMCALL") // systemcall由调用者(用户)清理参数
    {
        std::string arg;
        str >> arg;
        int num = std::stoi(arg);
        systemcall_table[num](*this);
        return;
    }
}
void VCPU::step()
{
    do_ins(asms[ip]);
    ip++;
}

void VCPU::run()
{
    while (state == CpuState::OK)
    {
        step();
    }
}
