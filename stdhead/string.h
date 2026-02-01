#ifndef STRING_H
#define STRING_H
typedef long size_t;
long strlen(char* str);
char* strcpy(char* to, char* from);
int strcmp(char* str1, char* str2);
char* memmove(char* des, char* src, size_t n);
#endif