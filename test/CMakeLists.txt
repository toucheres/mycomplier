# Smoke tests for the compiler.  The compiler embeds its source root
# (MYCOMPLIER_ROOT), so tests do not depend on the process working directory.
add_test(
    NAME mycomplier_hello
    COMMAND $<TARGET_FILE:mycomplier>
            ${CMAKE_SOURCE_DIR}/test/hello.c
            --print-ret-value
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(mycomplier_hello PROPERTIES
    PASS_REGULAR_EXPRESSION "ret: 52"
)

# A larger end-to-end input (the C4-style VM).  It is kept as a manual stress
# test because it is slow and its guest program still has its own bugs.
# add_test(
#     NAME mycomplier_test1
#     COMMAND $<TARGET_FILE:mycomplier>
#             ${CMAKE_SOURCE_DIR}/test/test1.c
#             --main-args ${CMAKE_SOURCE_DIR}/test/test1.c
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )
