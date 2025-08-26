cmake_minimum_required(VERSION 3.10)
project(mycomplier)

# 设置 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动收集 src 下所有源文件
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*")

# 添加 include 目录为头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add third-party libraries
add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/cpp-peglib)

# 设置严格警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    # -Wall -Wextra -Wpedantic 
    # # -Werror
    # -Wshadow -Wformat=2 -Wunused
    # -Wcast-align -Wconversion -Wsign-conversion
  )
  
  # 可选：运行时检查（需要在编译和链接时都添加）
  # add_compile_options(-fsanitize=address -fsanitize=undefined)
  # add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

add_executable(mycomplier ${SRC_FILES})

# Link peglib interface target
target_link_libraries(mycomplier PRIVATE peglib)