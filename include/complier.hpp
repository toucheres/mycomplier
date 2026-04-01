// 只支持int/char[*]类型
// 将一个char作为内存最小单位 指针,int大小均为4
// 函数调用:
// 调用fun(int a,int b,...)
// caller中:
// 计算a
// lea&li a    stack: a
// 计算b
// lea&li b    stack: a   b
//...
// call addr<fun>
// 低地址 +----------------+
//       |     参数 N     |
//       |     参数...    |
//       |     参数 1     | -12
//       +----------------+
//       |  返回地址(pc+1) | -8
//       +----------------+
//       |   旧的BP值     |  -4
// 高地址 +----------------+<- 新的BP和SP都指向这里

// 低地址 +----------------+
//       |     参数 N     |
//       |     参数...    |
//       |     参数 1     | -12
//       +----------------+
//       |  返回地址(pc+1) | -8
//       +----------------+
//       |   旧的BP值     |  -4
//       +----------------+      bp
//       |   tpvar0       |  0
//       |   tpvar1       |  4
// 高地址 +----------------+<-  sp
// fun中: a=bp[-(n*4)] b=bp[-((n-1)*4)]... retaddr=bp[0] obp=bp[4]
// nargs n  分配n个参数
// ret ax携带返回值,jump bp[0]

// dargs n 弹出n个参数
// [可选] push ax->stack压回返回值

// 编译时的空间分配:
// 全局var:直接访问  IMM + 数 LI 访问
// funvar:bp+偏移   LEA + 数 LI 访问
// funvar初始stack大小为8,为 obp opc+1预留位置
#pragma once
#include "obj.h"
#include <climits>
#include <peglib.h>
#include <string>
#include <vector>
#include <vm.h>
// AST 节点基类

struct exefile
{
    size_t global_size = 0;
    std::vector<std::string> asms;
};
struct linker
{
    std::unordered_map<std::string, size_t> addrmap;
    std::vector<OBJ>& objs;
    std::vector<std::string> mainargs;
    exefile exe;
    size_t pushfunc(std::string funcname);
    std::vector<std::string> process();
    linker(std::vector<OBJ>& ins, std::vector<std::string> amainargs)
        : objs(ins), mainargs(amainargs)
    {
    }
};
struct complier
{
    static std::vector<std::string> process(std::vector<std::string> paths);

  private:
    static void printAST(antlr4::tree::ParseTree* tree);
    static std::vector<std::string> process(std::vector<std::string> paths, bool showASt,
                                            int tolerate, bool showFoldedNames, bool disableStd,
                                            std::vector<std::string> mainargs,
                                            bool preprocess_only);
};