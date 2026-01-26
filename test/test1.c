struct tests
{
    int a;
    char b;
};
struct tests c;
// int a = 12, *b = &a, arr[12] = {1, 2};
int main()
{
    c.b = 12;
    c.a = 13;
    return c.b;
}
