int (*funptrarr[4])(int, int);
int (*funptrarr[4])(int arg1, int arg2);
int arr[12][14] = {};
int arr2[12][15];
int* ptrarr[12];
int** a;
int (*funcdecl2())(int, int);
int funcdef()
{
    int a;
    int b;
    int c;
}
/*
Type        <- ArrType / PointerType / BasicType
ArrType     <- ArrSubType  '[' expr ']'
PointerType <- PtrSubType '*'
ArrSubType  <- PointerType / BasicType
PointerSubType <- BasicType
BasicType   <- 'int'
%whitespace <- [ \t\r\n]*
*/