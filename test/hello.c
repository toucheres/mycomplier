int test_fun(int a, int b)
{
    int c;
    c = 100;
    return a + b * 2 + c;
}

int main()
{
    int test_var_a;
    int b;
    b = 12;
    test_var_a = 13;
    // b = test_fun(test_var_a, 13);
    // 12 + 13 * 2 + 14;
    return b + test_var_a * 2 + 14;
}
// int main()
// {
//     return 0;
// }