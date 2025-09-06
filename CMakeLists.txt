cmake_minimum_required(VERSION 3.10)
project(mycomplier)

# 设置 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
  ${ANTLR4_INCLUDE_DIR}
)

# 查找 ANTLR4 工具
find_program(ANTLR_EXECUTABLE antlr4 HINTS /usr/local/bin)
if(NOT ANTLR_EXECUTABLE)
  message(FATAL_ERROR "ANTLR4 executable not found. Please install ANTLR4.")
endif()

# 设置 ANTLR4 生成目录
set(ANTLR_GENERATED_DIR ${CMAKE_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${ANTLR_GENERATED_DIR})

# 定义语法文件
set(GRAMMAR_FILE ${CMAKE_SOURCE_DIR}/grammar/Complier.g4)

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
    ${ANTLR_GENERATED_DIR}/ComplierLexer.cpp
    ${ANTLR_GENERATED_DIR}/ComplierParser.cpp
    ${ANTLR_GENERATED_DIR}/ComplierBaseVisitor.cpp
    ${ANTLR_GENERATED_DIR}/ComplierVisitor.cpp
  COMMAND 
    ${ANTLR_EXECUTABLE} -Dlanguage=Cpp -visitor -no-listener -o ${ANTLR_GENERATED_DIR} ${GRAMMAR_FILE}
  DEPENDS 
    ${GRAMMAR_FILE}
    ${ANTLR_GENERATED_DIR}/antlr4-runtime.h
  COMMENT 
    "Generating parser from ${GRAMMAR_FILE}"
)

# 将生成的目录添加到包含路径
# 这里不需要重复，因为已经在前面添加了

# Add third-party libraries
add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/cpp-peglib)

# 自动收集 src 下所有源文件
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*")

# 添加生成的 ANTLR4 源文件
set(PARSER_SOURCES
  ${ANTLR_GENERATED_DIR}/ComplierLexer.cpp
  ${ANTLR_GENERATED_DIR}/ComplierParser.cpp
  ${ANTLR_GENERATED_DIR}/ComplierBaseVisitor.cpp
  ${ANTLR_GENERATED_DIR}/ComplierVisitor.cpp
)

# 创建可执行文件，包括源文件和生成的解析器代码
add_executable(mycomplier ${SRC_FILES} ${PARSER_SOURCES})

# 链接 ANTLR4 运行时
if(antlr4-runtime_FOUND)
  target_link_libraries(mycomplier PRIVATE peglib antlr4-runtime)
else()
  # 如果找不到 find_package，则手动查找并链接
  find_library(ANTLR4_RUNTIME_LIB
    NAMES antlr4-runtime
    PATHS /usr/local/lib
  )
  
  if(NOT ANTLR4_RUNTIME_LIB)
    message(FATAL_ERROR "ANTLR4 runtime library not found")
  endif()
  
  target_link_libraries(mycomplier PRIVATE peglib ${ANTLR4_RUNTIME_LIB})
endif()