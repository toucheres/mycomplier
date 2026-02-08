cmake_minimum_required(VERSION 3.10)
project(mycomplier)

# 设置 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在使用 GNU 编译器且为 Debug 构建时启用 gprof 支持（-pg）
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -pg")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -pg")
  endif()
endif()

# 首先查找 ANTLR4 运行时头文件
find_path(ANTLR4_INCLUDE_DIR
  NAMES antlr4-runtime.h
  PATHS 
    /antlr4-runtime
    /usr/local/include/antlr4-runtime
    /usr/include/antlr4-runtime
)

if(NOT ANTLR4_INCLUDE_DIR)
  message(FATAL_ERROR "ANTLR4 runtime headers not found")
else()
  message(STATUS "Found ANTLR4 headers: ${ANTLR4_INCLUDE_DIR}")
endif()

# 查找 ANTLR4 库
find_package(antlr4-runtime CONFIG QUIET)

# 添加 include 目录为头文件搜索路径 - 在这里添加 ANTLR4_INCLUDE_DIR
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/generated
  ${CMAKE_SOURCE_DIR}/grammar/Cpp
  ${ANTLR4_INCLUDE_DIR}
)

# 启用 CTest 测试框架
enable_testing()

# 查找 ANTLR4 工具
find_program(ANTLR_EXECUTABLE antlr4 HINTS /usr/local/bin)
if(NOT ANTLR_EXECUTABLE)
  message(FATAL_ERROR "ANTLR4 executable not found. Please install ANTLR4.")
endif()

# 设置 ANTLR4 生成目录
set(ANTLR_GENERATED_DIR ${CMAKE_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${ANTLR_GENERATED_DIR})

# 定义语法文件（分为 lexer 与 parser）
set(GRAMMAR_FILES
  ${CMAKE_SOURCE_DIR}/grammar/CLexer.g4
  ${CMAKE_SOURCE_DIR}/grammar/CParser.g4
)

# 在生成源文件前先复制 ANTLR4 头文件到生成目录
# 这是最关键的修改 - 使用 file(COPY) 代替符号链接
add_custom_command(
  OUTPUT ${ANTLR_GENERATED_DIR}/antlr4-runtime.h
  COMMAND ${CMAKE_COMMAND} -E copy ${ANTLR4_INCLUDE_DIR}/antlr4-runtime.h ${ANTLR_GENERATED_DIR}/antlr4-runtime.h
  COMMENT "Copying antlr4-runtime.h to generated directory"
  DEPENDS ${ANTLR4_INCLUDE_DIR}/antlr4-runtime.h
)

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
    ${ANTLR_GENERATED_DIR}/antlr4-runtime.h
  COMMENT 
    "Generating parser from ${GRAMMAR_FILES}"
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

# 链接 ANTLR4 运行时
if(antlr4-runtime_FOUND)
  target_link_libraries(mycomplier PRIVATE antlr4-runtime)
else()
  # 如果找不到 find_package，则手动查找并链接
  find_library(ANTLR4_RUNTIME_LIB
    NAMES antlr4-runtime
    PATHS /usr/local/lib
  )
  
  if(NOT ANTLR4_RUNTIME_LIB)
    message(FATAL_ERROR "ANTLR4 runtime library not found")
  endif()
  
  target_link_libraries(mycomplier PRIVATE ${ANTLR4_RUNTIME_LIB})
endif()
find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(mycomplier PRIVATE Boost::program_options)

# 添加 test 子目录以构建并注册测试
add_subdirectory(${CMAKE_SOURCE_DIR}/test)