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
    // std::vector<int> data;                  // data segment

    int pc; // program counter (现在是assembly_code的索引)
    int sp; // stack pointer
    int bp; // base pointer

    int ax; // accumulator register
    int cycle;

    // 构造函数
    VCPU() : pc(0), sp(0), bp(0), ax(0), cycle(0)
    {
        stack.resize(1024); // 默认栈大小
        // data.resize(1024);  // 默认数据段大小
    }
    enum class stack_cpu
    {
        PC,
        SP,
        BP,
        AX,
        STACK
    };
};

struct Execution
{
    enum class Status
    {
        RUNNING,
        WARING,
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
  public:
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
        RET,  // 函数返回
        LI,   // 从地址加载整数
        LC,   // 从地址加载字符
        SI,   // 存储整数到地址
        SC,   // 存储字符到地址
        MOVE, // 在栈顶，寄存器间复制 [TODO]未完成
        PUSH, // ax压栈
        POP,  // 出栈到ax
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
        UP,   //  bp,sp += arg，为.data 动态内存预留
        HOLD, // 占位
        DARG, // 弹出参数
    };

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

    // 新增：参数解析和验证
    bool parse_instruction_args(const std::string& line, std::string& instruction,
                                std::vector<int>& args);
    bool validate_args(VM::ASM asm_type, const std::vector<int>& args);

    // 调试方法
    void log_step_info();
    std::string get_instruction_name(int instruction_code);

  public:
    VCPU cpu; // 改为公有，方便测试和调试

    struct ASMmeta
    {
        std::string name;
        size_t num_args = 0; // 1为1个  2为2个...  10为0个或1个 210为0个或1个或2个...
    };
    ASM formStrToASM(const std::string& str)
    {
        static const std::unordered_map<std::string, ASM> name_map{
            {"SYSTEMCALL", ASM::SYSTEMCALL},
            {"IMM", ASM::IMM},
            {"LEA", ASM::LEA},
            {"JMP", ASM::JMP},
            {"JZ", ASM::JZ},
            {"JNZ", ASM::JNZ},
            {"CALL", ASM::CALL},
            {"NVAR", ASM::NVAR},
            {"DARG", ASM::DARG},
            {"RET", ASM::RET},
            {"MOVE", ASM::MOVE},
            {"LI", ASM::LI},
            {"LC", ASM::LC},
            {"SI", ASM::SI},
            {"SC", ASM::SC},
            {"PUSH", ASM::PUSH},
            {"POP", ASM::POP},
            {"DARG", ASM::DARG},
            {"OR", ASM::OR},
            {"XOR", ASM::XOR},
            {"AND", ASM::AND},
            {"EQ", ASM::EQ},
            {"NE", ASM::NE},
            {"LT", ASM::LT},
            {"GT", ASM::GT},
            {"LE", ASM::LE},
            {"GE", ASM::GE},
            {"SHL", ASM::SHL},
            {"SHR", ASM::SHR},
            {"ADD", ASM::ADD},
            {"SUB", ASM::SUB},
            {"MUL", ASM::MUL},
            {"DIV", ASM::DIV},
            {"MOD", ASM::MOD},
            {"UP", ASM::UP},
            {"HOLD", ASM::HOLD},
        };

        auto it = name_map.find(str);
        if (it != name_map.end())
        {
            return it->second;
        }
        return ASM::HOLD; // 未知指令返回HOLD作为占位符
    }
    static ASMmeta getASMmeta(VM::ASM ASM)
    {
        static const std::unordered_map<VM::ASM, ASMmeta> ASM_META{
            {VM::ASM::SYSTEMCALL, ASMmeta{"SYSTEMCALL", 1}},
            {VM::ASM::IMM, ASMmeta{"IMM", 1}},
            {VM::ASM::LEA, ASMmeta{"LEA", 1}},
            {VM::ASM::JMP, ASMmeta{"JMP", 1}},
            {VM::ASM::JZ, ASMmeta{"JZ", 1}},
            {VM::ASM::JNZ, ASMmeta{"JNZ", 1}},
            {VM::ASM::CALL, ASMmeta{"CALL", 1}},
            {VM::ASM::NVAR, ASMmeta{"NVAR", 1}},
            {VM::ASM::DARG, ASMmeta{"DARG", 1}},
            {VM::ASM::RET, ASMmeta{"RET", 0}},
            {VM::ASM::MOVE, ASMmeta{"MOVE", 2}},
            {VM::ASM::LI, ASMmeta{"LI", 10}}, // 可选参数：有参数时直接访问地址，无参数时从栈取地址
            {VM::ASM::LC, ASMmeta{"LC", 10}}, // 同LI，但加载字符
            {VM::ASM::SI, ASMmeta{"SI", 10}}, // 可选参数：有参数时直接写地址，无参数时从栈取地址
            {VM::ASM::SC, ASMmeta{"SC", 10}}, // 同SI，但存储字符
            {VM::ASM::PUSH, ASMmeta{"PUSH", 0}}, // 当前实现无参数，推送ax
            {VM::ASM::POP, ASMmeta{"POP", 0}},
            {VM::ASM::DARG , ASMmeta{"DARG", 1}},
            {VM::ASM::OR, ASMmeta{"OR", 0}},
            {VM::ASM::XOR, ASMmeta{"XOR", 0}},
            {VM::ASM::AND, ASMmeta{"AND", 0}},
            {VM::ASM::EQ, ASMmeta{"EQ", 0}},
            {VM::ASM::NE, ASMmeta{"NE", 0}},
            {VM::ASM::LT, ASMmeta{"LT", 0}},
            {VM::ASM::GT, ASMmeta{"GT", 0}},
            {VM::ASM::LE, ASMmeta{"LE", 0}},
            {VM::ASM::GE, ASMmeta{"GE", 0}},
            {VM::ASM::SHL, ASMmeta{"SHL", 0}},
            {VM::ASM::SHR, ASMmeta{"SHR", 0}},
            {VM::ASM::ADD, ASMmeta{"ADD", 0}},
            {VM::ASM::SUB, ASMmeta{"SUB", 0}},
            {VM::ASM::MUL, ASMmeta{"MUL", 0}},
            {VM::ASM::DIV, ASMmeta{"DIV", 0}},
            {VM::ASM::MOD, ASMmeta{"MOD", 0}},
            {VM::ASM::UP, ASMmeta{"UP", 10}}, // 可选参数：有参数时直接使用，无参数时从栈取
            {VM::ASM::HOLD, ASMmeta{"HOLD", 0}},
        };

        auto it = ASM_META.find(ASM);
        if (it != ASM_META.end())
        {
            return it->second;
        }
        return ASMmeta{"UNKNOWN", 0};
    }
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