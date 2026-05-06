#include "Symbol_table.h"
#include <stdlib.h>
#include <string.h>

static Symbol hash_table[HASH_SIZE];

unsigned int hash_pjw(const char* name) {
    unsigned int val = 0, i;
    for (; *name; ++name) {
        val = (val << 2) + (unsigned int)(*name);
        if ((i = val & ~0x3fffU)) {
            val = (val ^ (i >> 12)) & 0x3fffU;
        }
    }
    return val;
}

void init_symbol_table() {
    for (int i = 0; i < HASH_SIZE; i++)
        hash_table[i] = NULL;
}

void destroy_symbol_table() {
    for (int i = 0; i < HASH_SIZE; i++) {
        Symbol s = hash_table[i];
        while(s) {
            Symbol next = s->next;
            free(s->name);
            free(s);
            s = next;
        }
        hash_table[i] = NULL;
    }
}

Symbol lookup(const char* name) {
    unsigned int target = hash_pjw(name);
    Symbol s = hash_table[target];
    while (s) {
        if (strcmp(s->name, name) == 0)
            return s;
        s = s->next;
    }
    return NULL;
}

static int insert_symbol(Symbol s) {
    if (!s)
        return 0;
    if (lookup(s->name) != NULL)
        return 0;

    unsigned int target = hash_pjw(s->name);
    s->next = hash_table[target];
    hash_table[target] = s;
    return 1;
}

static char* my_strdup(const char* s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static Symbol new_symbol(const char* name, SymbolKind kind, int lineno) {
    Symbol s = (Symbol)calloc(1, sizeof(struct Symbol_));
    if (!s)
        return NULL;
    s->name = my_strdup(name);
    if (!s->name) {
        free(s);
        return NULL;
    }

    s->kind = kind;
    s->lineno = lineno;
    s->next = NULL;
    return s;
}

int insert_var(const char* name, Type type, int lineno) {
    Symbol s = new_symbol(name, SYM_VAR, lineno);
    if (!s)
        return 0;

    s->u.var_type = type;
    if (!insert_symbol(s)) {
        free(s->name);
        free(s);
        return 0;
    }
    return 1;
}

int insert_struct(const char* name, Type type, int lineno) {
    Symbol s = new_symbol(name, SYM_STRUCT, lineno);
    if (!s)
        return 0;

    s->u.struct_type = type;
    if (!insert_symbol(s)) {
        free(s->name);
        free(s);
        return 0;
    }
    return 1;
}

int insert_func(const char* name, Type ret_type, FieldList params, int lineno) {
    Symbol s = new_symbol(name, SYM_FUNC, lineno);
    if (!s)
        return 0;

    s->u.func_info.ret_type = ret_type;
    s->u.func_info.params = params;
    if (!insert_symbol(s)) {
        free(s->name);
        free(s);
        return 0;
    }
    return 1;
}