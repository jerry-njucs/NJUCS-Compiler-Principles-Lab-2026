#include "intercode.h"
#include <stdlib.h>
#include <string.h>

static int temp_cnt = 0;
static int label_cnt = 0;

static char* ic_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Operand 构造 */
Operand new_temp(void) {
    Operand op;
    op.kind = OP_TEMP;
    op.u.var_id = ++temp_cnt;
    return op;
}

Operand new_label(void) {
    Operand op;
    op.kind = OP_LABEL;
    op.u.label_id = ++label_cnt;
    return op;
}

Operand new_constant(int val) {
    Operand op;
    op.kind = OP_CONSTANT;
    op.u.val = val;
    return op;
}

Operand new_variable(const char* name) {
    Operand op;
    op.kind = OP_VARIABLE;
    op.u.name = ic_strdup(name);
    return op;
}

Operand new_function(const char* name) {
    Operand op;
    op.kind = OP_FUNCTION;
    op.u.name = ic_strdup(name);
    return op;
}

/* OP_ADDRESS 表示“地址值”，输出时应加 & 前缀 */
Operand new_address(Operand base) {
    Operand op = base;
    op.kind = OP_ADDRESS;
    return op;
}

/* InterCode 构造 */
InterCode* new_intercode(InterCodeKind kind) {
    InterCode* c = (InterCode*)calloc(1, sizeof(InterCode));
    if (!c) return NULL;
    c->kind = kind;
    return c;
}

/* CodeList */
CodeList* new_codelist(InterCode* code) {
    CodeList* n = (CodeList*)calloc(1, sizeof(CodeList));
    if (!n) return NULL;
    n->code = code;
    n->next = NULL;
    return n;
}

CodeList* join_codelist(CodeList* a, CodeList* b) {
    if (!a) return b;
    if (!b) return a;
    CodeList* p = a;
    while (p->next) p = p->next;
    p->next = b;
    return a;
}

/* 输出工具（后续会补全格式） */
void print_operand(FILE* out, Operand op) {
    switch (op.kind) {
        case OP_VARIABLE:  fprintf(out, "%s", op.u.name); break;
        case OP_FUNCTION:  fprintf(out, "%s", op.u.name); break;
        case OP_TEMP:      fprintf(out, "t%d", op.u.var_id); break;
        case OP_LABEL:     fprintf(out, "label%d", op.u.label_id); break;
        case OP_CONSTANT:  fprintf(out, "#%d", op.u.val); break;
        case OP_ADDRESS:
            /* 地址值输出为 &x 或 &t */
            fprintf(out, "&");
            /* 复用已有字段打印主体 */
            if (op.u.name) fprintf(out, "%s", op.u.name);
            else fprintf(out, "t%d", op.u.var_id);
            break;
        default:           fprintf(out, "<?>"); break;
    }
}

void print_intercode(FILE* out, InterCode* code) {  // intercode printer
    if (!code) return;

    switch (code->kind) {
        case LABEL:
            fprintf(out, "LABEL ");
            print_operand(out, code->u.one.op);
            fprintf(out, " :\n");
            break;

        case FUNCTION:
            fprintf(out, "FUNCTION ");
            print_operand(out, code->u.one.op);
            fprintf(out, " :\n");
            break;

        case ASSIGN:
            print_operand(out, code->u.assign.left);
            fprintf(out, " := ");
            print_operand(out, code->u.assign.right);
            fprintf(out, "\n");
            break;

        case PLUS:
        case MINUS:
        case STAR:
        case DIV: {
            print_operand(out, code->u.binop.result);
            fprintf(out, " := ");
            print_operand(out, code->u.binop.op1);
            fprintf(out, " %c ",
                    code->kind == PLUS ? '+' :
                    code->kind == MINUS ? '-' :
                    code->kind == STAR ? '*' : '/');
            print_operand(out, code->u.binop.op2);
            fprintf(out, "\n");
            break;
        }

        case GET_ADDR:
            print_operand(out, code->u.assign.left);
            fprintf(out, " := &");
            print_operand(out, code->u.assign.right);
            fprintf(out, "\n");
            break;

        case READ_MEM:
            print_operand(out, code->u.assign.left);
            fprintf(out, " := *");
            print_operand(out, code->u.assign.right);
            fprintf(out, "\n");
            break;

        case WRITE_MEM:
            fprintf(out, "*");
            print_operand(out, code->u.assign.left);
            fprintf(out, " := ");
            print_operand(out, code->u.assign.right);
            fprintf(out, "\n");
            break;

        case GOTO:
            fprintf(out, "GOTO ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        case IF_GOTO:
            fprintf(out, "IF ");
            print_operand(out, code->u.if_goto.x);
            fprintf(out, " %s ", code->u.if_goto.relop);
            print_operand(out, code->u.if_goto.y);
            fprintf(out, " GOTO ");
            print_operand(out, code->u.if_goto.z);
            fprintf(out, "\n");
            break;

        case RETURN:
            fprintf(out, "RETURN ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        case DEC:
            fprintf(out, "DEC ");
            print_operand(out, code->u.dec.x);
            fprintf(out, " %d\n", code->u.dec.size);
            break;

        case ARG:
            fprintf(out, "ARG ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        case CALL:
            print_operand(out, code->u.call.ret);
            fprintf(out, " := CALL ");
            print_operand(out, code->u.call.func);
            fprintf(out, "\n");
            break;

        case PARAM:
            fprintf(out, "PARAM ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        case READ:
            fprintf(out, "READ ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        case WRITE:
            fprintf(out, "WRITE ");
            print_operand(out, code->u.one.op);
            fprintf(out, "\n");
            break;

        default:
            break;
    }
}

void print_codelist(FILE* out, CodeList* list) {
    while (list) {
        if (list->code) print_intercode(out, list->code);
        list = list->next;
    }
}