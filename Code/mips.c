#include "mips.h"
#include <stdlib.h>
#include <string.h>

/* ===== 寄存器分配占位接口 ===== */
/* 
 * 后续实现寄存器分配算法时，在此函数中建立局部变量/临时变量到物理寄存器的映射。
 * reg_no 提供了当前指令需要的第几个寄存器，方便我们在初始版中返回不冲突的临时寄存器($t0, $t1, $t2)。
 */
static const char* get_reg(Operand op, int reg_no) {
    if (reg_no == 0) return "$t0";
    if (reg_no == 1) return "$t1";
    if (reg_no == 2) return "$t2";
    return "$t0";
}

/* 辅助函数：处理常量或寄存器的选择 */
static void print_operand_label(FILE* out, Operand op) {
    if (op.kind == OP_LABEL) {
        fprintf(out, "label%d", op.u.label_id);
    } else if (op.kind == OP_FUNCTION) {
        fprintf(out, "%s", op.u.name);
    }
}

static const char* get_branch_inst(const char* relop) {
    if (strcmp(relop, "==") == 0) return "beq";
    if (strcmp(relop, "!=") == 0) return "bne";
    if (strcmp(relop, ">") == 0) return "bgt";
    if (strcmp(relop, "<") == 0) return "blt";
    if (strcmp(relop, ">=") == 0) return "bge";
    if (strcmp(relop, "<=") == 0) return "ble";
    return "beq"; // fallback
}

void generate_target_code(CodeList* intercodes, FILE* out_file) {
    if (!intercodes || !out_file) return;

    /* 1. 打印 .data 节和必要的运行时库 (read/write) */
    fprintf(out_file, ".data\n");
    fprintf(out_file, "_prompt: .asciiz \"Enter an integer:\"\n");
    fprintf(out_file, "_ret: .asciiz \"\\n\"\n");
    fprintf(out_file, ".globl main\n");
    
    fprintf(out_file, "\n.text\n");
    fprintf(out_file, "read:\n"); // SPIM/MARS 库提供系统调用 read
    fprintf(out_file, "  li $v0, 4\n  la $a0, _prompt\n  syscall\n");
    fprintf(out_file, "  li $v0, 5\n  syscall\n  jr $ra\n");
    fprintf(out_file, "\nwrite:\n"); // SPIM/MARS 库提供系统调用 write
    fprintf(out_file, "  li $v0, 1\n  syscall\n");
    fprintf(out_file, "  li $v0, 4\n  la $a0, _ret\n  syscall\n");
    fprintf(out_file, "  move $v0, $0\n  jr $ra\n\n");

    /* 2. 逐条翻译 IR 指令 */
    CodeList* curr = intercodes;
    while (curr) {
        InterCode* code = curr->code;
        if (!code) { curr = curr->next; continue; }

        switch (code->kind) {
            case LABEL:
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, ":\n");
                break;

            case FUNCTION:
                fprintf(out_file, "\n");
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, ":\n");
                /* TODO: 栈帧管理 - Prologue (保存 $ra, $fp, 分配栈空间等) */
                break;

            case ASSIGN:
                if (code->u.assign.right.kind == OP_CONSTANT) {
                    fprintf(out_file, "  li %s, %d\n", get_reg(code->u.assign.left, 0), code->u.assign.right.u.val);
                } else {
                    fprintf(out_file, "  move %s, %s\n", get_reg(code->u.assign.left, 0), get_reg(code->u.assign.right, 1));
                }
                break;

            case PLUS:
                if (code->u.binop.op2.kind == OP_CONSTANT) {
                    fprintf(out_file, "  addi %s, %s, %d\n", get_reg(code->u.binop.result, 0), 
                            get_reg(code->u.binop.op1, 1), code->u.binop.op2.u.val);
                } else {
                    fprintf(out_file, "  add %s, %s, %s\n", get_reg(code->u.binop.result, 0), 
                            get_reg(code->u.binop.op1, 1), get_reg(code->u.binop.op2, 2));
                }
                break;

            case MINUS:
                if (code->u.binop.op2.kind == OP_CONSTANT) {
                    fprintf(out_file, "  addi %s, %s, %d\n", get_reg(code->u.binop.result, 0), 
                            get_reg(code->u.binop.op1, 1), -code->u.binop.op2.u.val);
                } else {
                    fprintf(out_file, "  sub %s, %s, %s\n", get_reg(code->u.binop.result, 0), 
                            get_reg(code->u.binop.op1, 1), get_reg(code->u.binop.op2, 2));
                }
                break;

            case STAR:
                fprintf(out_file, "  mul %s, %s, %s\n", get_reg(code->u.binop.result, 0), 
                        get_reg(code->u.binop.op1, 1), get_reg(code->u.binop.op2, 2));
                break;

            case DIV:
                fprintf(out_file, "  div %s, %s\n", get_reg(code->u.binop.op1, 1), get_reg(code->u.binop.op2, 2));
                fprintf(out_file, "  mflo %s\n", get_reg(code->u.binop.result, 0));
                break;

            case GET_ADDR:
                /* TODO: 需要结合栈帧结构计算局部变量/数组的相对偏移 */
                fprintf(out_file, "  la %s, 0(栈变量偏移计算占位)\n", get_reg(code->u.assign.left, 0));
                break;

            case READ_MEM:
                fprintf(out_file, "  lw %s, 0(%s)\n", get_reg(code->u.assign.left, 0), get_reg(code->u.assign.right, 1));
                break;

            case WRITE_MEM:
                fprintf(out_file, "  sw %s, 0(%s)\n", get_reg(code->u.assign.right, 1), get_reg(code->u.assign.left, 0));
                break;

            case GOTO:
                fprintf(out_file, "  j ");
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, "\n");
                break;

            case IF_GOTO:
                fprintf(out_file, "  %s %s, %s, ", get_branch_inst(code->u.if_goto.relop), 
                        get_reg(code->u.if_goto.x, 0), get_reg(code->u.if_goto.y, 1));
                print_operand_label(out_file, code->u.if_goto.z);
                fprintf(out_file, "\n");
                break;

            case RETURN:
                fprintf(out_file, "  move $v0, %s\n", get_reg(code->u.one.op, 0));
                /* TODO: 栈帧管理 - Epilogue (恢复栈帧后再 jr $ra) */
                fprintf(out_file, "  jr $ra\n");
                break;

            case CALL:
                /* TODO: 调用前应检查现场保存（caller-saved） */
                fprintf(out_file, "  jal ");
                print_operand_label(out_file, code->u.call.func);
                fprintf(out_file, "\n");
                fprintf(out_file, "  move %s, $v0\n", get_reg(code->u.call.ret, 0));
                break;

            case PARAM:
            case ARG:
            case DEC:
                /* TODO: 这里的逻辑强依赖于栈布局，在第三步完善栈管理的逻辑 */
                fprintf(out_file, "  # TODO: 栈空间/参数 操作 (DEC/ARG/PARAM)\n");
                break;

            case READ:
                fprintf(out_file, "  jal read\n");
                fprintf(out_file, "  move %s, $v0\n", get_reg(code->u.one.op, 0));
                break;

            case WRITE:
                fprintf(out_file, "  move $a0, %s\n", get_reg(code->u.one.op, 0));
                fprintf(out_file, "  jal write\n");
                break;

            default:
                break;
        }
        curr = curr->next;
    }
}