#pragma once
#include "obj.h"
#include "error.hpp"

enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    Shl, Shr,
    BitAnd, BitOr, BitXor,
    LogicAnd, LogicOr,
    CmpEq, CmpNe, CmpLt, CmpGt, CmpLe, CmpGe,
};

bool is_integer_basic(const Type& t);
int pointer_depth(const Type& t);
const Type& base_type(const Type& t);
Type deduce_binary_type(const Type& lhs, const Type& rhs, BinOp op);
