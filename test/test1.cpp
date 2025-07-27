#include <iostream>
#define testdef main_
#define main_ main__
int testdef()
{
    std::cout << "ciallo world" << '\n';
    return 0;
}