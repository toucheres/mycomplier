#ifndef STDLIB_H
#define STDLIB_H
char* malloc(long in);
void free(char* ptr);
char* memcpy(char* dest, char* src, long count);
int memcmp(char* dest, char* src, long count);
char* memset(char* s, int c, long n);
void exit(int code);
#endif