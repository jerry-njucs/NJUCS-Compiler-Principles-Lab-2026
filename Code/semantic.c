#include "Type.h"
#include "Symbol_table.h"
#include "Node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEM_INT   0
#define SEM_FLOAT 1

static int basic_from_type_node(Node* type_node);
static Node* get_vardec_id(Node* vardec);

// some basic helpers
static int is_node(Node* n, const char* t) {
    return n && strcmp(n->type, t) == 0;
}

static Node* child_at(Node* n, int idx) {
    Node* c = n ? n->child : NULL;
    while (c && idx > 0) {
        c = c->next;
        --idx;
    }
    return c;
}

void print_error(int type, int lineno, const char* msg, const char* name) {  // error output api
    if (name) {
        printf("Error type %d at Line %d: %s \"%s\".\n", type, lineno, msg, name);
    } else {
        printf("Error type %d at Line %d: %s.\n", type, lineno, msg);
    }
}

static char* sem_strdup(const char* s) {
    size_t n;
    char* p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

// make types
static Type make_basic_type(int basic) {
    Type t = (Type)calloc(1, sizeof(struct Type_));
    if (!t) return NULL;
    t->kind = BASIC;
    t->content.basic = basic; // 0:int, 1:float
    return t;
}

static Type make_struct_type(FieldList fields) {
    Type t = (Type)calloc(1, sizeof(struct Type_));
    if (!t) return NULL;
    t->kind = STRUCTURE;
    t->content.structure = fields;
    return t;
}

static FieldList make_field(const char* name, Type type) {
    FieldList f = (FieldList)calloc(1, sizeof(struct FieldList_));
    if (!f) return NULL;
    f->name = sem_strdup(name);
    f->type = type;
    f->tail = NULL;
    return f;
}

static Type parse_specifier(Node* specifier);
static Type parse_struct_specifier(Node* ss);
static int type_equal(Type a, Type b);
static int is_lvalue_exp(Node* exp);
static FieldList collect_varlist_params_ex(Node* varlist, int insert_vars);
static FieldList collect_varlist_params_decl(Node* varlist);
static void handle_function_decl_or_def(Node* specifier, Node* fundec, int is_definition);
static void report_error18_for_undefined_declared_functions(void);
static FieldList parse_struct_deflist(Node* deflist);

static Type infer_exp_type(Node* exp, int report_undef);
static void dfs_semantic(Node* root, Type cur_func_ret, int in_struct_def);
static void handle_stmt(Node* stmt, Type cur_func_ret);

// VarDec -> ID | VarDec LB INT RB
static Type build_type_from_vardec(Node* vardec, Type base_type) {
    Node* c0;
    Node* c2;
    Type t;

    if (!vardec) return base_type;
    c0 = child_at(vardec, 0);
    if (!c0) return base_type;

    if (is_node(c0, "ID")) {
        return base_type;
    }

    if (is_node(c0, "VarDec")) {
        t = (Type)calloc(1, sizeof(struct Type_));
        if (!t) return base_type;
        t->kind = ARRAY;
        t->content.array.elem = build_type_from_vardec(c0, base_type);

        c2 = child_at(vardec, 2); /* INT */
        t->content.array.size = (c2 && is_node(c2, "INT")) ? c2->intvalue : 0;
        return t;
    }

    return base_type;
}

static Node* get_vardec_id(Node* vardec) {
    Node* c0;
    if (!vardec) return NULL;
    c0 = child_at(vardec, 0);
    if (!c0) return NULL;

    if (is_node(c0, "ID")) return c0;
    if (is_node(c0, "VarDec")) return get_vardec_id(c0);
    return NULL;
}

/* collect params of function
 * ParamDec -> Specifier VarDec
 * VarList  -> ParamDec COMMA VarList | ParamDec
 */
static FieldList collect_varlist_params_ex(Node* varlist, int insert_vars) {
    Node* paramdec;
    Node* next_list;
    FieldList head = NULL, tail = NULL;

    if (!varlist) return NULL;

    paramdec = child_at(varlist, 0);
    if (paramdec && is_node(paramdec, "ParamDec")) {
        Node* specifier = child_at(paramdec, 0);
        Node* vardec = child_at(paramdec, 1);
        Node* id = get_vardec_id(vardec);
        Type base_t = parse_specifier(specifier);
        Type real_t = build_type_from_vardec(vardec, base_t);

        if (id && is_node(id, "ID")) {
            FieldList one = make_field(id->idname, real_t);
            if (one) head = tail = one;
            if (insert_vars) {
                if (!insert_var(id->idname, real_t, id->lineno, 1)) {
                    print_error(3, id->lineno, "Redefined variable", id->idname);
                }
            }
        }
    }

    next_list = child_at(varlist, 2);
    if (next_list && is_node(next_list, "VarList")) {
        FieldList rest = collect_varlist_params_ex(next_list, insert_vars);
        if (!head) return rest;
        tail->tail = rest;
    }

    return head;
}

// definition
static FieldList collect_varlist_params(Node* varlist) {
    return collect_varlist_params_ex(varlist, 1);
}

// declaration only, no variable insertion
static FieldList collect_varlist_params_decl(Node* varlist) {
    return collect_varlist_params_ex(varlist, 0);
}

typedef struct FuncTrack_ {
    char* name;
    Type ret_type;
    FieldList params;
    int first_decl_line;
    int defined;
    struct FuncTrack_* next;
} FuncTrack;

static FuncTrack* g_func_tracks = NULL;

static FuncTrack* find_func_track(const char* name) {
    FuncTrack* p = g_func_tracks;
    while (p) {
        if (p->name && name && strcmp(p->name, name) == 0) return p;
        p = p->next;
    }
    return NULL;
}

static int param_types_equal(FieldList a, FieldList b) {
    while (a && b) {
        if (!type_equal(a->type, b->type)) return 0;
        a = a->tail;
        b = b->tail;
    }
    return (a == NULL && b == NULL);
}

static int func_sig_equal(Type ret_a, FieldList pa, Type ret_b, FieldList pb) {
    return type_equal(ret_a, ret_b) && param_types_equal(pa, pb);
}

static void add_func_track(const char* name, Type ret_type, FieldList params, int line, int defined) {
    FuncTrack* n = (FuncTrack*)calloc(1, sizeof(FuncTrack));
    if (!n) return;
    n->name = sem_strdup(name);
    n->ret_type = ret_type;
    n->params = params;
    n->first_decl_line = line;
    n->defined = defined;
    n->next = g_func_tracks;
    g_func_tracks = n;
}

static void report_error18_for_undefined_declared_functions(void) {
    FuncTrack* p = g_func_tracks;
    while (p) {
        if (!p->defined) {
            print_error(18, p->first_decl_line, "Undefined function", p->name);
        }
        p = p->next;
    }
}

/* 实参与形参匹配（错误9） 
 * Args -> Exp COMMA Args | Exp
 */
static int args_match(FieldList formal, Node* args) {
    Node* exp;
    Node* rest;
    Type actual_t;

    if (!args) return formal == NULL;
    if (!formal) return 0;

    exp = child_at(args, 0);
    if (!exp || !is_node(exp, "Exp")) return 0;

    actual_t = infer_exp_type(exp, 0);
    if (!actual_t || !type_equal(actual_t, formal->type)) return 0;

    rest = child_at(args, 2);
    if (rest && is_node(rest, "Args")) {
        return args_match(formal->tail, rest);
    }
    return formal->tail == NULL;
}

// Specifier -> TYPE | StructSpecifier
static Type parse_specifier(Node* specifier) {
    Node* c0 = child_at(specifier, 0);
    if (!c0) return make_basic_type(SEM_INT);

    if (is_node(c0, "TYPE")) {
        return make_basic_type(basic_from_type_node(c0));
    }

    if (is_node(c0, "StructSpecifier")) {
        return parse_struct_specifier(c0);
    }

    return make_basic_type(SEM_INT);
}

// ExtDecList -> VarDec | VarDec COMMA ExtDecList
static void handle_extdeclist(Node* extdeclist, Type t) {
    Node* vardec;
    Node* next_list;

    if (!extdeclist) return;

    vardec = child_at(extdeclist, 0);
    if (vardec && is_node(vardec, "VarDec")) {
        Node* id = get_vardec_id(vardec);
        Type real_t = build_type_from_vardec(vardec, t);
        if (id && is_node(id, "ID")) {
            if (!insert_var(id->idname, real_t, id->lineno, 0)) {
                print_error(3, id->lineno, "Redefined variable", id->idname);
            }
        }
    }

    next_list = child_at(extdeclist, 2);
    if (next_list && is_node(next_list, "ExtDecList")) {
        handle_extdeclist(next_list, t);
    }
}

// DecList -> Dec | Dec COMMA DecList
static void handle_declist(Node* declist, Type t) {
    Node* dec;
    Node* next_list;

    if (!declist) return;

    dec = child_at(declist, 0);
    if (dec && is_node(dec, "Dec")) {
        Node* vardec = child_at(dec, 0);
        if (vardec && is_node(vardec, "VarDec")) {
            Node* id = get_vardec_id(vardec);
            Type real_t = build_type_from_vardec(vardec, t);
            if (id && is_node(id, "ID")) {
                if (!insert_var(id->idname, real_t, id->lineno, 0)) {
                    print_error(3, id->lineno, "Redefined variable", id->idname);
                }
            }
        }
    }

    next_list = child_at(declist, 2);
    if (next_list && is_node(next_list, "DecList")) {
        handle_declist(next_list, t);
    }
}

// VarList -> ParamDec COMMA VarList | ParamDec
static void handle_varlist_params(Node* varlist) {
    Node* paramdec;
    Node* next_list;

    if (!varlist) return;

    paramdec = child_at(varlist, 0);
    if (paramdec && is_node(paramdec, "ParamDec")) {
        Node* specifier = child_at(paramdec, 0);
        Node* vardec = child_at(paramdec, 1);
        Node* id = get_vardec_id(vardec);
        Type base_t = parse_specifier(specifier);
        Type real_t = build_type_from_vardec(vardec, base_t);

        if (id && is_node(id, "ID")) {
            if (!insert_var(id->idname, real_t, id->lineno, 1)) {
                print_error(3, id->lineno, "Redefined variable", id->idname);
            }
        }
    }

    next_list = child_at(varlist, 2);
    if (next_list && is_node(next_list, "VarList")) {
        handle_varlist_params(next_list);
    }
}

// Def -> Specifier DecList SEMI
static void handle_def(Node* def) {
    Node* specifier = child_at(def, 0);
    Node* declist = child_at(def, 1);

    if (!specifier || !declist) return;

    if (is_node(declist, "DecList")) {
        Type t = parse_specifier(specifier);
        handle_declist(declist, t);
    }
}

// ExtDef:
//   Specifier ExtDecList SEMI
// | Specifier SEMI
// | Specifier FunDec CompSt
// | Specifier FunDec SEMI 
static void handle_extdef(Node* extdef) {
    Node* specifier = child_at(extdef, 0);
    Node* second = child_at(extdef, 1);
    Node* third = child_at(extdef, 2);

    if (!specifier || !second) return;

    if (is_node(second, "ExtDecList")) {
        Type t = parse_specifier(specifier);
        handle_extdeclist(second, t);
        return;
    }

    // Specifier SEMI
    if (is_node(second, "SEMI")) {
        (void)parse_specifier(specifier);
        return;
    }

    if (is_node(second, "FunDec")) {
        if (third && is_node(third, "CompSt")) {
            handle_function_decl_or_def(specifier, second, 1);  /* 定义 */
        } else if (third && is_node(third, "SEMI")) {
            handle_function_decl_or_def(specifier, second, 0);  /* 声明 */
        }
        return;
    }
}

static void handle_exp(Node* exp) {
    (void)infer_exp_type(exp, 1);
}

static Type infer_exp_type(Node* exp, int report_undef) {
    Node* c0;
    Node* c1;
    Node* c2;
    Node* c3;

    if (!exp || !is_node(exp, "Exp")) return NULL;

    c0 = child_at(exp, 0);
    c1 = child_at(exp, 1);
    c2 = child_at(exp, 2);
    c3 = child_at(exp, 3);
    if (!c0) return NULL;

    /* Exp -> ID */
    if (is_node(c0, "ID") && c1 == NULL) {
        Symbol s = lookup(c0->idname);
        if (!s || s->kind != SYM_VAR) {
            if (report_undef) print_error(1, c0->lineno, "Undefined variable", c0->idname);
            return NULL;
        }
        return s->u.var_type;
    }

    /* Exp -> INT / FLOAT */
    if (is_node(c0, "INT"))   return make_basic_type(SEM_INT);
    if (is_node(c0, "FLOAT")) return make_basic_type(SEM_FLOAT);

    /* Exp -> LP Exp RP */
    if (is_node(c0, "LP") && c1 && is_node(c1, "Exp")) {
        return infer_exp_type(c1, 0);
    }

    /* Exp -> MINUS Exp */
    if (is_node(c0, "MINUS") && c1 && is_node(c1, "Exp")) {
        Type t = infer_exp_type(c1, 0);
        if (!t) return NULL;
        if (t->kind != BASIC) {
            if (report_undef) print_error(7, c0->lineno, "Type mismatched for operands", NULL);
            return NULL;
        }
        return t;
    }

    /* Exp -> NOT Exp */
    if (is_node(c0, "NOT") && c1 && is_node(c1, "Exp")) {
        Type t = infer_exp_type(c1, 0);
        if (!t) return NULL;
        if (t->kind != BASIC || t->content.basic != SEM_INT) {
            if (report_undef) print_error(7, c0->lineno, "Type mismatched for operands", NULL);
            return NULL;
        }
        return make_basic_type(SEM_INT);
    }

    /* Exp -> ID LP RP | ID LP Args RP */
    if (is_node(c0, "ID") && c1 && is_node(c1, "LP")) {
        Symbol s = lookup(c0->idname);

        if (!s) {
            if (report_undef) print_error(2, c0->lineno, "Undefined function", c0->idname);
            return NULL;
        }

        if (s->kind != SYM_FUNC) {
            if (report_undef) print_error(11, c0->lineno, "Not a function", c0->idname);
            return NULL;
        }

        if (c2 && is_node(c2, "RP")) {
            if (s->u.func_info.params != NULL) {
                if (report_undef) print_error(9, c0->lineno, "Function arguments mismatched", c0->idname);
            }
        } else if (c2 && is_node(c2, "Args")) {
            if (!args_match(s->u.func_info.params, c2)) {
                if (report_undef) print_error(9, c0->lineno, "Function arguments mismatched", c0->idname);
            }
        }

        return s->u.func_info.ret_type;
    }

    /* Exp -> Exp ASSIGNOP Exp */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "ASSIGNOP") &&
        c2 && is_node(c2, "Exp")) {
        Type lt = infer_exp_type(c0, 0);
        Type rt = infer_exp_type(c2, 0);

        if (!lt || !rt) return NULL; /* 子表达式已报错，不级联 */

        if (!is_lvalue_exp(c0)) {
            if (report_undef) print_error(6, c1->lineno, "The left-hand side of an assignment must be a variable", NULL);
        }
        if (!type_equal(lt, rt)) {
            if (report_undef) print_error(5, c1->lineno, "Type mismatched for assignment", NULL);
        }
        return lt;
    }

    /* Exp -> Exp RELOP Exp */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "RELOP") &&
        c2 && is_node(c2, "Exp")) {
        Type lt = infer_exp_type(c0, 0);
        Type rt = infer_exp_type(c2, 0);

        if (!lt || !rt) return NULL;

        if (lt->kind != BASIC || rt->kind != BASIC || !type_equal(lt, rt)) {
            if (report_undef) print_error(7, c1->lineno, "Type mismatched for operands", NULL);
            return NULL;
        }
        return make_basic_type(SEM_INT);
    }

    /* Exp -> Exp PLUS/MINUS/STAR/DIV Exp */
    if (c0 && is_node(c0, "Exp") && c1 &&
        (is_node(c1, "PLUS") || is_node(c1, "MINUS") || is_node(c1, "STAR") || is_node(c1, "DIV")) &&
        c2 && is_node(c2, "Exp")) {
        Type lt = infer_exp_type(c0, 0);
        Type rt = infer_exp_type(c2, 0);

        if (!lt || !rt) return NULL;

        if (lt->kind != BASIC || rt->kind != BASIC || !type_equal(lt, rt)) {
            if (report_undef) print_error(7, c1->lineno, "Type mismatched for operands", NULL);
            return NULL;
        }
        return lt;
    }

    /* Exp -> Exp AND/OR Exp */
    if (c0 && is_node(c0, "Exp") && c1 &&
        (is_node(c1, "AND") || is_node(c1, "OR")) &&
        c2 && is_node(c2, "Exp")) {
        Type lt = infer_exp_type(c0, 0);
        Type rt = infer_exp_type(c2, 0);

        if (!lt || !rt) return NULL;

        if (lt->kind != BASIC || rt->kind != BASIC ||
            lt->content.basic != SEM_INT || rt->content.basic != SEM_INT) {
            if (report_undef) print_error(7, c1->lineno, "Type mismatched for operands", NULL);
            return NULL;
        }
        return make_basic_type(SEM_INT);
    }

    /* Exp -> Exp LB Exp RB */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "LB") &&
        c2 && is_node(c2, "Exp") && c3 && is_node(c3, "RB")) {
        Type arr_t = infer_exp_type(c0, 0);
        Type idx_t = infer_exp_type(c2, 0);

        if (!arr_t) return NULL;
        if (arr_t->kind != ARRAY) {
            if (report_undef) print_error(10, c1->lineno, "Not an array", NULL);
            return NULL;
        }

        if (!idx_t) return NULL;
        if (idx_t->kind != BASIC || idx_t->content.basic != SEM_INT) {
            if (report_undef) print_error(12, c1->lineno, "Array index is not an integer", NULL);
            return NULL;
        }

        return arr_t->content.array.elem;
    }

    /* Exp -> Exp DOT ID */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "DOT") &&
        c2 && is_node(c2, "ID")) {
        Type st = infer_exp_type(c0, 0);

        if (!st) return NULL;

        if (st->kind != STRUCTURE) {
            if (report_undef) print_error(13, c1->lineno, "Illegal use of \".\"", NULL);
            return NULL;
        }

        {
            FieldList f = st->content.structure;
            while (f) {
                if (f->name && strcmp(f->name, c2->idname) == 0) return f->type;
                f = f->tail;
            }
            if (report_undef) print_error(14, c2->lineno, "Non-existent field", c2->idname);
        }
        return NULL;
    }

    return NULL;
}

static void handle_stmt(Node* stmt, Type cur_func_ret) {
    Node* c0 = child_at(stmt, 0);

    if (!stmt || !c0) return;

    /* Stmt -> RETURN Exp SEMI  (错误8) */
    if (is_node(c0, "RETURN")) {
        Node* exp = child_at(stmt, 1);
        Type rt = infer_exp_type(exp, 0);
        if (cur_func_ret && rt && !type_equal(cur_func_ret, rt)) {
            print_error(8, c0->lineno, "Type mismatched for return", NULL);
        }
        return;
    }

    /* Stmt -> IF LP Exp RP Stmt [ELSE Stmt]
       Stmt -> WHILE LP Exp RP Stmt
       条件必须 int（错误7） */
    if (is_node(c0, "IF") || is_node(c0, "WHILE")) {
        Node* cond = child_at(stmt, 2); /* IF/WHILE LP Exp RP ... */
        Type t = infer_exp_type(cond, 0);

        // 仅当条件类型可确定且不是int时，报7；NULL表示下层已报错 
        if (t && (t->kind != BASIC || t->content.basic != SEM_INT)) {
            print_error(7, c0->lineno, "Type mismatched for operands", NULL);
        }
    }
}

static void dfs_semantic(Node* root, Type cur_func_ret, int in_struct_def) {
    Node* c;

    if (!root) return;

    if (is_node(root, "ExtDef")) {
        Node* spec = child_at(root, 0);
        Node* second = child_at(root, 1);
        Node* third = child_at(root, 2);

        handle_extdef(root);

        if (spec && second && third && is_node(second, "FunDec") && is_node(third, "CompSt")) {
            Type ret_t = parse_specifier(spec);
            for (c = root->child; c; c = c->next) {
                if (c == third) dfs_semantic(c, ret_t, in_struct_def);
                else dfs_semantic(c, cur_func_ret, in_struct_def);
            }
            return;
        }
    } else if (is_node(root, "Def")) {
        if (!in_struct_def) handle_def(root);
    } else if (is_node(root, "Exp")) {
        handle_exp(root);
    } else if (is_node(root, "Stmt")) {
        handle_stmt(root, cur_func_ret);
    }

    /* 进入 StructSpecifier 的 DefList 子树时，打开 in_struct_def */
    if (is_node(root, "StructSpecifier")) {
        Node* second = child_at(root, 1);
        Node* deflist = child_at(root, 3);
        if (second && is_node(second, "OptTag") && deflist && is_node(deflist, "DefList")) {
            for (c = root->child; c; c = c->next) {
                if (c == deflist) dfs_semantic(c, cur_func_ret, 1);
                else dfs_semantic(c, cur_func_ret, in_struct_def);
            }
            return;
        }
    }

    for (c = root->child; c; c = c->next) {
        dfs_semantic(c, cur_func_ret, in_struct_def);
    }
}

void semantic_check(struct Node *root) {
    if (!root) return;
    init_symbol_table();
    dfs_semantic(root, NULL, 0);

    // 错误18（声明未定义）
    report_error18_for_undefined_declared_functions();
    // 符号表，你要至少活到中间代码生成结束呀！！！
    //destroy_symbol_table();
}

static int type_equal(Type a, Type b) {
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;

    switch (a->kind) {
        case BASIC:
            return a->content.basic == b->content.basic;
        case ARRAY:
            return type_equal(a->content.array.elem, b->content.array.elem);

        case STRUCTURE:
            // 名等价
            return a == b;

        default:
            return 0;
    }
}

// 左值判定
static int is_lvalue_exp(Node* exp) {
    Node* c0;
    Node* c1;
    Node* c2;
    Node* c3;

    if (!exp || !is_node(exp, "Exp")) return 0;
    c0 = child_at(exp, 0);
    c1 = child_at(exp, 1);
    c2 = child_at(exp, 2);
    c3 = child_at(exp, 3);

    /* Exp -> ID */
    if (c0 && is_node(c0, "ID") && c1 == NULL) return 1;

    /* Exp -> Exp LB Exp RB */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "LB") &&
        c2 && is_node(c2, "Exp") && c3 && is_node(c3, "RB")) return 1;

    /* Exp -> Exp DOT ID */
    if (c0 && is_node(c0, "Exp") && c1 && is_node(c1, "DOT") &&
        c2 && is_node(c2, "ID") && c3 == NULL) return 1;

    return 0;
}

static Type parse_struct_specifier(Node* ss) {
    Node* p;
    Node* lc = NULL;

    if (!ss) return make_struct_type(NULL);

    for (p = ss->child; p; p = p->next) {
        if (is_node(p, "LC")) {
            lc = p;
            break;
        }
    }

    if (lc) {
        Node* id = NULL;
        Node* deflist = NULL;
        FieldList fields = NULL;
        Type st;

        // 找可选结构体名（在 LC 之前）
        {
            Node* prev = NULL;
            for (p = ss->child; p && p != lc; p = p->next) prev = p;

            if (prev && prev != ss->child) {
                if (is_node(prev, "OptTag")) id = child_at(prev, 0);
                else if (is_node(prev, "ID")) id = prev; /* 兼容不包 OptTag 的树 */
            }
        }

        for (p = lc->next; p; p = p->next) {
            if (is_node(p, "DefList")) {
                deflist = p;
                break;
            }
        }

        if (deflist) fields = parse_struct_deflist(deflist);
        st = make_struct_type(fields);

        // 命名结构体插表并检查重名（16）
        if (id && is_node(id, "ID")) {
            Symbol old = lookup(id->idname);
            if (old && (old->kind == SYM_STRUCT || old->kind == SYM_VAR)) {
                print_error(16, id->lineno, "Duplicated name", id->idname);
            } else if (!insert_struct(id->idname, st, id->lineno)) {
                print_error(16, id->lineno, "Duplicated name", id->idname);
            }
        }

        return st;
    }

    {
        Node* second = child_at(ss, 1);
        Node* id = NULL;

        if (second && is_node(second, "Tag")) id = child_at(second, 0);
        else if (second && is_node(second, "ID")) id = second;

        if (id && is_node(id, "ID")) {
            Symbol s = lookup(id->idname);
            if (s && s->kind == SYM_STRUCT) return s->u.struct_type;
            print_error(17, id->lineno, "Undefined structure", id->idname);
        }
    }

    return make_struct_type(NULL);
}

static int basic_from_type_node(Node* type_node) {
    if (!type_node) return SEM_INT;

    if (type_node->datatype == DATA_FLOAT) return SEM_FLOAT;
    if (type_node->datatype == DATA_INT)   return SEM_INT;

    if (strcmp(type_node->idname, "float") == 0) return SEM_FLOAT;
    return SEM_INT;
}

static int field_exists(FieldList head, const char* name) {
    FieldList p = head;
    while (p) {
        if (p->name && name && strcmp(p->name, name) == 0) return 1;
        p = p->tail;
    }
    return 0;
}

static void append_field(FieldList* head, FieldList* tail, FieldList one) {
    if (!one) return;
    if (!*head) {
        *head = *tail = one;
    } else {
        (*tail)->tail = one;
        *tail = one;
    }
}

// DecList -> Dec | Dec COMMA DecList 
static void collect_struct_declist(Node* declist, Type base, FieldList* head, FieldList* tail) {
    Node* dec;
    Node* next_list;

    if (!declist) return;

    dec = child_at(declist, 0);
    if (dec && is_node(dec, "Dec")) {
        Node* vardec = child_at(dec, 0);
        Node* assignop = child_at(dec, 1);
        Node* id = get_vardec_id(vardec);
        Type real_t = build_type_from_vardec(vardec, base);

        /* 15: 结构体域初始化非法 */
        if (assignop && is_node(assignop, "ASSIGNOP")) {
            print_error(15, assignop->lineno, "Initialized field in struct definition", NULL);
        }

        if (id && is_node(id, "ID")) {
            /* 15: 同一结构体内域重名 */
            if (field_exists(*head, id->idname)) {
                print_error(15, id->lineno, "Redefined field", id->idname);
            } else {
                append_field(head, tail, make_field(id->idname, real_t));
            }
        }
    }

    next_list = child_at(declist, 2);
    if (next_list && is_node(next_list, "DecList")) {
        collect_struct_declist(next_list, base, head, tail);
    }
}

/* DefList -> Def DefList | empty */
static FieldList parse_struct_deflist(Node* deflist) {
    FieldList head = NULL, tail = NULL;
    Node* cur = deflist;

    while (cur) {
        Node* def = child_at(cur, 0);
        Node* next_deflist = child_at(cur, 1);

        if (def && is_node(def, "Def")) {
            Node* specifier = child_at(def, 0);
            Node* declist = child_at(def, 1);
            Type base = parse_specifier(specifier);
            collect_struct_declist(declist, base, &head, &tail);
        }

        if (next_deflist && is_node(next_deflist, "DefList")) cur = next_deflist;
        else break;
    }

    return head;
}

static void handle_function_decl_or_def(Node* specifier, Node* fundec, int is_definition) {
    Node* id = child_at(fundec, 0);       /* FunDec -> ID LP ... RP */
    Node* varlist = child_at(fundec, 2);
    Type ret_t;
    FieldList params = NULL;
    FuncTrack* old;

    if (!specifier || !fundec || !id || !is_node(id, "ID")) return;

    ret_t = parse_specifier(specifier);

    if (varlist && is_node(varlist, "VarList")) {
        params = is_definition ? collect_varlist_params(varlist)
                               : collect_varlist_params_decl(varlist);
    }

    old = find_func_track(id->idname);

    if (!old) {
        /* 首次出现：插函数符号（供调用检查）+ 建跟踪记录 */
        if (!insert_func(id->idname, ret_t, params, id->lineno)) {
            print_error(4, id->lineno, "Redefined function", id->idname);
            return;
        }
        add_func_track(id->idname, ret_t, params, id->lineno, is_definition ? 1 : 0);
        return;
    }

    if (is_definition && old->defined) {
        print_error(4, id->lineno, "Redefined function", id->idname);
        return;
    }

    /* 其余场景签名冲突才是19（声明-声明 / 声明-定义 / 定义后声明） */
    if (!func_sig_equal(old->ret_type, old->params, ret_t, params)) {
        print_error(19, id->lineno, "Inconsistent declaration of function", id->idname);
        return;
    }

    /* 声明后首次匹配定义 */
    if (is_definition) {
        old->defined = 1;
    }
}