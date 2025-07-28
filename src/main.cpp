#include "preprocessor.hpp"
#include "tokenprocessor.h"
#include <iostream>
int main()
{
    Preprocessor p{};
    p.process("/home/toucher/vscoderope/mycomplier/test/test1.c");
    auto ret = Tokenprocessor::process("/home/toucher/vscoderope/mycomplier/test/test1.c.pre");
    for(auto& each: ret.value())
    {
        std::cout << each<<' ';
    }
    return 0;
}