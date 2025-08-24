int fun(int arg1, int arg2)
{
    int localvar;
    localvar = arg1 + (arg2 * 2);
    return localvar;
}
int gvar1;
int gvar2;
int main()
{
    int mainvar;
    gvar1 = 1;
    gvar2 = 2;
    mainvar = fun(gvar1, gvar2) + 1;
    return mainvar;
}