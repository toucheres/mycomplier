int gtp;
int fun(int a)
{
    int tp;
    if (a == 1)
    {
        return 1;
    }
    else
    {
        gtp = a * fun(a - 1);
        tp = gtp;
        return tp;
    }
}
int main()
{
    return fun(4);
}