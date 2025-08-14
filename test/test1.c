int add(int a, int b)
{
    return a + b;
}
int main()
{
    int a;
    a = 2;
    int b;
    b = add(1, a);
    return b;
}
// [0] JMP 11
// [1] NVAR 4
// [2] IMM 10
// [3] LEA 3
// [4] SI
// [5] LEA 0
// [6] LI
// [7] LEA 3
// [8] LI
// [9] ADD
// [10] RET
// [11] NVAR 3
// [12] IMM 12
// [13] CALL 1
// [14] LEA 2
// [15] SI
// [16] LEA 2
// [17] LI
// [18] RET