#pragma once
#include <string>
// S = var_decl | fun_decl
// var_decl = type [*] id[,[*]id];

class Complier
{
  public:
    int process(std::string args);
};