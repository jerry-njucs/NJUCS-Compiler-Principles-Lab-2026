#include "intercode.h"
#include "Node.h"
#include "Symbol_table.h"
#include <stdlib.h>
#include <string.h>

static int temp_cnt = 0;
static int label_cnt = 0;

typedef struct ArgList_ {
    Operand op;
    struct ArgList_* next;
} ArgList;

// 前向声明
static CodeList* translate_CompSt(Node* node);
static CodeList* translate_Args(Node* node, ArgList** arg_list);
static CodeList* translate_Cond(Node* node, Operand label_true, Operand label_false);
static CodeList* translate_Exp_Addr(Node* exp, Operand* addr_place);

// helper functions
static char* ic_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_node(Node* n, const char* t) {
    return n && t && strcmp(n->type, t) == 0;
}

static const char* relop_to_str(Node* relop_node) {
    if (!relop_node) return "!=";
    switch (relop_node->reloptype) {
        case RELOP_GT: return ">";
        case RELOP_LT: return "<";
        case RELOP_GE: return ">=";
        case RELOP_LE: return "<=";
        case RELOP_EQ: return "==";
        case RELOP_NE: return "!=";
        default:       return "!=";
    }
}

static ArgList* arglist_push_front(ArgList* head, Operand op) {
    ArgList* n = (ArgList*)malloc(sizeof(ArgList));
    if (!n) return head;
    n->op = op;
    n->next = head;
    return n;
}

static int get_int_value(Node* n) {
    return n->intvalue;
}

static const char* get_vardec_id(Node* varDec) {
    if (!varDec) return NULL;
    Node* cur = varDec;
    while (cur && !is_node(cur, "ID")) {
        cur = cur->child;
    }
    return cur ? cur->idname : NULL;
}

/* 若 Exp 是 ID 或 INT，则直接生成 Operand，且不产生任何代码 */
static int exp_to_operand_if_simple(Node* exp, Operand* out) {
    if (!exp || !is_node(exp, "Exp")) return 0;
    Node* first = exp->child;
    Node* second = first ? first->next : NULL;

    /* Exp -> ID */
    if (first && is_node(first, "ID") && !second) {
        *out = new_variable(first->idname);
        return 1;
    }
    /* Exp -> INT */
    if (first && is_node(first, "INT")) {
        *out = new_constant(get_int_value(first));
        return 1;
    }
    return 0;
}

static Node* find_child(Node* n, const char* type) {
    for (Node* p = n ? n->child : NULL; p; p = p->next) {
        if (is_node(p, type)) return p;
    }
    return NULL;
}

static int get_type_size(Type type) {
    if (!type) return 4;
    if (type->kind == BASIC) return 4;
    if (type->kind == ARRAY) return type->content.array.size * get_type_size(type->content.array.elem);
    if (type->kind == STRUCTURE) {
        int size = 0;
        FieldList f = type->content.structure;
        while (f) {
            size += get_type_size(f->type);
            f = f->tail;
        }
        return size;
    }
    return 4;
}

// 通过查询符号表获取 VarDec 对应的变量名和类型大小
static int get_vardec_size_and_id(Node* varDec, const char** id_out) {
    *id_out = get_vardec_id(varDec); 
    if (*id_out) {
        Symbol s = lookup(*id_out);
        if (s && s->kind == SYM_VAR) {
            // 拒绝多维数组 
            if (s->u.var_type && s->u.var_type->kind == ARRAY && 
                s->u.var_type->content.array.elem->kind == ARRAY) {
                printf("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.\n");
                exit(0);
            }
            return get_type_size(s->u.var_type);
        }
    }
    return 4;
}

static Type get_exp_type(Node* exp) {
    if (!exp || !is_node(exp, "Exp")) return NULL;
    Node* first = exp->child;
    Node* second = first ? first->next : NULL;

    if (first && is_node(first, "ID") && !second) {
        Symbol s = lookup(first->idname);
        return (s && s->kind == SYM_VAR) ? s->u.var_type : NULL;
    }
    if (first && is_node(first, "Exp") && second && is_node(second, "DOT")) {
        Type base = get_exp_type(first);
        if (base && base->kind == STRUCTURE) {
            Node* id_node = second->next;
            FieldList f = base->content.structure;
            while (f) {
                if (strcmp(f->name, id_node->idname) == 0) return f->type;
                f = f->tail;
            }
        }
    }
    if (first && is_node(first, "Exp") && second && is_node(second, "LB")) {
        Type base = get_exp_type(first);
        if (base && base->kind == ARRAY) return base->content.array.elem;
    }
    return NULL;
}

static int get_field_offset(Node* exp, const char* field_name) {
    Type base_type = get_exp_type(exp);
    if (!base_type || base_type->kind != STRUCTURE) return 0;
    int offset = 0;
    FieldList f = base_type->content.structure;
    while (f) {
        if (strcmp(f->name, field_name) == 0) break;
        offset += get_type_size(f->type);
        f = f->tail;
    }
    return offset;
}

static int check_is_struct_or_array(Node* exp) {
    Type t = get_exp_type(exp);
    if (t && (t->kind == ARRAY || t->kind == STRUCTURE)) return 1;
    return 0;
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

// intercode output logic
void print_operand(FILE* out, Operand op) {
    switch (op.kind) {
        case OP_VARIABLE:  fprintf(out, "%s", op.u.name); break;
        case OP_FUNCTION:  fprintf(out, "%s", op.u.name); break;
        case OP_TEMP:      fprintf(out, "t%d", op.u.var_id); break;
        case OP_LABEL:     fprintf(out, "label%d", op.u.label_id); break;
        case OP_CONSTANT:  fprintf(out, "#%d", op.u.val); break;
        case OP_ADDRESS:
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

// four important components
static CodeList* translate_Exp(Node* node, Operand* place) {
    if (!node || !is_node(node, "Exp"))
        return NULL;

    Node* first = node->child;
    Node* second = first ? first->next : NULL;
    Node* third = second ? second->next : NULL;
    Node* fourth = third ? third->next : NULL;

    /* Exp -> LP Exp RP */
    if (first && is_node(first, "LP") && second && is_node(second, "Exp") && third && is_node(third, "RP")) {
        return translate_Exp(second, place);
    }

    if (first && is_node(first, "INT")) {  // INT
        if (!place) {
            Operand tmp = new_temp();
            place = &tmp;
        }
        InterCode* asn = new_intercode(ASSIGN);
        asn->u.assign.left = *place;
        asn->u.assign.right = new_constant(get_int_value(first));
        return new_codelist(asn);
    }
    if (first && is_node(first, "ID") && !second) {
        if (!place) {
            Operand tmp = new_temp();
            place = &tmp;
        }
        InterCode* asn = new_intercode(ASSIGN);
        asn->u.assign.left = *place;
        asn->u.assign.right = new_variable(first->idname);
        return new_codelist(asn);
    }
    if (first && is_node(first, "Exp") && second && is_node(second, "ASSIGNOP") && third && is_node(third, "Exp")) {
        Node* left = first->child;
        Node* left_sec = left ? left->next : NULL;

        /* ID 赋值 */
        if (left && is_node(left, "ID") && !left_sec) {
            Operand t1 = new_temp();
            CodeList* code1 = translate_Exp(third, &t1);

            InterCode* asn1 = new_intercode(ASSIGN);
            asn1->u.assign.left = new_variable(left->idname);
            asn1->u.assign.right = t1;

            CodeList* code2 = new_codelist(asn1);

            if (place) {
                InterCode* asn2 = new_intercode(ASSIGN);
                asn2->u.assign.left = *place;
                asn2->u.assign.right = new_variable(left->idname);
                CodeList* code3 = new_codelist(asn2);
                return join_codelist(code1, join_codelist(code2, code3));
            }
            return join_codelist(code1, code2);
        }
        /* 数组元素赋值 Exp[Exp] = Exp */
        else if (left && is_node(left, "Exp") && left_sec && (is_node(left_sec, "LB") || is_node(left_sec, "DOT"))) {
            Operand addr;
            CodeList* code1 = translate_Exp_Addr(first, &addr); /* 计算左值地址 */

            Operand t1 = new_temp();
            CodeList* code2 = translate_Exp(third, &t1);        /* 计算右值 */

            InterCode* wm = new_intercode(WRITE_MEM);
            wm->u.assign.left = addr;  // *addr
            wm->u.assign.right = t1;   // := t1

            CodeList* res = join_codelist(code1, code2);
            res = join_codelist(res, new_codelist(wm));

            if (place) {
                InterCode* asn = new_intercode(ASSIGN);
                asn->u.assign.left = *place;
                asn->u.assign.right = t1;
                res = join_codelist(res, new_codelist(asn));
            }
            return res;
        }
    }
    if (first && is_node(first, "Exp") && second && (is_node(second, "LB") || is_node(second, "DOT"))) {
        Operand addr;
        CodeList* code1 = translate_Exp_Addr(node, &addr);
        if (place) {
            InterCode* rm = new_intercode(READ_MEM);
            rm->u.assign.left = *place;
            rm->u.assign.right = addr; /* place := *addr */
            return join_codelist(code1, new_codelist(rm));
        }
        return code1;
    }
    if (first && is_node(first, "Exp") && second && third && is_node(third, "Exp")) {
        InterCodeKind k;
        if (is_node(second, "PLUS")) k = PLUS;
        else if (is_node(second, "MINUS")) k = MINUS;
        else if (is_node(second, "STAR")) k = STAR;
        else if (is_node(second, "DIV")) k = DIV;
        else k = -1;

        if (k != -1) {
            Operand t1 = new_temp();
            Operand t2 = new_temp();
            CodeList* code1 = translate_Exp(first, &t1);
            CodeList* code2 = translate_Exp(third, &t2);

            if (!place) {
                Operand tmp = new_temp();
                place = &tmp;
            }
            InterCode* bin = new_intercode(k);
            bin->u.binop.result = *place;
            bin->u.binop.op1 = t1;
            bin->u.binop.op2 = t2;

            return join_codelist(join_codelist(code1, code2), new_codelist(bin));
        }
    }
    if (first && is_node(first, "MINUS") && second && is_node(second, "Exp")) {
        Operand t1 = new_temp();
        CodeList* code1 = translate_Exp(second, &t1);

        if (!place) {
            Operand tmp = new_temp();
            place = &tmp;
        }
        InterCode* bin = new_intercode(MINUS);
        bin->u.binop.result = *place;
        bin->u.binop.op1 = new_constant(0);
        bin->u.binop.op2 = t1;

        return join_codelist(code1, new_codelist(bin));
    }
    if ((second && is_node(second, "RELOP")) || (first && is_node(first, "NOT")) ||
        (second && (is_node(second, "AND") || is_node(second, "OR")))) {

        if (!place) {
            Operand tmp = new_temp();
            place = &tmp;
        }

        Operand label1 = new_label();
        Operand label2 = new_label();

        InterCode* asn0 = new_intercode(ASSIGN);
        asn0->u.assign.left = *place;
        asn0->u.assign.right = new_constant(0);

        CodeList* code0 = new_codelist(asn0);
        CodeList* code1 = translate_Cond(node, label1, label2);

        InterCode* l1 = new_intercode(LABEL);
        l1->u.one.op = label1;

        InterCode* asn1 = new_intercode(ASSIGN);
        asn1->u.assign.left = *place;
        asn1->u.assign.right = new_constant(1);

        InterCode* l2 = new_intercode(LABEL);
        l2->u.one.op = label2;

        CodeList* code2 = new_codelist(l1);
        CodeList* code3 = new_codelist(asn1);
        CodeList* code4 = new_codelist(l2);

        CodeList* whole = join_codelist(code0, code1);
        whole = join_codelist(whole, code2);
        whole = join_codelist(whole, code3);
        whole = join_codelist(whole, code4);
        return whole;
    }
    if (first && is_node(first, "ID") && second && is_node(second, "LP") && 
        third && is_node(third, "RP")) {  // ID LP RP
        const char* funcname = first->idname;
        Operand tmp;
        Operand* dest = place;
        if (!dest) { tmp = new_temp(); dest = &tmp; }

        if (strcmp(funcname, "read") == 0) {
            InterCode* rd = new_intercode(READ);
            rd->u.one.op = *dest;
            return new_codelist(rd);
        } 
        else {
            InterCode* call = new_intercode(CALL);
            call->u.call.ret = *dest;
            call->u.call.func = new_function(funcname);
            return new_codelist(call);
        }
    }

    if (first && is_node(first, "ID") && second && is_node(second, "LP") &&
        third && is_node(third, "Args") && fourth && is_node(fourth, "RP")) {
        const char* funcname = first->idname;
        ArgList* arg_list = NULL;
        CodeList* code1 = translate_Args(third, &arg_list);

        Operand tmp;
        Operand* dest = place;
        if (!dest) { tmp = new_temp(); dest = &tmp; }

        if (strcmp(funcname, "write") == 0) {
            if (arg_list) {
                InterCode* wr = new_intercode(WRITE);
                wr->u.one.op = arg_list->op;
                CodeList* code2 = new_codelist(wr);

                if (place) { /* 只有 place 非空才写回 #0 */
                    InterCode* asn = new_intercode(ASSIGN);
                    asn->u.assign.left = *place;
                    asn->u.assign.right = new_constant(0);
                    CodeList* code3 = new_codelist(asn);
                    return join_codelist(code1, join_codelist(code2, code3));
                }
                return join_codelist(code1, code2);
            }
            return code1;
        }

        // normal function
        CodeList* code2 = NULL;
        for (ArgList* p = arg_list; p; p = p->next) {
            InterCode* arg = new_intercode(ARG);
            arg->u.one.op = p->op;
            code2 = join_codelist(code2, new_codelist(arg));
        }

        InterCode* call = new_intercode(CALL);
        call->u.call.ret = *dest;
        call->u.call.func = new_function(funcname);

        CodeList* code3 = new_codelist(call);
        return join_codelist(code1, join_codelist(code2, code3));
    }
    return NULL;
}

static CodeList* translate_Cond(Node* node, Operand label_true, Operand label_false) {
    if (!node || !is_node(node, "Exp")) return NULL;

    Node* first = node->child;
    Node* second = first ? first->next : NULL;
    Node* third = second ? second->next : NULL;

    /* Exp1 RELOP Exp2 */
    if (first && is_node(first, "Exp") && second && is_node(second, "RELOP") && third && is_node(third, "Exp")) {
        Operand op1, op2;
        CodeList* code1 = NULL;
        CodeList* code2 = NULL;

        if (!exp_to_operand_if_simple(first, &op1)) {
            Operand t1 = new_temp();
            code1 = translate_Exp(first, &t1);
            op1 = t1;
        }
        if (!exp_to_operand_if_simple(third, &op2)) {
            Operand t2 = new_temp();
            code2 = translate_Exp(third, &t2);
            op2 = t2;
        }

        InterCode* if_code = new_intercode(IF_GOTO);
        if_code->u.if_goto.x = op1;
        if_code->u.if_goto.y = op2;
        snprintf(if_code->u.if_goto.relop, sizeof(if_code->u.if_goto.relop), "%s", relop_to_str(second));
        if_code->u.if_goto.z = label_true;

        InterCode* go = new_intercode(GOTO);
        go->u.one.op = label_false;

        CodeList* whole = join_codelist(code1, code2);
        whole = join_codelist(whole, new_codelist(if_code));
        whole = join_codelist(whole, new_codelist(go));
        return whole;
    }

    /* NOT Exp */
    if (is_node(first, "NOT")) {
        return translate_Cond(second, label_false, label_true);
    }
    /* Exp AND Exp */
    if (second && is_node(second, "AND")) {
        Operand label1 = new_label();
        CodeList* code1 = translate_Cond(first, label1, label_false);
        CodeList* code2 = translate_Cond(third, label_true, label_false);

        InterCode* mid_code = new_intercode(LABEL);
        mid_code->u.one.op = label1;

        CodeList* whole_code = join_codelist(code1, new_codelist(mid_code));
        whole_code = join_codelist(whole_code, code2);
        return whole_code;
    }
    /* Exp OR Exp */
    if (second && is_node(second, "OR")) {
        Operand label1 = new_label();
        CodeList* code1 = translate_Cond(first, label_true, label1);
        CodeList* code2 = translate_Cond(third, label_true, label_false);

        InterCode* mid_code = new_intercode(LABEL);
        mid_code->u.one.op = label1;

        CodeList* whole_code = join_codelist(code1, new_codelist(mid_code));
        whole_code = join_codelist(whole_code, code2);
        return whole_code;
    }

    // other cases
    Operand t1 = new_temp();
    CodeList* code1 = translate_Exp(node, &t1);
    InterCode* if_code = new_intercode(IF_GOTO);
    if_code->u.if_goto.x = t1;
    if_code->u.if_goto.y = new_constant(0);
    snprintf(if_code->u.if_goto.relop, sizeof(if_code->u.if_goto.relop), "!=");
    if_code->u.if_goto.z = label_true;

    InterCode* goto_false = new_intercode(GOTO);
    goto_false->u.one.op = label_false;

    CodeList* whole_code = join_codelist(code1, new_codelist(if_code));
    whole_code = join_codelist(whole_code, new_codelist(goto_false));
    return whole_code;
}

static CodeList* translate_Stmt(Node* node) {
    if (!node || !is_node(node, "Stmt"))
        return NULL;

    Node* first = node->child;

    if (first && is_node(first, "Exp")) {  // Exp SEMI
        return translate_Exp(first, NULL);
    }
    if (first && is_node(first, "CompSt")) {  // CompSt
        return translate_CompSt(first);
    }
    if (first && is_node(first, "RETURN")) {  // RETURN Exp SEMI
        Node* exp = first->next;
        Operand t1 = new_temp();
        CodeList* code1 = translate_Exp(exp, &t1);
        InterCode* ret_code = new_intercode(RETURN);
        ret_code->u.one.op = t1;
        CodeList* code2 = new_codelist(ret_code);
        return join_codelist(code1, code2);
    }
    if (first && is_node(first, "IF")) {
        Node* exp = first->next->next;
        Node* stmt1 = exp->next->next;

        if (stmt1->next && is_node(stmt1->next, "ELSE")) {  // IF Exp Stmt ELSE Stmt
            Node* stmt2 = stmt1->next->next;
            Operand label1 = new_label();
            Operand label2 = new_label();
            Operand label3 = new_label();

            CodeList* code1 = translate_Cond(exp, label1, label2);
            CodeList* code2 = translate_Stmt(stmt1);
            CodeList* code3 = translate_Stmt(stmt2);

            InterCode* l1 = new_intercode(LABEL);
            l1->u.one.op = label1;
            InterCode* l2 = new_intercode(LABEL);
            l2->u.one.op = label2;
            InterCode* l3 = new_intercode(LABEL);
            l3->u.one.op = label3;

            InterCode* go = new_intercode(GOTO);
            go->u.one.op = label3;

            CodeList* whole = join_codelist(code1, new_codelist(l1));
            whole = join_codelist(whole, code2);
            whole = join_codelist(whole, new_codelist(go));
            whole = join_codelist(whole, new_codelist(l2));
            whole = join_codelist(whole, code3);
            whole = join_codelist(whole, new_codelist(l3));
            return whole;
        } else {  // IF Exp Stmt
            Operand label1 = new_label();
            Operand label2 = new_label();
            CodeList* code1 = translate_Cond(exp, label1, label2);
            CodeList* code2 = translate_Stmt(stmt1);

            InterCode* l1 = new_intercode(LABEL);
            l1->u.one.op = label1;
            InterCode* l2 = new_intercode(LABEL);
            l2->u.one.op = label2;

            CodeList* whole_code = join_codelist(code1, new_codelist(l1));
            whole_code = join_codelist(whole_code, code2);
            whole_code = join_codelist(whole_code, new_codelist(l2));
            return whole_code;
        }
    }
    if (first && is_node(first, "WHILE")) { // WHILE Exp Stmt
        Node* exp = first->next->next;
        Node* stmt = exp->next->next;

        Operand label1 = new_label();
        Operand label2 = new_label();
        Operand label3 = new_label();

        CodeList* code1 = translate_Cond(exp, label2, label3);
        CodeList* code2 = translate_Stmt(stmt);

        InterCode* l1 = new_intercode(LABEL);
        l1->u.one.op = label1;
        InterCode* l2 = new_intercode(LABEL);
        l2->u.one.op = label2;
        InterCode* l3 = new_intercode(LABEL);
        l3->u.one.op = label3;

        InterCode* goto_l1 = new_intercode(GOTO);
        goto_l1->u.one.op = label1;

        CodeList* whole_code = join_codelist(new_codelist(l1), code1);
        whole_code = join_codelist(whole_code, new_codelist(l2));
        whole_code = join_codelist(whole_code, code2);
        whole_code = join_codelist(whole_code, new_codelist(goto_l1));
        whole_code = join_codelist(whole_code, new_codelist(l3));
        return whole_code;
    }

    return NULL;
}

static CodeList* translate_Args(Node* node, ArgList** arg_list) {
    if (!node || !is_node(node, "Args"))
        return NULL;

    Node* exp = node->child;
    Node* comma = exp->next;
    Node* args = comma ? comma->next : NULL;

    Operand t1 = new_temp();
    CodeList* code1 = NULL;

    // 检查是否为结构体或数组，若属于则传地址
    if (check_is_struct_or_array(exp)) {
        code1 = translate_Exp_Addr(exp, &t1);
    } else {
        code1 = translate_Exp(exp, &t1);
    }
    
    *arg_list = arglist_push_front(*arg_list, t1);

    if (comma && is_node(comma, "COMMA")) {  // 参数不止一个
        CodeList* code2 = translate_Args(args, arg_list);
        return join_codelist(code1, code2);
    }
    return code1;
}

/* Dec -> VarDec | VarDec ASSIGNOP Exp */
static CodeList* translate_Dec(Node* node) {
    if (!node || !is_node(node, "Dec")) return NULL;
    Node* vardec = node->child;
    Node* assignop = vardec ? vardec->next : NULL;
    
    const char* var_name = NULL;
    int size = get_vardec_size_and_id(vardec, &var_name);
    
    CodeList* code_dec = NULL;
    // 如果 size 大于 4，说明是数组，申请空间 
    if (size > 4 && var_name) {
        InterCode* dec = new_intercode(DEC);
        dec->u.dec.x = new_variable(var_name);
        dec->u.dec.size = size;
        code_dec = new_codelist(dec);
    }

    if (assignop && is_node(assignop, "ASSIGNOP")) {
        Node* exp = assignop->next;
        if (var_name) {
            Operand t1 = new_temp();
            CodeList* code1 = translate_Exp(exp, &t1);
            InterCode* asn = new_intercode(ASSIGN);
            asn->u.assign.left = new_variable(var_name);
            asn->u.assign.right = t1;
            return join_codelist(code_dec, join_codelist(code1, new_codelist(asn)));
        }
    }
    return code_dec;
}

static CodeList* translate_Exp_Addr(Node* exp, Operand* addr_place) {
    if (!exp || !is_node(exp, "Exp")) return NULL;
    Node* first = exp->child;
    Node* second = first ? first->next : NULL;

    /* Exp -> ID */
    if (first && is_node(first, "ID") && !second) {
        *addr_place = new_temp();
        
        Symbol s = lookup(first->idname);
        if (s && s->is_param && s->u.var_type && 
           (s->u.var_type->kind == ARRAY || s->u.var_type->kind == STRUCTURE)) {
            InterCode* asn = new_intercode(ASSIGN);
            asn->u.assign.left = *addr_place;
            asn->u.assign.right = new_variable(first->idname);
            return new_codelist(asn);
        } else {
            InterCode* ga = new_intercode(GET_ADDR);
            ga->u.assign.left = *addr_place;
            ga->u.assign.right = new_variable(first->idname);
            return new_codelist(ga);
        }
    }

    /* Exp -> Exp LB Exp RB (一维数组访问) */
    if (first && is_node(first, "Exp") && second && is_node(second, "LB")) {
        Node* index_exp = second->next;
        
        Operand base_addr = new_temp();
        CodeList* code1 = translate_Exp_Addr(first, &base_addr);

        Operand index_val = new_temp();
        CodeList* code2 = translate_Exp(index_exp, &index_val);
        
        Type elem_type = get_exp_type(exp); 
        int elem_size = get_type_size(elem_type); /* 动态获取数组元素的大小 */

        Operand offset = new_temp();
        InterCode* mul = new_intercode(STAR);
        mul->u.binop.result = offset;
        mul->u.binop.op1 = index_val;
        mul->u.binop.op2 = new_constant(elem_size);

        *addr_place = new_temp();
        InterCode* add = new_intercode(PLUS);
        add->u.binop.result = *addr_place;
        add->u.binop.op1 = base_addr;
        add->u.binop.op2 = offset;

        CodeList* res = join_codelist(code1, code2);
        res = join_codelist(res, new_codelist(mul));
        res = join_codelist(res, new_codelist(add));
        return res;
    }
    
    /* Exp -> Exp DOT ID (结构体访问) */
    if (first && is_node(first, "Exp") && second && is_node(second, "DOT")) {
        Node* id_node = second->next;
        Operand base_addr = new_temp();
        CodeList* code1 = translate_Exp_Addr(first, &base_addr);

        int offset = get_field_offset(first, id_node->idname);

        *addr_place = new_temp();
        InterCode* add = new_intercode(PLUS);
        add->u.binop.result = *addr_place;
        add->u.binop.op1 = base_addr;
        add->u.binop.op2 = new_constant(offset);

        return join_codelist(code1, new_codelist(add));
    }
    return NULL;
}

/* DecList -> Dec | Dec COMMA DecList */
static CodeList* translate_DecList(Node* node) {
    if (!node || !is_node(node, "DecList")) return NULL;
    Node* dec = node->child;
    Node* comma = dec ? dec->next : NULL;
    
    CodeList* code1 = translate_Dec(dec);
    if (comma && is_node(comma, "COMMA")) {
        CodeList* code2 = translate_DecList(comma->next);
        return join_codelist(code1, code2);
    }
    return code1;
}

/* Def -> Specifier DecList SEMI */
static CodeList* translate_Def(Node* node) {
    if (!node || !is_node(node, "Def")) return NULL;
    Node* declist = node->child ? node->child->next : NULL;
    
    if (declist && is_node(declist, "DecList")) {
        return translate_DecList(declist);
    }
    return NULL;
}

/* DefList -> Def DefList | empty */
static CodeList* translate_DefList(Node* node) {
    if (!node || !is_node(node, "DefList") || !node->child)
        return NULL;
        
    Node* def = node->child;
    Node* deflist = def->next;
    
    CodeList* code1 = translate_Def(def);
    CodeList* code2 = translate_DefList(deflist);
    
    return join_codelist(code1, code2);
}

static CodeList* translate_StmtList(Node* node) {
    if (!node || !is_node(node, "StmtList") || !node->child)
        return NULL;

    Node* stmt = node->child;      // Stmt
    Node* stmtlist = stmt->next;   // StmtList
    CodeList* code1 = translate_Stmt(stmt);
    CodeList* code2 = translate_StmtList(stmtlist);
    return join_codelist(code1, code2);
}

static CodeList* translate_CompSt(Node* node) {  // 语句块
    if (!node || !is_node(node, "CompSt"))
        return NULL;

    Node* deflist = find_child(node, "DefList");
    Node* stmtlist = find_child(node, "StmtList");

    CodeList* code1 = translate_DefList(deflist);
    CodeList* code2 = translate_StmtList(stmtlist);
    return join_codelist(code1, code2);
}  

/* ParamDec -> Specifier VarDec */
static CodeList* translate_ParamDec(Node* node) {
    if (!node || !is_node(node, "ParamDec")) return NULL;
    Node* varDec = node->child ? node->child->next : NULL;
    const char* name = get_vardec_id(varDec);
    if (!name) return NULL;

    /* 拦截：拒绝数组类型作为参数 */
    Symbol s = lookup(name);
    if (s && s->kind == SYM_VAR && s->u.var_type && s->u.var_type->kind == ARRAY) {
        printf("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.\n");
        exit(0);
    }

    InterCode* p = new_intercode(PARAM);
    p->u.one.op = new_variable(name);
    return new_codelist(p);
}

/* VarList -> ParamDec COMMA VarList | ParamDec */
static CodeList* translate_VarList(Node* node) {
    if (!node || !is_node(node, "VarList")) return NULL;
    Node* param = node->child;
    Node* comma = param ? param->next : NULL;
    Node* rest = comma ? comma->next : NULL;

    CodeList* code1 = translate_ParamDec(param);
    if (comma && is_node(comma, "COMMA")) {
        CodeList* code2 = translate_VarList(rest);
        return join_codelist(code1, code2);
    }
    return code1;
}
static CodeList* translate_FunDec(Node* node) {  // 函数头
    if (!node || !is_node(node, "FunDec")) return NULL;

    Node* id = node->child;              // ID
    Node* lp = id ? id->next : NULL;     // LP
    Node* varlist = lp ? lp->next : NULL; // VarList or RP

    /* FUNCTION f : */
    InterCode* f = new_intercode(FUNCTION);
    f->u.one.op = new_function(id->idname);
    CodeList* code1 = new_codelist(f);

    /* 参数列表 */
    if (varlist && is_node(varlist, "VarList")) {
        CodeList* code2 = translate_VarList(varlist);
        return join_codelist(code1, code2);
    }
    return code1;
}
static CodeList* translate_ExtDef(Node* node) {
    // 在这里忽略全局变量和结构体的定义
    if (!node || !is_node(node, "ExtDef"))
        return NULL;

    Node* specifier = node->child;
    Node* fundec = specifier->next;
    Node* compst = fundec->next;

    if (is_node(fundec, "FunDec") && is_node(compst, "CompSt")) {  // 处理函数定义
        CodeList* code1 = translate_FunDec(fundec);
        CodeList* code2 = translate_CompSt(compst);
        return join_codelist(code1, code2);
    }
    return NULL; 
}

static CodeList* translate_ExtDefList(Node* node) {
    if (!node)
        return NULL;

    Node* extdef = node->child;
    Node* extdeflist = extdef->next;
    CodeList* code1 = translate_ExtDef(extdef);
    CodeList* code2 = translate_ExtDefList(extdeflist);
    return join_codelist(code1, code2);  // 高层只负责拼接
}

static CodeList* translate_Program(Node* node) {
    if (!node || !is_node(node, "Program"))
        return NULL;
    return translate_ExtDefList(node->child);
}

// 中间代码生成入口
void generate_intercode(Node* root, FILE* out_file) {
    if (!root || !out_file) return;
    CodeList* codes = translate_Program(root);
    print_codelist(out_file, codes);
}