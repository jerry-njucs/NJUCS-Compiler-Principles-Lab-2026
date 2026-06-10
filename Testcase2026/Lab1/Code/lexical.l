%{
    #include "Node.h"
    #include "syntax.tab.h"
    #include <string.h>
    #include <stdlib.h>

    int yycolumn = 1;
    #define YY_USER_ACTION \
        yylloc.first_line = yylloc.last_line = yylineno; \
        yylloc.first_column = yycolumn; \
        yylloc.last_column = yycolumn + yyleng - 1; \
        yycolumn += yyleng;
%}

%option yylineno

STRUCT struct 
RETURN return 
IF if 
ELSE else 
WHILE while 

SEMI ;
COMMA ,
ASSIGNOP =
RELOP >|<|>=|<=|==|!=

PLUS "+"
MINUS "-"
STAR "*"
DIV "/"
AND "&&"
OR "||"
DOT "."
NOT !

LP "("
RP ")"
LB "["
RB "]"
LC "{"
RC "}"

digit [0-9]
INT 0|[1-9]{digit}*
FLOAT ({INT})\.{digit}+
ID [_a-zA-Z][_0-9a-zA-Z]*

TYPE int|float 
%%

\n {yycolumn = 1;}

"//" {
    int c;
    while ((c = input()) != 0 && c != EOF) {
        if (c == '\n') {  
            yycolumn = 1;
            break;
        } else {
            yycolumn++;
        }
    }
}

"/*" {
    int c, prev = 0;
    while ((c = input()) != 0 && c != EOF) {
        if (c == '\n') {   
            yycolumn = 1;
        } else {
            yycolumn++;
        }

        if (prev == '*' && c == '/') {
            break;
        }
        prev = c;
    }

    if ((c == 0 || c == EOF) && yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: Missing */.\n", yylineno);
    }
}

{STRUCT}    {
    //printf("STRUCT at Line %d: %s\n", yylineno, yytext); 
    struct Node* node = createNode("STRUCT");
    node->lineno = yylineno;
    yylval.node = node;
    return STRUCT;
}
{RETURN}    {
    //printf("RETURN at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("RETURN");
    node->lineno = yylineno;
    yylval.node = node;
    return RETURN;
}
{IF}        {
    //printf("IF at Line %d: %s\n", yylineno, yytext); 
    struct Node* node = createNode("IF");
    node->lineno = yylineno;
    yylval.node = node;
    return IF;
}
{ELSE}      {
    //printf("ELSE at Line %d: %s\n", yylineno, yytext); 
    struct Node* node = createNode("ELSE");
    node->lineno = yylineno;
    yylval.node = node;
    return ELSE;
}
{WHILE}     {
    //printf("WHILE at Line %d: %s\n", yylineno, yytext); 
    struct Node* node = createNode("WHILE");
    node->lineno = yylineno;
    yylval.node = node;
    return WHILE;
}
{TYPE}      {
    //printf("TYPE at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("TYPE");
    if (!strcmp(yytext, "int")) {
        node->datatype = DATA_INT;
    }
    else if (!strcmp(yytext, "float")) {
        node->datatype = DATA_FLOAT;
    }
    node->lineno = yylineno;
    yylval.node = node;
    return TYPE;
}

{SEMI}      {
    //printf("SEMI at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("SEMI");
    node->lineno = yylineno;
    yylval.node = node;
    return SEMI;
}
{COMMA}     {
    //printf("COMMA at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("COMMA");
    node->lineno = yylineno;
    yylval.node = node;
    return COMMA;
}

{ASSIGNOP}  {
    //printf("ASSIGNOP at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("ASSIGNOP");
    node->lineno = yylineno;
    yylval.node = node;
    return ASSIGNOP;
}
{RELOP}     {
    //printf("RELOP at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("RELOP");
    node->lineno = yylineno;
    if (!strcmp(yytext, ">")) {
        node->reloptype = RELOP_GT;
    }
    else if (!strcmp(yytext, "<")) {
        node->reloptype = RELOP_LT;
    }
    else if (!strcmp(yytext, ">=")) {
        node->reloptype = RELOP_GE;
    }
    else if (!strcmp(yytext, "<=")) {
        node->reloptype = RELOP_LE;
    }
    else if (!strcmp(yytext, "==")) {
        node->reloptype = RELOP_EQ;
    }
    else if (!strcmp(yytext, "!=")) {
        node->reloptype = RELOP_NE;
    }
    yylval.node = node;
    return RELOP;
}

{PLUS}      {
    //printf("PLUS at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("PLUS");
    node->lineno = yylineno;
    yylval.node = node;
    return PLUS;
}
{MINUS}     {
    //printf("MINUS at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("MINUS");
    node->lineno = yylineno;
    yylval.node = node;
    return MINUS;
}
{STAR}      {
    //printf("STAR at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("STAR");
    node->lineno = yylineno;
    yylval.node = node;
    return STAR;
}
{DIV}       {
    //printf("DIV at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("DIV");
    node->lineno = yylineno;
    yylval.node = node;
    return DIV;
}
{AND}       {
    //printf("AND at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("AND");
    node->lineno = yylineno;
    yylval.node = node;
    return AND;
}
{OR}        {
    //printf("OR at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("OR");
    node->lineno = yylineno;
    yylval.node = node;
    return OR;
}
{DOT}       {
    //printf("DOT at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("DOT");
    node->lineno = yylineno;
    yylval.node = node;
    return DOT;
}
{NOT}       {
    //printf("NOT at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("NOT");
    node->lineno = yylineno;
    yylval.node = node;
    return NOT;
}

{LP}        {
    //printf("LP at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("LP");
    node->lineno = yylineno;
    yylval.node = node;
    return LP;
}
{RP}        {
    //printf("RP at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("RP");
    node->lineno = yylineno;
    yylval.node = node;
    return RP;
}
{LB}        {
    //printf("LB at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("LB");
    node->lineno = yylineno;
    yylval.node = node;
    return LB;
}
{RB}        {
    //printf("RB at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("RB");
    node->lineno = yylineno;
    yylval.node = node;
    return RB;
}
{LC}        {
    //printf("LC at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("LC");
    node->lineno = yylineno;
    yylval.node = node;
    return LC;
}
{RC}        {
    //printf("RC at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("RC");
    node->lineno = yylineno;
    yylval.node = node;
    return RC;
}

\.[0-9]+  {
    if (yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: illegal float literal \"%s\"\n", yylineno, yytext);
    }
}

0[xX][0-9a-zA-Z]+  {
    if (yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: Illegal hexadecimal number \'%s\'\n", yylineno, yytext);
    }
}

0[0-9]+  {
    if (yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: Illegal octal number \'%s\'\n", yylineno, yytext);
    }
}

([0-9]*\.[0-9]+|[0-9]+\.[0-9]*|[0-9]+)[eE][+-]?[0-9a-zA-Z]*  {
    if (yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: Illegal scientific float \"%s\"\n", yylineno, yytext);
    }
}

{FLOAT}     {
    //printf("FLOAT at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("FLOAT");
    node->lineno = yylineno;
    node->floatvalue = atof(yytext);
    yylval.node = node;
    return FLOAT;
}
{INT}       {
    //printf("INT at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("INT");
    node->lineno = yylineno;
    node->intvalue = atoi(yytext);
    yylval.node = node;
    return INT;
}

{ID}        {
    //printf("ID at Line %d: %s\n", yylineno, yytext);
    struct Node* node = createNode("ID");
    node->lineno = yylineno;
    strcpy(node->idname, yytext);
    yylval.node = node;
    return ID;
}

[ \t\r]+  { /* 跳过空白字符 */ }

. {
    if (yylineno != lastErrorLine) {
        lexicalError += 1;
        lastErrorLine = yylineno;
        printf("Error type A at Line %d: Undefined characters \'%s\'\n", yylineno, yytext);
    }
}



%%

