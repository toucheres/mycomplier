#ifndef STDLIB_H
#define STDLIB_H
char* malloc(long in);
void free(char* ptr);
void* memcpy(void* dest, void* src, long count);
int memcmp(void* dest, void* src, long count);
void exit(int code);
#endif