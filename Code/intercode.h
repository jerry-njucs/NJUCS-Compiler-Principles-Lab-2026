#ifndef INTERCODE_H
#define INTERCODE_H

#include <stdio.h>

/* 操作数类型：
 * OP_ADDRESS 表示“地址值”（用于 x := &y、x := *y、*x := y、ARG 传引用等）
 * 注意：OP_ADDRESS 不表示“解引用”，解引用由 READ_MEM/WRITE_MEM 指令表达
 */
typedef enum {
    OP_VARIABLE,   /* 普通变量名 */
    OP_TEMP,       /* 临时变量 t1, t2 ... */
    OP_LABEL,      /* 标号 label1, label2 ... */
    OP_CONSTANT,   /* 立即数 #k */
    OP_ADDRESS,    /* 地址值 &x 或 &t */
    OP_FUNCTION    /* 函数名 f */
} OperandKind;

typedef struct Operand_ {
    OperandKind kind;
    union {
        int val;        /* 常量值 */
        int var_id;     /* 临时变量编号 */
        int label_id;   /* 标号编号 */
        char* name;     /* 变量名/函数名 */
    } u;
} Operand;

/* 中间代码类型（严格对应表 4.6） */
typedef enum {
    LABEL,       /* LABEL x : */
    FUNCTION,    /* FUNCTION f : */
    ASSIGN,      /* x := y */
    PLUS,        /* x := y + z */
    MINUS,       /* x := y - z */
    STAR,        /* x := y * z */
    DIV,         /* x := y / z */
    GET_ADDR,    /* x := &y */
    READ_MEM,    /* x := *y */
    WRITE_MEM,   /* *x := y */
    GOTO,        /* GOTO x */
    IF_GOTO,     /* IF x relop y GOTO z */
    RETURN,      /* RETURN x */
    DEC,         /* DEC x [size] */
    ARG,         /* ARG x */
    CALL,        /* x := CALL f */
    PARAM,       /* PARAM x */
    READ,        /* READ x */
    WRITE        /* WRITE x */
} InterCodeKind;

typedef struct InterCode_ {
    InterCodeKind kind;
    union {
        struct { Operand op; } one;                 /* LABEL/RETURN/ARG/PARAM/READ/WRITE/GOTO */
        struct { Operand left, right; } assign;     /* ASSIGN / GET_ADDR / READ_MEM / WRITE_MEM */
        struct { Operand result, op1, op2; } binop; /* PLUS/MINUS/STAR/DIV */
        struct { Operand x, y; char relop[8]; Operand z; } if_goto; /* IF x relop y GOTO z */
        struct { Operand x; int size; } dec;        /* DEC x [size] */
        struct { Operand ret, func; } call;         /* x := CALL f */
    } u;
} InterCode;

/* 单链表 */
typedef struct CodeList_ {
    InterCode* code;
    struct CodeList_* next;
} CodeList;

/* 构造与管理接口 */
Operand new_temp(void);
Operand new_label(void);
Operand new_constant(int val);
Operand new_variable(const char* name);
Operand new_function(const char* name);
Operand new_address(Operand base);

InterCode* new_intercode(InterCodeKind kind);
CodeList* new_codelist(InterCode* code);
CodeList* join_codelist(CodeList* a, CodeList* b);

void print_operand(FILE* out, Operand op);
void print_intercode(FILE* out, InterCode* code);
void print_codelist(FILE* out, CodeList* list);

#endif