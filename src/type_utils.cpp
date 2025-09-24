#include "type_utils.hpp"

static Type make_basic(Type::BasicType bt) {
    return Type(Type::Kind::Basic, bt);
}

bool is_integer_basic(const Type& t) {
    const Type* cur = &t;
    while (cur && (cur->kind == Type::Kind::Pointer || cur->kind == Type::Kind::Array)) {
        cur = cur->subType.get();
    }
    if (!cur || cur->kind != Type::Kind::Basic) return false;
    switch (cur->basic_type) {
        case Type::BasicType::Int:
        case Type::BasicType::Char:
        case Type::BasicType::Long:
        case Type::BasicType::Short:
        case Type::BasicType::Unsigned:
        case Type::BasicType::Signed:
            return true;
        default: return false;
    }
}

int pointer_depth(const Type& t) {
    int depth = 0;
    const Type* cur = &t;
    while (cur) {
        if (cur->kind == Type::Kind::Pointer) {
            depth += cur->arr_or_ptr_num <= 0 ? 1 : cur->arr_or_ptr_num;
        } else if (cur->kind == Type::Kind::Array) {
            // 数组表达式暂未衰减处理
        }
        cur = cur->subType.get();
    }
    return depth;
}

const Type& base_type(const Type& t) {
    const Type* cur = &t;
    while (cur && (cur->kind == Type::Kind::Pointer || cur->kind == Type::Kind::Array)) {
        cur = cur->subType.get();
    }
    return *cur;
}

static Type arithmetic_result(const Type&, const Type&) {
    return make_basic(Type::BasicType::Int);
}

Type deduce_binary_type(const Type& lhs, const Type& rhs, BinOp op) {
    auto is_cmp = [op]() {
        switch (op) {
            case BinOp::LogicAnd: case BinOp::LogicOr:
            case BinOp::CmpEq: case BinOp::CmpNe:
            case BinOp::CmpLt: case BinOp::CmpGt:
            case BinOp::CmpLe: case BinOp::CmpGe: return true;
            default: return false;
        }
    }();
    if (is_cmp) {
        return make_basic(Type::BasicType::Int);
    }

    int lptr = pointer_depth(lhs);
    int rptr = pointer_depth(rhs);

    switch (op) {
        case BinOp::Add: case BinOp::Sub: {
            if (lptr > 0 && rptr == 0 && is_integer_basic(rhs)) {
                return lhs; // ptr +/- int
            }
            if (rptr > 0 && lptr == 0 && is_integer_basic(lhs) && op == BinOp::Add) {
                return rhs; // int + ptr
            }
            if (lptr > 0 && rptr > 0) {
                if (op == BinOp::Sub) {
                    return make_basic(Type::BasicType::Int); // ptr - ptr => int
                }
                THROW_ERR_NOCTX(error::unsurpported_op);
            }
            if (is_integer_basic(lhs) && is_integer_basic(rhs)) {
                return arithmetic_result(lhs, rhs);
            }
            THROW_ERR_NOCTX(error::unsurpported_op);
        }
        case BinOp::Mul: case BinOp::Div: case BinOp::Mod:
        case BinOp::Shl: case BinOp::Shr:
        case BinOp::BitAnd: case BinOp::BitOr: case BinOp::BitXor: {
            if (lptr == 0 && rptr == 0 && is_integer_basic(lhs) && is_integer_basic(rhs)) {
                return arithmetic_result(lhs, rhs);
            }
            THROW_ERR_NOCTX(error::unsurpported_op);
        }
        default:
            THROW_ERR_NOCTX(error::unsurpported_op);
    }
}
