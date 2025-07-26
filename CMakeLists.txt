cmake_minimum_required(VERSION 3.10)
project(mycomplier)

# 自动收集 include 下所有头文件
file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/include/*")
# 自动收集 src 下所有源文件
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*")

add_executable(mycomplier ${SRC_FILES} ${HEADER_FILES})
