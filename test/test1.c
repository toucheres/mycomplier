int fun2()
{
    return 2;
}
int fun1()
{
    return fun2();
}
int main()
{
    return fun1();
}