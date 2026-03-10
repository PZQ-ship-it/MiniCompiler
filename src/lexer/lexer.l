%option noinput
%option nounput
%option noyywrap
%option yylineno

%{
#include "ast/tree.hpp"
#include "parser/parser.tab.hh"
#include <iostream>
extern FILE *input;
%}

digit [0-9]
blank [ \t\n]
letter [_a-zA-Z]
linecomment \/\/[^\n]*
comment \/\*([^*]|\*[^/])*\*\/
identifier {letter}({letter}|{digit})*

%%

"0"[0-7]*       { yylval.int_val = (int)strtol(yytext, NULL, 8); return INTCONST; }  // 八进制
"0"[xX][0-9a-fA-F]+ { yylval.int_val = (int)strtol(yytext, NULL, 16); return INTCONST; }  // 十六进制
[1-9]{digit}*   { yylval.int_val = atoi(yytext); return INTCONST; }  // 十进制
"+"             { return ADD; }
"-"             { return SUB; }
"*"             { return MUL; }
"/"             { return DIV; }
"%"             { return MOD; }
"!"             { return NOT; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
"="             { return ASSIGN; }
","             { return COMMA; }
";"             { return SEMICOLON; }
"<"             { return LT; }
"<="            { return LE; }
">"             { return GT; }
">="            { return GE; }
"=="            { return EQ; }
"!="            { return NE; }
"&&"            { return AND; }
"||"            { return OR; }
"return"        { return RETURN; }
"int"           { return INT; }
"void"          { return VOID; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
{identifier}    { yylval.str_val = strdup(yytext); return IDENT; }
{linecomment}   { }
{comment}       { }
{blank}         { }
.               { throw std::runtime_error("Unknown token '" + std::string(yytext) + "' at line " + std::to_string(yylineno)); }

%%

