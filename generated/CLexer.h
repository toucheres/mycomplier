#include "CLexerBase.h"

// Generated from grammar/CLexer.g4 by ANTLR 4.13.1

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public CLexerBase {
public:
  enum {
    Attribute = 1, KW__builtin_offsetof = 2, KW__builtin_va_arg = 3, KW__builtin_choose_expr = 4, 
    KW__builtin_types_compatible_p = 5, KW__builtin_tgmath = 6, KW__builtin_complex = 7, 
    KW__cdecl = 8, KW__clrcall = 9, KW__declspec = 10, KW__extension__ = 11, 
    KW__fastcall = 12, KW__m128 = 13, KW__m128d = 14, KW__m128i = 15, KW__stdcall = 16, 
    KW__thiscall = 17, KW__vectorcall = 18, KW__real__ = 19, KW__imag__ = 20, 
    KW__func__ = 21, KW__FUNCTION__ = 22, KW__PRETTY_FUNCTION__ = 23, Alignas = 24, 
    Alignof = 25, Asm = 26, Auto = 27, Bool = 28, Break = 29, Case = 30, 
    Char = 31, Const = 32, Constexpr = 33, Continue = 34, Default = 35, 
    Deprecated = 36, Do = 37, Double = 38, Else = 39, Enum = 40, Extern = 41, 
    False = 42, Float = 43, For = 44, Goto = 45, If = 46, Inline = 47, Int = 48, 
    Label = 49, Long = 50, Nulptr = 51, Register = 52, Restrict = 53, Return = 54, 
    Short = 55, Signed = 56, Sizeof = 57, Static = 58, Static_assert = 59, 
    Struct = 60, Switch = 61, True = 62, Typedef = 63, Typeof = 64, Typeof_unqual = 65, 
    Union = 66, Unsigned = 67, Void = 68, Volatile = 69, While = 70, Atomic = 71, 
    BitInt = 72, Complex = 73, Decimal128 = 74, Decimal32 = 75, Decimal64 = 76, 
    Generic = 77, Imaginary = 78, Noreturn = 79, StaticAssert = 80, ThreadLocal = 81, 
    LeftParen = 82, RightParen = 83, LeftBracket = 84, RightBracket = 85, 
    LeftBrace = 86, RightBrace = 87, Less = 88, LessEqual = 89, Greater = 90, 
    GreaterEqual = 91, LeftShift = 92, RightShift = 93, Plus = 94, PlusPlus = 95, 
    Minus = 96, MinusMinus = 97, Star = 98, Div = 99, Mod = 100, And = 101, 
    Or = 102, AndAnd = 103, OrOr = 104, Caret = 105, Not = 106, Tilde = 107, 
    Question = 108, Colon = 109, Semi = 110, Comma = 111, Assign = 112, 
    StarAssign = 113, DivAssign = 114, ModAssign = 115, PlusAssign = 116, 
    MinusAssign = 117, LeftShiftAssign = 118, RightShiftAssign = 119, AndAssign = 120, 
    XorAssign = 121, OrAssign = 122, Equal = 123, NotEqual = 124, Arrow = 125, 
    Dot = 126, Ellipsis = 127, Identifier = 128, IntegerConstant = 129, 
    FloatingConstant = 130, DigitSequence = 131, CharacterConstant = 132, 
    StringLiteral = 133, MultiLineMacro = 134, LineDirective = 135, Directive = 136, 
    Whitespace = 137, Newline = 138, BlockComment = 139, LineComment = 140
  };

  enum {
    LINEDIRECTIVECHANNEL = 2
  };

  explicit CLexer(antlr4::CharStream *input);

  ~CLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

