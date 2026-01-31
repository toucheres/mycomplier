#include <stdio.h>
int main()
{
    FILE* fp = fopen("/home/toucher/vscoderope/mycomplier/test/testfile.txt", "r+");
    char arr[33];
    fread(arr, 1, 100, fp);
    printf(arr);
    char files[30] = "ciallo world\n";
    fwrite(files, 1, strlen(files), fp);
    return 0;
}