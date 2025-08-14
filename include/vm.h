#pragma once
#include <fstream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

struct VCPU
{
    std::vector<std::string> assembly_code; // 直接存储汇编指令字符串
    std::vector<int> stack;                 // stack segment
    std::vector<int> data;                 // data segment

    int pc; // program counter (现在是assembly_code的索引)
    int sp; // stack pointer
    int bp; // base pointer

    int ax; // accumulator register
    int cycle;

    // 构造函数
    VCPU() : pc(0), sp(0), bp(0), ax(0), cycle(0)
    {
        stack.resize(1024); // 默认栈大小
        data.resize(1024);  // 默认数据段大小
    }
};

struct Execution
{
    enum class Status
    {
        RUNNING,
        STOPPED,
        ERROR,
        SYSCALL_PENDING
    };

    Status status;
    int exit_code;
    std::string error_message;

    Execution() : status(Status::STOPPED), exit_code(0)
    {
    }
};

class VM
{
  private:
    Execution exec;
    std::unordered_map<std::string, int> external_functions;

    // 调试相关
    bool debug_enabled;
    std::ofstream debug_log;
    int step_count;

    // 私有辅助方法
    void push(int value);
    int pop();
    void execute_instruction();
    void handle_syscall(int syscall_id);
    bool check_bounds(int address, int size = 1);

    // 调试方法
    void log_step_info();
    std::string get_instruction_name(int instruction_code);

  public:
    VCPU cpu; // 改为公有，方便测试和调试
    enum class ASM
    {
        SYSTEMCALL,
        IMM,  // 立即数压栈
        LEA,  // 加载有效地址
        JMP,  // 无条件跳转
        JZ,   // 零跳转
        JNZ,  // 非零跳转
        CALL, // 函数调用
        NVAR, // 新建局部变量
        DARG, // 删除参数
        RET,  // 函数返回
        LI,   // 从地址加载整数
        LC,   // 从地址加载字符
        SI,   // 存储整数到地址
        SC,   // 存储字符到地址
        PUSH, // 压栈
        OR,   // 逻辑或
        XOR,  // 异或
        AND,  // 逻辑与
        EQ,   // 相等比较
        NE,   // 不等比较
        LT,   // 小于比较
        GT,   // 大于比较
        LE,   // 小于等于比较
        GE,   // 大于等于比较
        SHL,  // 左移
        SHR,  // 右移
        ADD,  // 加法
        SUB,  // 减法
        MUL,  // 乘法
        DIV,  // 除法
        MOD,  // 取模
        HOLD, // 占位
    };

    enum class systemcall
    {
        OPEN,
        READ,
        CLOS,
        PRTF, // printf
        MALC, // malloc
        FREE, // free
        MSET, // memset
        MCMP, // memcmp
        EXIT, // exit
    };

    // 构造函数
    VM();

    // 加载代码到虚拟机
    void load_code(const std::vector<int>& code);

    // 直接加载汇编指令数组（新的主要方法）
    void load_assembly_vector(const std::vector<std::string>& assembly);

    // 从字符串格式的汇编代码加载（保留兼容性）
    void load_assembly_string(const std::vector<std::string>& assembly);

    // 从stack格式加载汇编代码
    void load_assembly_stack(std::stack<std::string> assembly_stack);

    // 执行虚拟机
    int start();

    // 单步执行
    bool step();

    // 重置虚拟机
    void reset();

    // 获取执行状态
    const Execution& get_execution_status() const
    {
        return exec;
    }

    // 调试功能
    void dump_registers() const;
    void dump_stack(int count = 10) const;
    void dump_code(int start = 0, int count = 20) const;

    // 注册外部函数
    void register_external_function(const std::string& name, int address);

    // 调试功能
    void enable_debug(const std::string& log_filename = "vm_debug.log");
    void disable_debug();
};