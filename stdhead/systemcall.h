char* __malloc(char in);
void __free(char* ptr);
void __write(char in);
long __open(char* file, char* mode);
int __close(long fp);
int __putc(int c, long fp);
int __getc(long fp);
long __read(long fp, char* buf, long size);
long __write_file(long fp, char* buf, long size);