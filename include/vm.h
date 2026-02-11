#pragma once
#include "error.hpp"
#include <array>
#include <deque>
#include <cstring>
#include <expected>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
class VM;
struct VCPU
{
    static const size_t StackSize = 1024000;
    using Word = int64_t;
    inline static const size_t size_word = sizeof(Word);
    std::map<int, std::function<void(VCPU&)>> systemcall_table;
    alignas(8) std::vector<std::string> asms;
    std::array<Word, StackSize / size_word> mem{0};
    Word axmem = 0;
    Word* ax = &axmem;
    Word* bp = &mem.back();
    Word* sp = &mem.back();
    Word* ss = &mem.back();
    Word* ds = &mem.front();
    size_t ip = 0;
    enum class CpuState
    {
        OK,
        ERROR,
        OVER
    };
    CpuState state = CpuState::OK;
    void do_ins(const std::string& in);
    void step();
    void run();
};
struct VM
{
    enum systemcall
    {
        WRITE,
        MALLOC,
        FREE,
        BREAKPOINT,
        OPEN,
        CLOSE,
        PUTC,
        GETC,
        READ,
        WRITE_FILE,
        EXIT,
        MEMCPY,
        MEMSET
    };
    VM(const std::vector<std::string>& asms);
    bool enable_debug = true;
    std::deque<std::string> debug_buffer{1024};
    VCPU vcpu;
    std::optional<int64_t> run();
    void debug();
    void dump_debug_buffer(size_t lo);
};
