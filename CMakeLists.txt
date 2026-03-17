cmake_minimum_required(VERSION 3.14)
project(mycomplier)

# 设置 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在使用 GNU 编译器且为 Debug/RelWithDebInfo 构建时启用 gprof 支持（-pg）
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -pg")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -pg")

    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -pg")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} -pg")
  endif()
endif()

# 查找 ANTLR4 工具
find_program(ANTLR_EXECUTABLE
  NAMES
    antlr4
  REQUIRED
)

# 定义语法文件（分为 lexer 与 parser）
set(GRAMMAR_FILES
  ${CMAKE_SOURCE_DIR}/grammar/CLexer.g4
  ${CMAKE_SOURCE_DIR}/grammar/CParser.g4
)

# 设置 ANTLR4 生成目录
set(ANTLR_GENERATED_DIR ${CMAKE_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${ANTLR_GENERATED_DIR})

# 生成 ANTLR4 解析器代码的自定义命令
add_custom_command(
  OUTPUT
    ${ANTLR_GENERATED_DIR}/CLexer.cpp
    ${ANTLR_GENERATED_DIR}/CParser.cpp
    ${ANTLR_GENERATED_DIR}/CParserBaseVisitor.cpp
    ${ANTLR_GENERATED_DIR}/CParserVisitor.cpp
  COMMAND 
    ${ANTLR_EXECUTABLE} -Dlanguage=Cpp -visitor -listener -o ${ANTLR_GENERATED_DIR} ${GRAMMAR_FILES}
  DEPENDS 
    ${GRAMMAR_FILES}
  COMMENT 
    "Generating parser from ${GRAMMAR_FILES}"
)

# 查找 ANTLR4 库
find_package(antlr4-runtime REQUIRED CONFIG QUIET)


# 添加 include 目录为头文件搜索路径
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/generated
  ${CMAKE_SOURCE_DIR}/grammar/Cpp
)

# 自动收集 src 下所有源文件
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*")

# grammar Cpp 支撑源文件（CLexerBase/CParserBase/SymbolTable）
set(CPP_GRAMMAR_SOURCES
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/CLexerBase.cpp
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/CParserBase.cpp
  ${CMAKE_SOURCE_DIR}/grammar/Cpp/SymbolTable.cpp
)

# 添加生成的 ANTLR4 源文件
set(PARSER_SOURCES
  ${ANTLR_GENERATED_DIR}/CLexer.cpp
  ${ANTLR_GENERATED_DIR}/CParser.cpp
  ${ANTLR_GENERATED_DIR}/CParserBaseVisitor.cpp
  ${ANTLR_GENERATED_DIR}/CParserVisitor.cpp
)

# 创建可执行文件，包括源文件和生成的解析器代码
add_executable(mycomplier ${SRC_FILES} ${CPP_GRAMMAR_SOURCES} ${PARSER_SOURCES})

# for #include "antlr4-runtime.h" insteaded of "antlr4/antlr4-runtime.h"
find_path(ANTLR4_INCLUDE_DIR NAMES antlr4-runtime.h HINTS ${CMAKE_PREFIX_PATH})
if(ANTLR4_INCLUDE_DIR)
    target_include_directories(mycomplier PRIVATE ${ANTLR4_INCLUDE_DIR})
else()
    message(FATAL_ERROR "antlr4 runtime not found. Install it or set CMAKE_PREFIX_PATH or ensure pkg-config exports antlr4-runtime.")
endif()
# 链接 ANTLR4 运行时
target_link_libraries(mycomplier PRIVATE antlr4-runtime)

find_package(Boost REQUIRED COMPONENTS program_options)

include(FetchContent)
FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG        v0.9.7
)
FetchContent_MakeAvailable(magic_enum)
target_link_libraries(mycomplier PRIVATE magic_enum Boost::program_options)

# 启用 CTest 测试框架
enable_testing()

# 添加 test 子目录以构建并注册测试
add_subdirectory(${CMAKE_SOURCE_DIR}/test)