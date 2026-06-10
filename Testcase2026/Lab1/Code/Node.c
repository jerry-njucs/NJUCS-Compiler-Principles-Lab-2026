#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Node.h"

bool isTerminal(const char* name) {
    const char* terminals[] = {
        "STRUCT", "RETURN", "IF", "ELSE", "WHILE",
        "TYPE", "SEMI", "COMMA", "ASSIGNOP", "RELOP",
        "PLUS", "MINUS", "STAR", "DIV", "AND",
        "OR", "DOT", "NOT", "LP", "RP",
        "LB", "RB", "LC", "RC", "ID",
        "INT", "FLOAT"
    };
    int num = sizeof(terminals) / sizeof(terminals[0]);
    for (int i = 0; i < num; i++) {
        if (strcmp(name, terminals[i]) == 0) {
            return true;
        }
    }
    return false;
}

struct Node* createNode(const char* type) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    memset(node, 0, sizeof(struct Node));
    strcpy(node->type, type);
    node->next = NULL;
    node->child = NULL;
    return node;
}

void printTree(struct Node* root, int level) {
    if (root == NULL) {
        return;
    }
    for (int i = 0; i < 2 * level; i++) {
        printf(" ");
    }
    printNode(root);
    Node* child = root->child;
    while (child != NULL) {
        printTree(child, level + 1);
        child = child->next;
    }
}

void printNode(struct Node* node) {
    if (node == NULL) {
        return;
    }
    if (!isTerminal(node->type)) {
        printf("%s (%d)\n", node->type, node->lineno);
    }
    else {
        if (strcmp(node->type, "ID") == 0) {
            printf("%s: %s\n", node->type, node->idname);
        }
        else if (strcmp(node->type, "TYPE") == 0) {
            if (node->datatype == DATA_FLOAT) {
                printf("%s: float\n", node->type);
            }
            else if (node->datatype == DATA_INT) {
                printf("%s: int\n", node->type);
            }
        }
        else if (strcmp(node->type, "INT") == 0) {
            printf("%s: %d\n", node->type, node->intvalue);
        }
        else if (strcmp(node->type, "FLOAT") == 0) {
            printf("%s: %f\n", node->type, node->floatvalue);
        }
        else {
            printf("%s\n", node->type);
        }
    }
}