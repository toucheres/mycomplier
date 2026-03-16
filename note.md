语言:cpp antlr4

1.词法分析/预处理

NOTE: 

c语言的预处理阶段的词法分析与编译阶段的词法分析不同, 本文之后的“词法分析”若未特地声明均为编译阶段的词法分析

将源码切分为满足特定正则表达式的token的过程

[TODO]正则表达式->NFA->DFA...

对于c语言而言，预处理通常应先于词法分析进行，而antlr4示例将预处理集成进入tokenize中但未保留行、列号信息

2.语法分析

将token流转化为AST的过程, 主流算法为自上而下或自下而上算法

2.1.自上而下算法

以antlr4为例, antlr4使用自适应LL(\*) 即 ALL(\*) 算法

c语言使用 自上而下算法 解进行析时主要有两个难点:

- 左递归的消除

- 二义性的消除 

2.1.1左递归: 左递归是[上下文无关文法](https://baike.baidu.com/item/上下文无关文法/2001908?fromModule=lemma_inlink)中非终端符号通过推导使句型最左端再次出现自身的递归形式

例如使用 expr = expr + num (此时''+''为左结合) 解析 1 + 2 + 3时

当解析器解析到expr时，会在未消费token情况下尝试匹配解析expr, 造成无限递归

解决方案: 将左递归改为循环

expr = num (+ num)*

NOTE: expr = num (+ num)\*与expr = expr + num 生成的ast结构不同, expr = expr + num 为二叉树结构，而expr = num (+ num)\* 是单层n叉树，没有保留优先级信息，需要手动处理. 而antlr实际上支持(直接)左递归并生成对应ast, 注意默认左结合, 使用<assoc=right>传递右结合属性

```
e : e '*' e
  | e '+' e
  |<assoc=right> e '?' e ':' e
  |<assoc=right> e '=' e
  | INT
  ;
```

在antlr官方仓库[TODO]示例中，考虑到兼容性与可读性，仍然手动去除左递归并写全优先级:

````antlr
........

// ISO C: inclusive-OR-expression (6.5.13)
inclusiveOrExpression
    : exclusiveOrExpression ('|' exclusiveOrExpression)*
    ;

// ISO C: logical-AND-expression (6.5.14)
logicalAndExpression
    : inclusiveOrExpression ('&&' inclusiveOrExpression)*
    ;

// ISO C: logical-OR-expression (6.5.15)
logicalOrExpression
    : logicalAndExpression ('||' logicalAndExpression)*
    ;

// ISO C: conditional-expression (6.5.16)
conditionalExpression
    : logicalOrExpression ('?' expression ':' conditionalExpression)?
    ;

// ISO C: assignment-expression (6.5.17.1)
assignmentExpression
    : conditionalExpression
    | unaryExpression assignementOperator=('=' | '*=' | '/=' | '%=' | '+=' | '-=' | '<<=' | '>>=' | '&=' | '^=' | '|=') assignmentExpression
    | DigitSequence // for
    ;

// ISO C: assignment-operator (6.5.17.1) - No ANTLR4 rule

// ISO C: expression (6.5.18)
expression
    : assignmentExpression (',' assignmentExpression)*
    ;
````

2.1.2二义性问题

c语言不是严格的上下文无关:

```
int T,t;
T*t; // expr * expr
```

```
typedef int T;
T*t; // type pointer id
```

这意味着在语法分析阶段就需要严格区分symbol

事实上主流c编译器通过手写递归下降高度将语法分析与语义分析耦合，在解析T时立刻查看符号表并将其解释为type，考虑到可读性与工作量，我们仍然将语义分析与语法分析视为两个阶段, 但符号表仍然是不可或缺的:

```
// Symbol.h:
class Symbol {
public:
    Symbol() = default;

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::unordered_set<TypeClassification>& getClassification() const { return classification_; }
    void setClassification(const std::unordered_set<TypeClassification>& classification) { classification_ = classification; }

    const std::unordered_map<std::string, std::shared_ptr<Symbol>>& getMembers() const { return members_; }
    std::unordered_map<std::string, std::shared_ptr<Symbol>>& getMembers() { return members_; }

    Symbol* getParent() const { return parent_; }
    void setParent(Symbol* parent) { parent_ = parent; }

    bool isPredefined() const { return predefined_; }
    void setPredefined(bool predefined) { predefined_ = predefined; }

    const std::string& getDefinedFile() const { return definedFile_; }
    void setDefinedFile(const std::string& file) { definedFile_ = file; }

    int getDefinedLine() const { return definedLine_; }
    void setDefinedLine(int line) { definedLine_ = line; }

    int getDefinedColumn() const { return definedColumn_; }
    void setDefinedColumn(int column) { definedColumn_ = column; }

    std::string toString() const;

private:
    std::string name_;
    std::unordered_set<TypeClassification> classification_;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> members_;
    Symbol* parent_ = nullptr;
    bool predefined_ = false;
    std::string definedFile_;
    int definedLine_ = 0;
    int definedColumn_ = 0;
};
// SymbolTable.h:
class SymbolTable {
public:
    SymbolTable();

    void enterScope(std::shared_ptr<Symbol> newScope);
    void exitScope();
    std::shared_ptr<Symbol> currentScope() const;

    bool define(std::shared_ptr<Symbol> symbol);
    bool defineInScope(std::shared_ptr<Symbol> currentScope, std::shared_ptr<Symbol> symbol);

    std::shared_ptr<Symbol> resolve(const std::string& name, std::shared_ptr<Symbol> startScope = nullptr) const;

    std::shared_ptr<Symbol> pushBlockScope();
    void popBlockScope();

    std::string toString() const;

private:
    static std::shared_ptr<Symbol> createSymbol(const std::string& name, std::initializer_list<TypeClassification> classifications);

    void toStringHelper(std::ostringstream& sb, const std::shared_ptr<Symbol>& scope, int depth) const;

    // Use a vector as a stack so we can iterate from bottom to top
    std::vector<std::shared_ptr<Symbol>> scopeStack_;
    int blockCounter_ = 0;
};

```

3.语法分析阶段

3.1c的声明与类型系统

c语言的类型复杂，这里只考虑数组, 指针, typedef与一些基本类型做一个简化示例:

```
declaration
	: declarationSpecifiers initDeclaratorList? ';'
	;
declarationSpecifiers
    :  declarationSpecifier+
    ;
declarationSpecifier
    : typeSpecifier
    ;
typeSpecifier
    : 'void'
    | 'char'
    | 'short'
    | 'int'
    | 'long'
    | 'float'
    | 'double'
    | 'signed'
    | 'unsigned'
    | Bool
    | typedefName
    ;
typedefName
    : Identifier
    ;
```



