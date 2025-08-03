int test_fun(int a, int b)
{
    int c;
    c = 100;
    return a + b * 2 + c;
}

int main()
{
    int test_var_a;
    int b;
    test_var_a = 13;
    b = test_fun(test_var_a, 13);
    return b + 1;
}
// TranslationUnitDecl 0x55776fe33ad8 <<invalid sloc>> <invalid sloc>
// |-TypedefDecl 0x55776fe34308 <<invalid sloc>> <invalid sloc> implicit __int128_t '__int128'
// | `-BuiltinType 0x55776fe340a0 '__int128'
// |-TypedefDecl 0x55776fe34378 <<invalid sloc>> <invalid sloc> implicit __uint128_t 'unsigned
// __int128' | `-BuiltinType 0x55776fe340c0 'unsigned __int128'
// |-TypedefDecl 0x55776fe34680 <<invalid sloc>> <invalid sloc> implicit __NSConstantString 'struct
// __NSConstantString_tag' | `-RecordType 0x55776fe34450 'struct __NSConstantString_tag' | `-Record
// 0x55776fe343d0 '__NSConstantString_tag'
// |-TypedefDecl 0x55776fe34728 <<invalid sloc>> <invalid sloc> implicit __builtin_ms_va_list 'char
// *' | `-PointerType 0x55776fe346e0 'char *' |   `-BuiltinType 0x55776fe33b80 'char'
// |-TypedefDecl 0x55776fe34a20 <<invalid sloc>> <invalid sloc> implicit __builtin_va_list 'struct
// __va_list_tag[1]' | `-ConstantArrayType 0x55776fe349c0 'struct __va_list_tag[1]' 1 | `-RecordType
// 0x55776fe34800 'struct __va_list_tag' |     `-Record 0x55776fe34780 '__va_list_tag'
// |-VarDecl 0x55776fe93d70 <./test/test1.c:1:1, col:18> col:5 used test_var_a 'int' cinit
// | `-IntegerLiteral 0x55776fe93e20 <col:18> 'int' 12
// |-FunctionDecl 0x55776fe93fb8 <line:3:1, line:13:1> line:3:5 used test_fun 'int (int, int)'
// | |-ParmVarDecl 0x55776fe93e58 <col:14, col:18> col:18 used a 'int'
// | |-ParmVarDecl 0x55776fe93ed8 <col:21, col:25> col:25 used b 'int'
// | `-CompoundStmt 0x55776fe94288 <line:4:1, line:13:1>
// |   `-IfStmt 0x55776fe94258 <line:5:5, line:12:5> has_else
// |     |-BinaryOperator 0x55776fe940e0 <line:5:9, col:13> 'int' '>'
// |     | |-ImplicitCastExpr 0x55776fe940b0 <col:9> 'int' <LValueToRValue>
// |     | | `-DeclRefExpr 0x55776fe94070 <col:9> 'int' lvalue ParmVar 0x55776fe93e58 'a' 'int'
// |     | `-ImplicitCastExpr 0x55776fe940c8 <col:13> 'int' <LValueToRValue>
// |     |   `-DeclRefExpr 0x55776fe94090 <col:13> 'int' lvalue ParmVar 0x55776fe93ed8 'b' 'int'
// |     |-CompoundStmt 0x55776fe941e0 <line:6:5, line:8:5>
// |     | `-ReturnStmt 0x55776fe941d0 <line:7:9, col:24>
// |     |   `-BinaryOperator 0x55776fe941b0 <col:16, col:24> 'int' '+'
// |     |     |-ImplicitCastExpr 0x55776fe94198 <col:16> 'int' <LValueToRValue>
// |     |     | `-DeclRefExpr 0x55776fe94100 <col:16> 'int' lvalue ParmVar 0x55776fe93e58 'a' 'int'
// |     |     `-BinaryOperator 0x55776fe94178 <col:20, col:24> 'int' '*'
// |     |       |-ImplicitCastExpr 0x55776fe94160 <col:20> 'int' <LValueToRValue>
// |     |       | `-DeclRefExpr 0x55776fe94120 <col:20> 'int' lvalue ParmVar 0x55776fe93ed8 'b'
// 'int' |     |       `-IntegerLiteral 0x55776fe94140 <col:24> 'int' 2 |     `-CompoundStmt
// 0x55776fe94240 <line:10:5, line:12:5> |       `-ReturnStmt 0x55776fe94230 <line:11:9, col:16> |
// `-ImplicitCastExpr 0x55776fe94218 <col:16> 'int' <LValueToRValue> |           `-DeclRefExpr
// 0x55776fe941f8 <col:16> 'int' lvalue Var 0x55776fe93d70 'test_var_a' 'int'
// `-FunctionDecl 0x55776fe942f8 <line:15:1, line:21:1> line:15:5 main 'int ()'
//   `-CompoundStmt 0x55776fe94600 <line:16:1, line:21:1>
//     |-DeclStmt 0x55776fe94420 <line:17:5, col:10>
//     | `-VarDecl 0x55776fe943b8 <col:5, col:9> col:9 used b 'int'
//     |-BinaryOperator 0x55776fe94478 <line:18:5, col:18> 'int' '='
//     | |-DeclRefExpr 0x55776fe94438 <col:5> 'int' lvalue Var 0x55776fe93d70 'test_var_a' 'int'
//     | `-IntegerLiteral 0x55776fe94458 <col:18> 'int' 13
//     |-BinaryOperator 0x55776fe945b0 <line:19:5, col:32> 'int' '='
//     | |-DeclRefExpr 0x55776fe94498 <col:5> 'int' lvalue Var 0x55776fe943b8 'b' 'int'
//     | `-CallExpr 0x55776fe94568 <col:9, col:32> 'int'
//     |   |-ImplicitCastExpr 0x55776fe94550 <col:9> 'int (*)(int, int)' <FunctionToPointerDecay>
//     |   | `-DeclRefExpr 0x55776fe944b8 <col:9> 'int (int, int)' Function 0x55776fe93fb8
//     'test_fun' 'int (int, int)' |   |-ImplicitCastExpr 0x55776fe94598 <col:18> 'int'
//     <LValueToRValue> |   | `-DeclRefExpr 0x55776fe944d8 <col:18> 'int' lvalue Var 0x55776fe93d70
//     'test_var_a' 'int' |   `-IntegerLiteral 0x55776fe944f8 <col:30> 'int' 13
//     `-ReturnStmt 0x55776fe945f0 <line:20:5, col:12>
//       `-IntegerLiteral 0x55776fe945d0 <col:12> 'int' 0