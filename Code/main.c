#include <stdio.h>
#include "Node.h"

void semantic_check(struct Node* node);
void generate_intercode(struct Node* node, FILE* out_file);

extern FILE *yyin;
extern void yyrestart(FILE *input_file);
extern int yyparse(void);

extern Node* root;
extern int lexicalError;
extern int syntaxError;

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <input.cmm> <output.ir>\n", argv[0]);
        return 1;
    }

    FILE* in = fopen(argv[1], "r");
    if (!in) {
        perror(argv[1]);
        return 1;
    }

    FILE* out = fopen(argv[2], "w");
    if (!out) {
        perror(argv[2]);
        fclose(in);
        return 1;
    }

    yyrestart(in);
    yyparse();
    fclose(in);

    if (lexicalError == 0 && syntaxError == 0) {
        //printTree(root, 0);
        semantic_check(root);
        generate_intercode(root, out);
    }

    fclose(out);
    return 0;
}