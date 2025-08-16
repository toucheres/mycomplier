
int fun(int in)
{
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
    return fun(5);
}