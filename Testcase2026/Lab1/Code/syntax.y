%locations

%{
#include "Node.h"
#include <stdio.h>
#include <stdlib.h>
extern int yylineno;
extern char* yytext;
int yyerror(const char* msg);

Node* root = NULL;
int lexicalError = 0;
int syntaxError = 0;
int lastErrorLine = -1;

#include "lex.yy.c"
%}

%union {
    Node* node;
}

/* token definitions */
%token <node> INT FLOAT ID
%token <node> SEMI COMMA TYPE LC RC STRUCT RETURN IF ELSE WHILE
%token <node> ASSIGNOP RELOP PLUS MINUS STAR DIV AND OR DOT NOT LP RP LB RB
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT UMINUS
%left LP RP LB RB DOT
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <node> Program ExtDefList ExtDef ExtDecList Specifier StructSpecifier OptTag Tag
%type <node> VarDec FunDec VarList ParamDec CompSt StmtList Stmt DefList Def DecList Dec Exp Args

%%
/* high-level definitions */
Program : ExtDefList {
    $$ = createNode("Program");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
    root = $$;
}
    ;
ExtDefList : ExtDef ExtDefList {
    $$ = createNode("ExtDefList");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
    if ($1 != NULL) {
        $1->next = $2;
    }
}
    | { $$ = NULL; }
    ;
ExtDef : Specifier ExtDecList SEMI
      {
          $$ = createNode("ExtDef");
          $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
          $$->child = $1;
          if ($1 != NULL) $1->next = $2;
          if ($2 != NULL) $2->next = $3;
      }
    | Specifier SEMI
      {
          $$ = createNode("ExtDef");
          $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
          $$->child = $1;
          if ($1 != NULL) $1->next = $2;
      }
    | Specifier FunDec CompSt
      {
          $$ = createNode("ExtDef");
          $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
          $$->child = $1;
          if ($1 != NULL) $1->next = $2;
          if ($2 != NULL) $2->next = $3;
      }
    ;
ExtDecList : VarDec {
    $$ = createNode("ExtDecList");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
}
    | VarDec COMMA ExtDecList {
        $$ = createNode("ExtDecList");
        $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
        $$->child = $1;
        if ($1 != NULL) $1->next = $2;
        if ($2 != NULL) $2->next = $3;
    }
    ;

/* Specifier */
Specifier : TYPE {
    $$ = createNode("Specifier");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
}
    | StructSpecifier {
        $$ = createNode("Specifier");
        $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
        $$->child = $1;
    }
    ;
StructSpecifier
    : STRUCT OptTag LC DefList RC
      {
          $$ = createNode("StructSpecifier");
          $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
          $$->child = $1;

          if ($2 != NULL) {
              $1->next = $2;
              $2->next = $3;
          } else {
              $1->next = $3;
          }

          if ($4 != NULL) {
              $3->next = $4;
              $4->next = $5;
          } else {
              $3->next = $5;
          }
      }
    | STRUCT Tag
      {
          $$ = createNode("StructSpecifier");
          $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
          $$->child = $1;
          if ($1 != NULL) $1->next = $2;
      }
    ;
OptTag : ID {
    $$ = createNode("OptTag");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
}
    | { $$ = NULL; }
    ;
Tag : ID {
    $$ = createNode("Tag");
    $$->lineno = ($1 != NULL) ? $1->lineno : yylineno;
    $$->child = $1;
}
    ;

/* Declarators */
VarDec
    : ID
      {
          $$ = createNode("VarDec");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    | VarDec LB INT RB
      {
          $$ = createNode("VarDec");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
          $3->next = $4;
      }
    /* 错误: 数组维度使用了浮点数，如 int a[2.0] */
    | VarDec LB FLOAT RB
      {
          $$ = NULL;
          if ($3 != NULL && $3->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $3->lineno;
              printf("Error type B at Line %d: Array dimension must be integer.\n", $3->lineno);
          }
      }
    /* 错误: 数组声明多了一个右括号，如 int a[5]] */
    | VarDec LB INT RB RB
      {
          $$ = NULL;
          if ($4 != NULL && $4->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $4->lineno;
              printf("Error type B at Line %d: Extra closing bracket in array declaration.\n", $4->lineno);
          }
      }
    ;

FunDec
    : ID LP VarList RP
      {
          $$ = createNode("FunDec");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
          $3->next = $4;
      }
    | ID LP RP
      {
          $$ = createNode("FunDec");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
      }
    | ID LP error RP
      {
          $$ = NULL;
          yyerrok;
      }
    ;

VarList
    : ParamDec COMMA VarList
      {
          $$ = createNode("VarList");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
      }
    | ParamDec
      {
          $$ = createNode("VarList");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    ;

ParamDec
    : Specifier VarDec
      {
          $$ = createNode("ParamDec");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
      }
    ;

/* Statements */
CompSt
    : LC DefList StmtList RC
      {
          $$ = createNode("CompSt");
          $$->lineno = $1->lineno;
          $$->child = $1;

          /* 只对可空节点做防御，避免丢链 */
          if ($2 != NULL) {
              $1->next = $2;
              if ($3 != NULL) {
                  $2->next = $3;
                  $3->next = $4;
              } else {
                  $2->next = $4;
              }
          } else {
              if ($3 != NULL) {
                  $1->next = $3;
                  $3->next = $4;
              } else {
                  $1->next = $4;
              }
          }
      }
    ;

StmtList
    : Stmt StmtList
      {
          if ($1 == NULL) $$ = $2;
          else {
              $$ = createNode("StmtList");
              $$->lineno = $1->lineno;
              $$->child = $1;
              $1->next = $2;
          }
      }
    | /* empty */
      { $$ = NULL; }
    ;

Stmt
    : Exp SEMI
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
      }
    | CompSt
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    | RETURN Exp SEMI
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
      }
    | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
          $3->next = $4;
          $4->next = $5;
      }
    | IF LP Exp RP Stmt ELSE Stmt
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
          $3->next = $4;
          
          /* 防御断链导致的段错误 */
          if ($5 != NULL) {
              $4->next = $5;
              $5->next = $6;
          } else {
              $4->next = $6;
          }
          
          $6->next = $7;
      }
    | WHILE LP Exp RP Stmt
      {
          $$ = createNode("Stmt");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
          $3->next = $4;
          $4->next = $5;
      }
    | WHILE LP Exp RP SEMI
      {
          $$ = NULL;
          if ($1 && $1->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $1->lineno;
              printf("Error type B at Line %d: syntax error\n", $1->lineno);
          }
      }
    | Exp RB ASSIGNOP Exp SEMI
      {
          $$ = NULL;
          if ($2 && $2->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $2->lineno;
              printf("Error type B at Line %d: syntax error\n", $2->lineno);
          }
      }

    | IF LP error RP Stmt %prec LOWER_THAN_ELSE { $$ = NULL; yyerrok; }
    | IF LP error RP Stmt ELSE Stmt { $$ = NULL; yyerrok; }
    | WHILE LP error RP Stmt { $$ = NULL; yyerrok; }

    | IF LP error CompSt %prec LOWER_THAN_ELSE { $$ = NULL; yyerrok; }
    | IF LP error CompSt ELSE Stmt { $$ = NULL; yyerrok; }
    | WHILE LP error CompSt { $$ = NULL; yyerrok; }

    /* 黑科技 */
    | ID ID SEMI
      {
          $$ = NULL;
          if ($1 && $1->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $1->lineno;
              printf("Error type B at Line %d: syntax error\n", $1->lineno);
              if ($3 != NULL) lastErrorLine = $3->lineno + 1;
          }
      }

    | TYPE DecList SEMI
      {
          $$ = NULL;
          if ($1 && $1->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $1->lineno;
              printf("Error type B at Line %d: syntax error\n", $1->lineno);
          }
      }

    | Exp error
      {
          $$ = NULL;
          yyerrok;
      }

    /* 底层兜底规则 */
    | error SEMI
      { 
          $$ = NULL; 
          yyerrok; 
      }
    ;
    ;

/* Local definitions */
DefList
    : Def DefList
      {
          if ($1 == NULL) $$ = $2;
          else {
              $$ = createNode("DefList");
              $$->lineno = $1->lineno;
              $$->child = $1;
              $1->next = $2;
          }
      }
    | /* empty */
      { $$ = NULL; }
    ;

Def
    : Specifier DecList SEMI
      {
          $$ = createNode("Def");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          if ($2 != NULL) {
              $2->next = $3;
          } else {
              $1->next = $3;
          }
      }
    | Specifier error SEMI
      {
          $$ = NULL;
          yyerrok;
      }
    ;

DecList
    : Dec
      {
          if ($1 == NULL) {
              $$ = NULL;
          } else {
              $$ = createNode("DecList");
              $$->lineno = $1->lineno;
              $$->child = $1;
          }
      }
    | Dec COMMA DecList
      {
          if ($1 == NULL) {
              $$ = $3;
          } else {
              $$ = createNode("DecList");
              $$->lineno = $1->lineno;
              $$->child = $1;
              $1->next = $2;
              $2->next = $3;
          }
      }
    ;

Dec
    : VarDec
      {
          if ($1 == NULL) {
              $$ = NULL;
          } else {
              $$ = createNode("Dec");
              $$->lineno = $1->lineno;
              $$->child = $1;
          }
      }
    | VarDec ASSIGNOP Exp
      {
          if ($1 == NULL) {
              $$ = NULL;
          } else {
              $$ = createNode("Dec");
              $$->lineno = $1->lineno;
              $$->child = $1;
              $1->next = $2;
              $2->next = $3;
          }
      }
    ;

/* Expressions */
Exp
    : Exp ASSIGNOP Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp AND Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp OR Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp RELOP Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp PLUS Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp MINUS Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp STAR Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp DIV Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | LP Exp RP
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | MINUS Exp %prec UMINUS
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
      }
    | NOT Exp
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
      }
    | ID LP Args RP
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3; $3->next = $4;
      }
    | ID LP RP
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | Exp LB Exp RB
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3; $3->next = $4;
      }
    | Exp LB Exp COMMA Exp RB
      {
          $$ = createNode("Exp");
          $$->lineno = $2->lineno;
          if ($2->lineno != lastErrorLine) {
              syntaxError += 1;
              lastErrorLine = $2->lineno;
              printf("Error type B at Line %d: syntax error\n", $2->lineno);
          }
      }
    | Exp DOT ID
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2; $2->next = $3;
      }
    | ID
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    | INT
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    | FLOAT
      {
          $$ = createNode("Exp");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    ;

Args
    : Exp COMMA Args
      {
          $$ = createNode("Args");
          $$->lineno = $1->lineno;
          $$->child = $1;
          $1->next = $2;
          $2->next = $3;
      }
    | Exp
      {
          $$ = createNode("Args");
          $$->lineno = $1->lineno;
          $$->child = $1;
      }
    ;

%%

int yyerror(const char* msg) {
    int line = yylineno;
    if (line == lastErrorLine) return 0;
    syntaxError += 1;
    lastErrorLine = line;
    printf("Error type B at Line %d: %s\n", line, msg);
    return 0;
}