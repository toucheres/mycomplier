cmake_minimum_required(VERSION 3.10)
project(mycomplier)

# 设置 C++23 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动收集 src 下所有源文件
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*")

# 添加 include 目录为头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(mycomplier ${SRC_FILES})
