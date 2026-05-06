#ifndef TYPE_H
#define TYPE_H

typedef struct Type_* Type;
typedef struct FieldList_* FieldList;

struct Type_ {
    enum { BASIC, ARRAY, STRUCTURE } kind;
    union {
        // basic
        int basic;

        // array
        struct { Type elem; int size; } array;

        // structure
        FieldList structure;
    } content;
};

struct FieldList_ {
    char* name;
    Type type;
    FieldList tail;
};

#endif