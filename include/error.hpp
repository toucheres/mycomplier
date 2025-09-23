#pragma once
enum class error
{
    file_not_exsist,
    empty_node,
    undifined_type,
    undifined_func,
    undifined_var,
    undifined_id,
    undifined_obj_init_fun,
    double_defined,
    double_type,
    cpu_error,
    illegal_calcu,
    unsurpport_basictype,
    unsurpport_directDeclarator,
    unsurpport_abstractDeclarator,
    unsurpported_op,
    invalid_constant,
    expected_lvalue,
    expected_arr_initor,
    expected_func_or_funcptr,
    expected_ptr,
    unsurpported_num
};