#include <stdarg.h>
#include <stdio.h>

// 实现一个简单的求和函数
int sum(int count, ...)
{
    va_list args;          // 定义可变参数列表
    va_start(args, count); // 初始化可变参数列表

    int total = 0;
    for (int i = 0; i < count; i++)
    {
        total += va_arg(args, int); // 获取当前参数
    }

    va_end(args); // 清理可变参数列表
    return total;
}

int main()
{
    printf("Sum: %d\n", sum(4, 1, 2, 3, 4)); // 输出：Sum: 10
    return 0;
}