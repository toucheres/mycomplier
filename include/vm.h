#pragma once
#include "error.hpp"
#include <expected>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>
template <size_t StackSize = 1024, class Word = int64_t> struct VCPU
{
    inline static const size_t size_word = sizeof(Word);
    std::array<Word, StackSize / size_word> mem;
    Word axmem = 0;
    Word* ax = &axmem;
    Word* bp = &mem.back();
    Word* sp = &mem.back();
    Word* ss = &mem.back();
    Word* ds = &mem.front();
};
struct VM
{
};