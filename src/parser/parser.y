%{
#include <cstdio>
#include <memory>
#include <string>
#include <iostream>
#include "ast/tree.hpp"
#define YYDEBUG 1
void yyerror(const char *s);
extern int yylex(void);
using namespace AST;
extern NodePtr root;

template <typename T>
inline std::shared_ptr<T> shared_cast(Node *ptr) {
  return std::shared_ptr<T>(static_cast<T *>(ptr));
}

%}

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 这里的 union 是一种特殊的数据结构，所有成员共享相同的内存地址
// 因此只能存储一个成员的值，且占用的内存大小等于其最大成员的大小
// union 中的类型必须是 trivially copyable 的类型
// 也就是说不能包含有自定义的构造函数、析构函数、虚函数等
// 符合这个条件的类型有基本数据类型、指针、C 结构体、枚举等
// 因此我们不能使用 std::shared_ptr，而只能使用普通指针
%union{
    int int_val;
    char *str_val;
    BinaryOp op;
    AST::Node *node;
}

%start AstRoot
%token ADD "+"
%token SUB "-"
%token MUL "*"
%token DIV "/"
%token MOD "%"
%token NOT "!"
%token ASSIGN "="
%token LT "<"
%token LE "<="
%token GT ">"
%token GE ">="
%token EQ "=="
%token NE "!="
%token AND "&&"
%token OR "||"
%token SEMICOLON ";"
%token COMMA ","
%token LPAREN "("
%token RPAREN ")"
%token LBRACE "{"
%token RBRACE "}"
%token LBRACKET "["
%token RBRACKET "]"
%token INT "int"
%token VOID "void"
%token RETURN "return"
%token IF "if"
%token ELSE "else"
%token WHILE "while"
%token <str_val> IDENT
%token <int_val> INTCONST

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <node> AstRoot CompUnit Decl VarDecl VarDefs VarDef FuncDef Block BlockItem BlockItems Stmt Exp LVal PrimaryExp IntConst UnaryExp FuncRParams MulExp AddExp FuncFParams FuncFParam Cond RelExp EqExp LAndExp LOrExp ArrayDims ArrayIndices InitVal InitValList
%type <op> UnaryOp

%%

// 由于我们在后续代码中所有指针都是 std::shared_ptr
// 而在 union 中没法直接定义 std::shared_ptr
// 所以我们在这里需要将普通的指针转换成 std::shared_ptr
AstRoot : CompUnit { root = NodePtr($1); }
    ;


CompUnit : FuncDef { $$ = new CompUnit(shared_cast<FuncDef>($1)); }
    | Decl { $$ = new CompUnit(NodePtr($1)); }  
    | CompUnit FuncDef { static_cast<CompUnit*>($1)->add_unit(shared_cast<FuncDef>($2)); $$ = $1; }
    | CompUnit Decl { static_cast<CompUnit*>($1)->add_unit(NodePtr($2)); $$ = $1; }  
    ;

Decl : VarDecl { $$ = $1; }
    ;

// 由于 union 中的类型不能是 std::shared_ptr
// 只是普通的指针，所以我们需要通过 static_cast 来转换类型
// 才能访问到对应的成员和函数
VarDecl : "int" VarDefs ";" { static_cast<VarDecl *>($2)->btype = BasicType::Int; $$ = $2; }
    ;

VarDefs : VarDef { $$ = new VarDecl(shared_cast<VarDef>($1)); }
    | VarDefs "," VarDef { static_cast<VarDecl*>($1)->add_def(shared_cast<VarDef>($3)); $$ = $1; }
    ;

VarDef : IDENT { $$ = new VarDef($1); }
    | IDENT "=" InitVal { $$ = new VarDefInit($1, NodePtr($3)); }
    | IDENT ArrayDims { 
        auto var_def = new VarDef($1); 
        var_def->is_array = true;
        var_def->array_dims = static_cast<ArrayDims*>($2)->dims;
        $$ = var_def;
    }
    | IDENT ArrayDims "=" InitVal { 
        auto var_def = new VarDefInit($1, NodePtr($4)); 
        var_def->is_array = true;
        var_def->array_dims = static_cast<ArrayDims*>($2)->dims;
        $$ = var_def;
    }
    ;

InitVal : Exp { $$ = $1; }
    | "{" "}" { $$ = new ArrayInitVal(); }
    | "{" InitValList "}" { $$ = $2; }
    ;

InitValList : InitVal { 
        $$ = new ArrayInitVal();
        static_cast<ArrayInitVal*>($$)->add_val(NodePtr($1));
    }
    | InitValList "," InitVal { 
        static_cast<ArrayInitVal*>($1)->add_val(NodePtr($3));
        $$ = $1;
    }
    ;

ArrayDims : "[" INTCONST "]" { 
        $$ = new ArrayDims();
        static_cast<ArrayDims*>($$)->add_dim($2);
    }
    | ArrayDims "[" INTCONST "]" { 
        static_cast<ArrayDims*>($1)->add_dim($3);
        $$ = $1;
    }
    ;

// 同样的，由于 union 中的类型不能是 std::shared_ptr
// 而 FuncDef 初始化时需要传入一个 BlockPtr (std::shared_ptr<Block>)
// 所以我们需要通过 static_cast 来转换类型
// 再用 std::shared_ptr 包装一下
// 才能传入 FuncDef 的构造函数
// 这里 shared_cast 是一个自定义的模板函数，用于将普通指针转换成 std::shared_ptr
// 相当于 std::shared_ptr<T>(static_cast<T*>(ptr))

FuncDef : "int" IDENT "(" ")" Block { $$ = new FuncDef(BasicType::Int, $2, shared_cast<Block>($5)); }
    | "int" IDENT "(" FuncFParams ")" Block { 
        auto func = new FuncDef(BasicType::Int, $2, shared_cast<Block>($6));
        func->params = static_cast<FuncFParams*>($4)->params;
        $$ = func;
    }
    | "void" IDENT "(" ")" Block { $$ = new FuncDef(BasicType::Void, $2, shared_cast<Block>($5)); }
    | "void" IDENT "(" FuncFParams ")" Block { 
        auto func = new FuncDef(BasicType::Void, $2, shared_cast<Block>($6));
        func->params = static_cast<FuncFParams*>($4)->params;
        $$ = func;
    }
    ;

FuncFParams : FuncFParam { $$ = new FuncFParams(shared_cast<FuncFParam>($1)); }
    | FuncFParams "," FuncFParam { static_cast<FuncFParams*>($1)->add_param(shared_cast<FuncFParam>($3)); $$ = $1; }
    ;

FuncFParam : "int" IDENT { $$ = new FuncFParam(BasicType::Int, $2); }
    | "int" IDENT "[" "]" { 
        auto param = new FuncFParam(BasicType::Int, $2);
        param->is_array = true;
        param->array_dims.clear();  
        $$ = param;
    }
    | "int" IDENT "[" "]" ArrayDims { 
        auto param = new FuncFParam(BasicType::Int, $2);
        param->is_array = true;
        param->array_dims = static_cast<ArrayDims*>($5)->dims;
        $$ = param;
    }
    ;

Block : "{" "}" { $$ = new Block(); }
    | "{" BlockItems "}" { $$ = $2; }
    ;

BlockItems : BlockItem { $$ = new Block(NodePtr($1)); }
    | BlockItems BlockItem { static_cast<Block*>($1)->add_stmt(NodePtr($2)); $$ = $1; }
    ;

BlockItem : Stmt { $$ = $1; }
    | Decl { $$ = $1; }
    ;


Stmt : LVal "=" Exp ";" { $$ = new AssignStmt(shared_cast<LVal>($1), NodePtr($3)); }
    | Exp ";" { $$ = $1; }
    | ";" { $$ = new EmptyStmt(); }  
    | Block { $$ = $1; }
    | "return" Exp ";" { $$ = new ReturnStmt(NodePtr($2)); }
    | "return" ";" { $$ = new ReturnStmt(); }
    | "if" "(" Cond ")" Stmt %prec LOWER_THAN_ELSE { $$ = new IfStmt(NodePtr($3), NodePtr($5)); }
    | "if" "(" Cond ")" Stmt "else" Stmt { $$ = new IfStmt(NodePtr($3), NodePtr($5), NodePtr($7)); }
    | "while" "(" Cond ")" Stmt { $$ = new WhileStmt(NodePtr($3), NodePtr($5)); }
    ;


Cond : LOrExp { $$ = $1; }
    ;

RelExp : AddExp { $$ = $1; }
    | RelExp "<" AddExp { $$ = new BinaryExp(BinaryOp::Lt, NodePtr($1), NodePtr($3)); }
    | RelExp "<=" AddExp { $$ = new BinaryExp(BinaryOp::Le, NodePtr($1), NodePtr($3)); }
    | RelExp ">" AddExp { $$ = new BinaryExp(BinaryOp::Gt, NodePtr($1), NodePtr($3)); }
    | RelExp ">=" AddExp { $$ = new BinaryExp(BinaryOp::Ge, NodePtr($1), NodePtr($3)); }
    ;

EqExp : RelExp { $$ = $1; }
    | EqExp "==" RelExp { $$ = new BinaryExp(BinaryOp::Eq, NodePtr($1), NodePtr($3)); }
    | EqExp "!=" RelExp { $$ = new BinaryExp(BinaryOp::Ne, NodePtr($1), NodePtr($3)); }
    ;

LAndExp : EqExp { $$ = $1; }
    | LAndExp "&&" EqExp { $$ = new BinaryExp(BinaryOp::And, NodePtr($1), NodePtr($3)); }
    ;

LOrExp : LAndExp { $$ = $1; }
    | LOrExp "||" LAndExp { $$ = new BinaryExp(BinaryOp::Or, NodePtr($1), NodePtr($3)); }
    ;

Exp : AddExp { $$ = $1; }
    ;

LVal : IDENT { $$ = new LVal($1); }
    | IDENT ArrayIndices { 
        auto lval = new LVal($1);
        lval->is_array_element = true;
        lval->indices = static_cast<ArrayIndices*>($2)->indices;
        $$ = lval;
    }
    ;

ArrayIndices : "[" Exp "]" { 
        $$ = new ArrayIndices();
        static_cast<ArrayIndices*>($$)->add_index(NodePtr($2));
    }
    | ArrayIndices "[" Exp "]" { 
        static_cast<ArrayIndices*>($1)->add_index(NodePtr($3));
        $$ = $1;
    }
    ;

PrimaryExp : LVal { $$ = $1; }
    | IntConst { $$ = $1; }
    | "(" Exp ")" { $$ = $2; }  
    ;

IntConst : INTCONST { $$ = new IntConst($1); }
    ;

UnaryExp : PrimaryExp { $$ = $1; }
    | IDENT "(" ")" { $$ = new FuncCall($1); }
    | IDENT "(" FuncRParams ")" { static_cast<FuncCall*>($3)->name = $1; $$ = $3; }
    | UnaryOp UnaryExp { $$ = new UnaryExp($1, NodePtr($2)); }
    ;

UnaryOp : "+" { $$ = BinaryOp::Add; }
    | "-" { $$ = BinaryOp::Sub; }
    | "!" { $$ = BinaryOp::Not; }
    ;

FuncRParams : Exp { $$ = new FuncCall(NodePtr($1)); }
    | FuncRParams "," Exp { static_cast<FuncCall*>($1)->add_arg(NodePtr($3)); $$ = $1; }
    ;

MulExp : UnaryExp { $$ = $1; }
    | MulExp "*" UnaryExp { $$ = new BinaryExp(BinaryOp::Mul, NodePtr($1), NodePtr($3)); }
    | MulExp "/" UnaryExp { $$ = new BinaryExp(BinaryOp::Div, NodePtr($1), NodePtr($3)); }
    | MulExp "%" UnaryExp { $$ = new BinaryExp(BinaryOp::Mod, NodePtr($1), NodePtr($3)); }
    ;

AddExp : MulExp { $$ = $1; }
    | AddExp "+" MulExp { $$ = new BinaryExp(BinaryOp::Add, NodePtr($1), NodePtr($3)); }
    | AddExp "-" MulExp { $$ = new BinaryExp(BinaryOp::Sub, NodePtr($1), NodePtr($3)); }
    ;

%%

void yyerror(const char *s) {
    printf("error: %s\n", s);
}
