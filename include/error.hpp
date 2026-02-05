#pragma once
#include <exception>
#include <string>
#include <utility>

enum class error
{
    file_not_exsist,
    empty_node,
    undifined_feild,
    undifined_type,
    undifined_func,
    undifined_var,
    undifined_id,
    undifined_struct,
    undifined_obj_init_fun,
    double_defined,
    double_type,
    double_StorageClassSpecifier,
    cpu_error,
    illegal_calcu,
    unsurpport_basictype,
    unsurpport_DeclarationSpecifier,
    unsurpport_directDeclarator,
    unsurpport_StorageClassSpecifier,
    unsurpport_union,
    unsurpport_abstractDeclarator,
    unsurpported_op,
    invalid_constant,
    expected_lvalue,
    expected_arr_initor,
    expected_aligned_addr,
    expected_struct,
    expected_struct_ptr,
    expected_func_or_funcptr,
    expected_ptr,
    expected_type,
    expected_same_type,
    expected_initializerList,
    initializerList_too_long,
    unsurpported_num,
    unexpected_storageClassSpecifier,
    preprocess_error,
    unknow,
};

inline const char* error_name(error e)
{
    switch (e)
    {
    case error::file_not_exsist:
        return "file_not_exsist";
    case error::empty_node:
        return "empty_node";
    case error::undifined_type:
        return "undifined_type";
    case error::undifined_func:
        return "undifined_func";
    case error::undifined_var:
        return "undifined_var";
    case error::undifined_id:
        return "undifined_id";
    case error::undifined_obj_init_fun:
        return "undifined_obj_init_fun";
    case error::double_defined:
        return "double_defined";
    case error::double_type:
        return "double_type";
    case error::cpu_error:
        return "cpu_error";
    case error::illegal_calcu:
        return "illegal_calcu";
    case error::unsurpport_basictype:
        return "unsurpport_basictype";
    case error::unsurpport_directDeclarator:
        return "unsurpport_directDeclarator";
    case error::unsurpport_abstractDeclarator:
        return "unsurpport_abstractDeclarator";
    case error::unsurpported_op:
        return "unsurpported_op";
    case error::invalid_constant:
        return "invalid_constant";
    case error::expected_lvalue:
        return "expected_lvalue";
    case error::expected_arr_initor:
        return "expected_arr_initor";
    case error::expected_func_or_funcptr:
        return "expected_func_or_funcptr";
    case error::expected_ptr:
        return "expected_ptr";
    case error::unsurpported_num:
        return "unsurpported_num";
    default:
        return "unknown_error";
    }
}

struct compile_error : std::exception
{
    error code;
    std::string ctx_text;      // 来自 ctx->getText()
    std::string file;          // 抛出位置: 源文件
    int line = 0;              // 抛出位置: 行号
    std::string func;          // 抛出位置: 函数
    std::string message_cache; // 缓存 what() 返回值

    compile_error(error c, std::string ctx = {}, const char* file_in = nullptr,
                  int line_in = 0, const char* func_in = nullptr)
        : code(c), ctx_text(std::move(ctx)), file(file_in ? file_in : ""), line(line_in),
          func(func_in ? func_in : "")
    {
        message_cache = std::string("error[") + error_name(code) + "]";

        if (!file.empty())
        {
            message_cache += ": " + file;
            if (line > 0)
            {
                message_cache += ":" + std::to_string(line);
            }
        }

        if (!func.empty())
        {
            message_cache += " in " + func;
        }

        if (!ctx_text.empty())
        {
            message_cache += " -> " + ctx_text;
        }
    }

    const char* what() const noexcept override
    {
        return message_cache.c_str();
    }
};

// THROW_ERR(code, ctxPtr) 若 ctxPtr 非空则捕获其 getText()
#define THROW_ERR(code, ctxPtr)                                                                    \
    do                                                                                             \
    {                                                                                              \
        std::string _ctx_str;                                                                      \
        if (ctxPtr)                                                                                \
        {                                                                                          \
            try                                                                                    \
            {                                                                                      \
                _ctx_str = (ctxPtr)->getText();                                                    \
            }                                                                                      \
            catch (...)                                                                            \
            {                                                                                      \
            }                                                                                      \
        }                                                                                          \
        throw compile_error((code), std::move(_ctx_str), __FILE__, __LINE__, __func__);            \
    } while (0)

// 无上下文场景
#define THROW_ERR_NOCTX(code) throw compile_error((code), {}, __FILE__, __LINE__, __func__)
#define THROW_ERR_INFO(info) throw compile_error((error::unknow), info, __FILE__, __LINE__, __func__)