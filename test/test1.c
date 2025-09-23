int fun2()
{
    return 43;
}
int fun()
{
    return 42;
}
int main()
{
    int (*funcptr)() = &fun;
    int a;
    a = funcptr();
    int arr[12];
    int* p = arr;
    return a;
}