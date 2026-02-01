#ifndef STDLIB_H
#define STDLIB_H
char* malloc(long in);
void free(char* ptr);
void* memcpy(void* dest, void* src, long count);
#endif