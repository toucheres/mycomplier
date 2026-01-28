typedef long size_t;
long strlen(char* str)
{
    long lenth = 0;
    while (*str != 0)
    {
        lenth = lenth + 1;
        str = str + 1;
    }
    return lenth;
}
char* strcpy(char* to, char* from)
{
    char* tmp = to;
    while (*from != 0)
    {
        *to = *from;
        from++;
        to++;
    }
    return tmp;
}
char* memmove(char* des, char* src, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        des[i] = src[i];
    }
    return des;
}