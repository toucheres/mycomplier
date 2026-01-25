struct tests
{
    int a;
    char b;
};
struct tests c;
int main()
{
    c.a = 12;
    return c.a;
}
// int main()
// {
//     int a = 12;
//     a = 13;
//     return a;
// }