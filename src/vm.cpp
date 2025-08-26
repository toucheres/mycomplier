#include "vm.h"
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

bool VM::setlogpath(std::string path)
{
    // 创建新的文件输出流
    static std::ofstream* file_stream = nullptr;

    // 关闭并删除之前可能打开的文件流
    if (file_stream)
    {
        file_stream->close();
        delete file_stream;
    }

    // 创建新文件流
    file_stream = new std::ofstream(path);

    // 检查文件是否成功打开
    if (!file_stream->is_open())
    {
        std::cerr << "Failed to open log file: " << path << std::endl;
        delete file_stream;
        file_stream = nullptr;
        return false;
    }

    // 重定向 logout 到新的文件流缓冲区
    logout.rdbuf(file_stream->rdbuf());

    return true;
}
