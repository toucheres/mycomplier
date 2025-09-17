#pragma once
enum class error
{
    file_not_exsist,
    empty_node,
    undifined_type,
    undifined_func,
    undifined_var,
    undifined_obj_init_fun,
    double_defined,
    double_type,
    cpu_error,
    illegal_calcu,
    unsurpport_basictype,
    unsurpport_directDeclarator,
    unsurpport_abstractDeclarator,
    unsurpported_op,
    expected_lvalue,
    expected_func_or_funcptr,
};