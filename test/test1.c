int a;
int fun(int in)
{
    int ss;
    if (in == 1)
    {
        return 1;
    }
    else
    {
        return in * fun(in - 1);
    }
}

int main()
{
    a = 3;
    return fun(a);
}