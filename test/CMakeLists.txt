
cmake_minimum_required(VERSION 3.10)

# Register a CTest that runs the built `mycomplier` executable with `test1.c` as input
# CTest 会把非零返回码视为失败，零视为成功（不需要额外设置）
add_test(NAME run_mycomplier_with_test1
	COMMAND $<TARGET_FILE:mycomplier> ${CMAKE_SOURCE_DIR}/test/test1.c)

# 如果 mycomplier 输出中包含 "hello world" 则视为通过；否则失败。
    # add_test(NAME run_and_check_stdout
#   COMMAND $<TARGET_FILE:mycomplier> ${CMAKE_SOURCE_DIR}/test/test1.c
# )
# set_tests_properties(run_and_check_stdout PROPERTIES
#   PASS_REGULAR_EXPRESSION "hello world"
# )