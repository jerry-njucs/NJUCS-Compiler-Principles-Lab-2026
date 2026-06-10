#ifndef NODE_H
#define NODE_H

#include <stdbool.h>

typedef enum {
    RELOP_GT = 1, RELOP_LT, RELOP_GE, RELOP_LE, RELOP_EQ, RELOP_NE
} RelopType;

typedef enum {
    DATA_INT = 1, DATA_FLOAT
} DataType;

typedef struct Node {
    struct Node* child;
    struct Node* next;

    char type[32];
    int lineno;

    union {
        int intvalue;
        float floatvalue;
        int reloptype;
        int datatype;
        char idname[32];
    };
} Node;

Node* createNode(const char* type);
bool isTerminal(const char* name);
void printTree(Node* root, int level);
void printNode(Node* node);
#endif