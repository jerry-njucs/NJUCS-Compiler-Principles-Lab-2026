#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "Type.h"

#define HASH_SIZE 0x4000

typedef struct Symbol_ *Symbol;

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_STRUCT
} SymbolKind;

struct Symbol_ {
    char* name;
    SymbolKind kind;
    int lineno;

    union {
        Type var_type;

        struct {
            Type ret_type;
            FieldList params;
        } func_info;

        Type struct_type;
    } u;

    Symbol next; // for hash table chaining
};

void init_symbol_table();
void destroy_symbol_table();

Symbol lookup(const char* name);

int insert_var(const char* name, Type type, int lineno);
int insert_func(const char* name, Type ret_type, FieldList params, int lineno);
int insert_struct(const char* name, Type type, int lineno);

unsigned int hash_pjw(const char* name);

#endif