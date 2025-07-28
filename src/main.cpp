#include "preprocessor.hpp"
#include "tokenprocessor.h"
#include <iostream>
int main()
{
    Preprocessor p{};
    p.process("/home/toucher/vscoderope/mycomplier/test/test1.cpp");
    auto ret = Tokenprocessor::process("/home/toucher/vscoderope/mycomplier/test/test1.cpp.pre");
    int a;
    return 0;
}