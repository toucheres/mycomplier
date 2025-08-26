#include "vm.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>



std::expected<bool, error> VM::setlogpath(std::string path)
{
    // // 创建新的文件输出流
    // static std::ofstream* file_stream = nullptr;

    // // 关闭并删除之前可能打开的文件流
    // if (file_stream)
    // {
    //     file_stream->close();
    //     delete file_stream;
    // }

    // // 创建新文件流
    // file_stream = new std::ofstream(path);

    // // 检查文件是否成功打开
    // if (!file_stream->is_open())
    // {
    //     std::cerr << "Failed to open log file: " << path << std::endl;
    //     delete file_stream;
    //     file_stream = nullptr;
    //     return std::unexpected(error::file_not_exsist);
    // }

    // // 重定向 logout 到新的文件流缓冲区
    // logout->rdbuf(file_stream->rdbuf());

    return true;
}

std::expected<bool, error> VM::eachcycle()
{
    return std::expected<bool, error>();
}
std::expected<int, error> VM::run()
{
    cpu.run();
    if (cpu.state == VCPU::State::STOP)
    {
        return cpu.ax;
    }
    else
    {
        return std::unexpected(error::cpu_error);
    }
}

void VCPU::do_ins(std::string thisasm)
{
    if (thisasm.starts_with("MOVE"))
    {
        this->DOMOVE(thisasm);
    } // MOVE ax stack;ax值替换栈顶值
    else if (thisasm.starts_with("IMM"))
    {
        this->DOIMM(thisasm);
    } // 立即数入栈
    else if (thisasm.starts_with("LEA"))
    {
        this->DOLEA(thisasm);
    } // 将bp+arg推入栈顶
    else if (thisasm.starts_with("LI"))
    {
        this->DOLI(thisasm);
    } // 栈顶为地址，替换栈顶为值
    else if (thisasm.starts_with("LC"))
    {
        this->DOLC(thisasm);
    } // 栈顶为地址，替换栈顶为值
    else if (thisasm.starts_with("SI"))
    {
        this->DOSI(thisasm);
    } // 栈顶为值，次栈顶为地址
    else if (thisasm.starts_with("SC"))
    {
        this->DOSC(thisasm);
    } // 栈顶为值，次栈顶为地址
    else if (thisasm.starts_with("ADD"))
    {
        this->DOADD(thisasm);
    } // 二元运算符汇编栈顶为右操作数，次栈顶为左操作数，出栈操作数，入栈结果
    else if (thisasm.starts_with("SUB"))
    {
        this->DOSUB(thisasm);
    }
    else if (thisasm.starts_with("MUL"))
    {
        this->DOMUL(thisasm);
    }
    else if (thisasm.starts_with("DIV"))
    {
        this->DODIV(thisasm);
    }
    else if (thisasm.starts_with("MOD"))
    {
        this->DOMOD(thisasm);
    }
    else if (thisasm.starts_with("JMP"))
    {
        this->DOJMP(thisasm);
    }
    else if (thisasm.starts_with("JZ"))
    {
        this->DOJZ(thisasm);
    }
    else if (thisasm.starts_with("JNZ"))
    {
        this->DOJNZ(thisasm);
    }
    else if (thisasm.starts_with("PUSH"))
    {
        this->DOPUSH(thisasm);
    } // ax->stack
    else if (thisasm.starts_with("POP"))
    {
        this->DOPOP(thisasm);
    } // stack->ax
    else if (thisasm.starts_with("CALL"))
    {
        this->DOCALL(thisasm);
    } // 栈顶为地址
    else if (thisasm.starts_with("NVAR"))
    {
        this->DONVAR(thisasm);
    } // 分配函数局部变量栈空间,4字节为单位
    else if (thisasm.starts_with("RET"))
    {
        this->DORET(thisasm);
    }
    else if (thisasm.starts_with("EXIT"))
    {
        this->DOEXIT(thisasm);
    }
    else if (thisasm.starts_with("DARG"))
    {
        this->DODARG(thisasm);
    }
    else if (thisasm.starts_with("UP"))
    {
        this->DOUP(thisasm);
    } // 分配data段
    else if (thisasm.starts_with("SYSTEMCALL"))
    {
        this->DOSYSTEMCALL(thisasm);
    }
    else
    {
    };
}

void VCPU::DOMOVE(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string src;
    std::string des;
    str >> src;
    str >> src;
    str >> des;
    std::map<std::string, int*> addrmap{{"stack", reinterpret_cast<int*>(&stack[sp - 4])},
                                        {"ax", &ax},
                                        {"pc", &pc},
                                        {"sp", &sp},
                                        {"bp", &bp}};
    *addrmap[des] = *addrmap[src];
    return;
}
void VCPU::DOIMM(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string src;
    str >> src;
    str >> src;
    this->stackpush(std::stoi(src));
}
// 将bp+arg推入栈顶
void VCPU::DOLEA(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, offset_str;
    str >> cmd >> offset_str;
    int offset = std::stoi(offset_str);

    // 计算地址 bp+offset 并压栈
    this->stackpush(bp + offset);
}

// 栈顶为地址，替换栈顶为值(int)
void VCPU::DOLI(std::string thisasm)
{
    // 获取栈顶地址
    int addr = this->stacktop();
    // 弹出地址
    this->stackpop();
    // 获取该地址处的int值并压栈
    int value = this->memget(addr, 4);
    this->stackpush(value);
}

// 栈顶为地址，替换栈顶为值(char)
void VCPU::DOLC(std::string thisasm)
{
    int addr = this->stacktop();
    this->stackpop();
    // 获取该地址处的char值并压栈
    int value = this->memget(addr, 1);
    this->stackpush(value);
}

// 栈顶为值，次栈顶为地址，存储int值
void VCPU::DOSI(std::string thisasm)
{
    // 获取栈顶值
    int value = this->stacktop();
    this->stackpop();

    // 获取次栈顶地址
    int addr = this->stacktop();
    this->stackpop();

    // 在地址处存储值
    this->memloal(addr, value, 4);
}

// 栈顶为值，次栈顶为地址，存储char值
void VCPU::DOSC(std::string thisasm)
{
    int value = this->stacktop();
    this->stackpop();

    int addr = this->stacktop();
    this->stackpop();

    this->memloal(addr, value, 1);
}

// 加法操作
void VCPU::DOADD(std::string thisasm)
{
    int right = this->stacktop();
    this->stackpop();

    int left = this->stacktop();
    this->stackpop();

    this->stackpush(left + right);
}

// 减法操作
void VCPU::DOSUB(std::string thisasm)
{
    int right = this->stacktop();
    this->stackpop();

    int left = this->stacktop();
    this->stackpop();

    this->stackpush(left - right);
}

// 乘法操作
void VCPU::DOMUL(std::string thisasm)
{
    int right = this->stacktop();
    this->stackpop();

    int left = this->stacktop();
    this->stackpop();

    this->stackpush(left * right);
}

// 除法操作
void VCPU::DODIV(std::string thisasm)
{
    int right = this->stacktop();
    this->stackpop();

    int left = this->stacktop();
    this->stackpop();

    if (right == 0)
    {
        // 处理除零错误
        // 这里可以设置一个错误标志或抛出异常
        this->stackpush(0);
    }
    else
    {
        this->stackpush(left / right);
    }
}

// 取模操作
void VCPU::DOMOD(std::string thisasm)
{
    int right = this->stacktop();
    this->stackpop();

    int left = this->stacktop();
    this->stackpop();

    if (right == 0)
    {
        // 处理除零错误
        this->stackpush(0);
    }
    else
    {
        this->stackpush(left % right);
    }
}

// 无条件跳转
void VCPU::DOJMP(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, addr_str;
    str >> cmd >> addr_str;

    // 设置程序计数器为目标地址（-1是因为do_cycle会自增pc）
    pc = std::stoi(addr_str) - 1;
}

// 如果栈顶为0则跳转
void VCPU::DOJZ(std::string thisasm)
{
    int value = this->stacktop();
    this->stackpop();

    if (value == 0)
    {
        std::stringstream str(thisasm);
        std::string cmd, addr_str;
        str >> cmd >> addr_str;

        pc = std::stoi(addr_str) - 1;
    }
}

// 如果栈顶不为0则跳转
void VCPU::DOJNZ(std::string thisasm)
{
    int value = this->stacktop();
    this->stackpop();

    if (value != 0)
    {
        std::stringstream str(thisasm);
        std::string cmd, addr_str;
        str >> cmd >> addr_str;

        pc = std::stoi(addr_str) - 1;
    }
}

// 将ax值压入栈
void VCPU::DOPUSH(std::string thisasm)
{
    this->stackpush(ax);
}

// 将栈顶值弹出到ax
void VCPU::DOPOP(std::string thisasm)
{
    ax = this->stacktop();
    this->stackpop();
}

// 函数调用，栈顶为函数地址
void VCPU::DOCALL(std::string thisasm)
{
    // 获取函数地址
    int func_addr = this->stacktop();
    this->stackpop();

    // 保存返回地址
    this->stackpush(pc + 1);

    // 保存旧的帧指针
    this->stackpush(bp);

    // 设置新的帧指针
    bp = sp;

    // 跳转到函数地址（-1是因为do_cycle会自增pc）
    pc = func_addr - 1;
}

// 分配局部变量空间，参数为变量数量
void VCPU::DONVAR(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, count_str;
    str >> cmd >> count_str;

    // 分配空间（以4字节为单位）
    int count = std::stoi(count_str);
    sp += count * VCPU::size_word;
}

// 函数返回
void VCPU::DORET(std::string thisasm)
{
    // 将返回值保存到ax
    ax = this->stacktop();

    // 恢复sp到帧起始位置
    sp = bp;

    // 恢复旧的帧指针
    bp = this->stacktop();
    this->stackpop(); // 弹出旧的bp值
    int ret_addr = this->stacktop();
    // 恢复返回地址并跳转
    this->stackpop();
    pc = ret_addr - 1; // -1因为do_cycle会自增
}

// 退出程序
void VCPU::DOEXIT(std::string thisasm)
{
    state = State::STOP;
}

// 清理函数参数
void VCPU::DODARG(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, count_str;
    str >> cmd >> count_str;

    // 移除参数（每个参数4字节）
    int count = std::stoi(count_str);
    sp -= count * 4;
}

// 分配data段
void VCPU::DOUP(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, size_str;
    str >> cmd >> size_str;

    int size = std::stoi(size_str) * 4;
    bp += size;
    sp += size;
    // 确保stack容量足够
    if (stack.size() < size)
    {
        stack.resize(size);
    }
}

// 系统调用
void VCPU::DOSYSTEMCALL(std::string thisasm)
{
    std::stringstream str(thisasm);
    std::string cmd, syscall_id;
    str >> cmd >> syscall_id;

    int id = std::stoi(syscall_id);

    // 根据syscall_id执行不同的系统调用
    switch (id)
    {
    case 1: // 打印整数
        std::cout << ax << std::endl;
        break;
    case 2: // 打印字符
        std::cout << static_cast<char>(ax);
        break;
    // 可以添加更多系统调用
    default:
        // 未知系统调用，可以记录错误
        break;
    }
}