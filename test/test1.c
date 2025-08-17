int fun(int a)
{
    int tp;
    if (a == 1)
    {
        return 1;
    }
    else
    {
        return a * fun(a - 1);
    }
}
int main()
{
    return fun(4);
}