
struct tests
{
    int a;
    char b;
    int c;
};
typedef struct tests sh;
int main()
{
    sh c = {1, 2, 3};
    return c.a;
}