#include "mips.h"
#include <stdlib.h>
#include <string.h>

/* ===== 1. 寄存器状态池 (Register Pool) ===== */
#define REG_NUM 18 

typedef struct {
    const char* name;   // 物理寄存器名称，如 "$t0"
    int free;           // 1 表示空闲， 0 表示被占用
    char var_name[32];  // 当前存放的变量名（若被占用）
    int dirty;          // 1 表示脏数据（值被修改过，溢出时必须sw），0表示干净
} RegDesc;

static RegDesc regs[REG_NUM];

/* 初始化所有可用于局部寄存器分配的 MIPS 寄存器 */
static void init_regs() {
    const char* reg_names[REG_NUM] = {
        "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9",
        "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7"
    };
    for (int i = 0; i < REG_NUM; i++) {
        regs[i].name = reg_names[i];
        regs[i].free = 1;
        regs[i].var_name[0] = '\0';
        regs[i].dirty = 0;
    }
}

/* ===== 2. 变量栈映射表 (Variable Stack Mapping) ===== */
/* 用于记录局部变量/临时变量 -> 内存(相对 $fp 的偏移) 的映射关系 */
typedef struct VarDesc_ {
    char var_name[32];     // 变量名，如 "v1", "t2"
    int offset;            // 相对于 $fp 的偏移，例如 -4, -8
    struct VarDesc_* next; 
} VarDesc;

static VarDesc* var_map = NULL;
static int frame_size = 0; // 当前函数的栈帧大小

/* 清空变量映射表 (在进入新函数时调用) */
static void clear_var_map() {
    VarDesc* curr = var_map;
    while (curr) {
        VarDesc* temp = curr;
        curr = curr->next;
        free(temp);
    }
    var_map = NULL;
    frame_size = 0;
}

/* 在栈上为变量分配空间，并返回相对于 $fp 的偏移 */
static int allocate_var_offset(const char* name, int size) {
    VarDesc* v = (VarDesc*)malloc(sizeof(VarDesc));
    strncpy(v->var_name, name, 31);
    v->var_name[31] = '\0';
    
    frame_size += size;
    v->offset = -frame_size; 
    
    v->next = var_map;
    var_map = v;
    return v->offset;
}

/* 查找变量在栈上的偏移量。目前采用局部算法，要求此前必然分配过 */
static int get_var_offset(const char* name) {
    VarDesc* curr = var_map;
    while (curr) {
        if (strcmp(curr->var_name, name) == 0) return curr->offset;
        curr = curr->next;
    }
    return 0; // 找不到时的后备返回值
}

/* 辅助函数：将 Operand 转换为唯一的字符串标识，方便存入表和池 */
static void get_operand_name(Operand op, char* buf) {
    if (op.kind == OP_VARIABLE) {
        sprintf(buf, "%s", op.u.name); // 此前 IR 中变量名应已保存在 name (如 "v1")
    } else if (op.kind == OP_TEMP) {
        sprintf(buf, "t%d", op.u.var_id);
    } else {
        buf[0] = '\0';
    }
}

/* ===== 3. 核心分配逻辑 (Allocate, Ensure, Free) ===== */

static int spill_ptr = 0; // 用于轮转替换（Round-Robin）的指针
static int param_cnt = 0;   // 记录进入子函数后遇到了几个 PARAM
static Operand arg_list[32]; // 暂存调用另一函数前的所有 ARG 
static int arg_cnt = 0;      // 暂存数量

/* 寻找一个空闲寄存器。如果不空，就挑一个写回内存(Spill)来腾出位置 */
static int get_free_reg_idx(FILE* out_file) {
    // 1. 先尝试找完全空闲的寄存器
    for (int i = 0; i < REG_NUM; i++) {
        if (regs[i].free) return i;
    }

    // 2. 如果全满，采用轮转贪心法挑一个牺牲者
    int idx = spill_ptr;
    spill_ptr = (spill_ptr + 1) % REG_NUM;

    // 3. 执行溢出 (Spilling)：只有脏数据（被修改过）才需要写回栈
    if (regs[idx].dirty) {
        int offset = get_var_offset(regs[idx].var_name);
        // 生成 sw 指令将该寄存器数据写回到栈里对应的老家
        fprintf(out_file, "  sw %s, %d($fp) # [Spill] 寄存器满，强制写回 %s\n", 
                regs[idx].name, offset, regs[idx].var_name);
    }
    
    // 清理该寄存器状态，返回
    regs[idx].free = 1;
    return idx;
}

/* 为“写入目标”分配寄存器 (等同于 Allocate)
 * 只要一个空位置，不需要从内存加载它之前的值，因为马上要覆盖它 */
static const char* Allocate(Operand op, FILE* out_file) {
    char name[32];
    get_operand_name(op, name);

    // 1. 如果变量本来就在某个寄存器里，直接复用
    for (int i = 0; i < REG_NUM; i++) {
        if (!regs[i].free && strcmp(regs[i].var_name, name) == 0) {
            regs[i].dirty = 1; // 马上要写入，标记为脏
            return regs[i].name;
        }
    }

    // 2. 如果不在，要一个位置
    int idx = get_free_reg_idx(out_file);
    
    // 3. 登记注册
    regs[idx].free = 0;
    strcpy(regs[idx].var_name, name);
    regs[idx].dirty = 1; // 作为左值，马上会被赋值，直接标为脏
    return regs[idx].name;
}

/* 为“读取源头”准备寄存器 (等同于 Ensure)
 * 如果在寄存器里直接用，如果不在内存里，需要发出 lw 指令加载进寄存器 */
static const char* Ensure(Operand op, FILE* out_file) {
    /* 【新增分支：应对读取常量的需求】 */
    if (op.kind == OP_CONSTANT) {
        char const_name[32];
        sprintf(const_name, "c_%d", op.u.val); // 取一个防重复的假名字
        
        // 1. 命中缓存：该常数已经在某个寄存器中
        for (int i = 0; i < REG_NUM; i++) {
            if (!regs[i].free && strcmp(regs[i].var_name, const_name) == 0) {
                return regs[i].name;
            }
        }
        // 2. 未命中缓存：分配新寄存器并立即装载常数
        int idx = get_free_reg_idx(out_file);
        regs[idx].free = 0;
        strcpy(regs[idx].var_name, const_name);
        regs[idx].dirty = 0; // 常数当然不需要写回内存
        fprintf(out_file, "  li %s, %d\n", regs[idx].name, op.u.val);
        return regs[idx].name;
    }

    /* 原本针对普通变量的逻辑 */
    char name[32];
    get_operand_name(op, name);

    // 1. 命中缓存：直接在可用寄存器中找到
    for (int i = 0; i < REG_NUM; i++) {
        if (!regs[i].free && strcmp(regs[i].var_name, name) == 0) {
            return regs[i].name;
        }
    }

    // 2. 缓存未命中（Miss）：需要腾一个位置
    int idx = get_free_reg_idx(out_file);
    regs[idx].free = 0;
    strcpy(regs[idx].var_name, name);
    regs[idx].dirty = 0; // 仅仅是读出，跟内存中的一致，不算脏数据

    // 3. 发射指令：将其从栈存根中重新读取 (Reload)
    int offset = get_var_offset(name);
    fprintf(out_file, "  lw %s, %d($fp) # [Reload] 装载 %s\n", regs[idx].name, offset, name);

    return regs[idx].name;
}

/* 手动释放一个寄存器 (供将来做激进的数据流分析，回收死代码空间) */
static void Free(Operand op) {
    char name[32];
    get_operand_name(op, name);
    for (int i = 0; i < REG_NUM; i++) {
        if (!regs[i].free && strcmp(regs[i].var_name, name) == 0) {
            regs[i].free = 1; 
            return;
        }
    }
}

/* 基本块边界的“大清洗”：把所有修改过的数据存入内存，清空所有寄存器 */
static void spill_all(FILE* out_file) {
    for (int i = 0; i < REG_NUM; i++) {
        if (!regs[i].free) {
            if (regs[i].dirty) {
                int offset = get_var_offset(regs[i].var_name);
                fprintf(out_file, "  sw %s, %d($fp) # [Block End] 洗盘 %s\n", 
                        regs[i].name, offset, regs[i].var_name);
            }
            regs[i].free = 1; // 全部清空，下个基本块大家重新来
        }
    }
}

/* 辅助函数：处理常量或寄存器的选择 */
static void print_operand_label(FILE* out, Operand op) {
    if (op.kind == OP_LABEL) {
        fprintf(out, "label%d", op.u.label_id);
    } else if (op.kind == OP_FUNCTION) {
        // 【新增安全过滤】：防止函数名与 MIPS 保留指令（如 add, sub）冲突
        if (strcmp(op.u.name, "main") == 0 || 
            strcmp(op.u.name, "read") == 0 || 
            strcmp(op.u.name, "write") == 0) {
            fprintf(out, "%s", op.u.name); // 这三个基础函数原样输出
        } else {
            fprintf(out, "func_%s", op.u.name); // 用户函数统一加前缀 func_
        }
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

/* ===== 4. 栈帧预扫描模块 ===== */

/* 辅助检查：如果操作数是变量且未分配偏址，则分配 4 字节 */
static void check_alloc(Operand op) {
    if (op.kind == OP_VARIABLE || op.kind == OP_TEMP) {
        char name[32];
        get_operand_name(op, name);
        if (name[0] != '\0' && get_var_offset(name) == 0) {
            allocate_var_offset(name, 4); // 默认标量分配 4 字节
        }
    }
}

/* 预扫描一个函数内部所有的变量和 DEC 数组需求，预先在栈图上排好座位 */
static void pre_scan_function(CodeList* func_curr) {
    CodeList* curr = func_curr->next;
    while (curr && curr->code->kind != FUNCTION) {
        InterCode* c = curr->code;
        if (!c) { curr = curr->next; continue; }
        
        switch (c->kind) {
            case ASSIGN: case GET_ADDR: case READ_MEM: case WRITE_MEM:
                check_alloc(c->u.assign.left);
                check_alloc(c->u.assign.right);
                break;
            case PLUS: case MINUS: case STAR: case DIV:
                check_alloc(c->u.binop.result);
                check_alloc(c->u.binop.op1);
                check_alloc(c->u.binop.op2);
                break;
            case IF_GOTO:
                check_alloc(c->u.if_goto.x);
                check_alloc(c->u.if_goto.y);
                break;
            case RETURN: case READ: case WRITE: case ARG: case PARAM:
                check_alloc(c->u.one.op);
                break;
            case CALL:
                check_alloc(c->u.call.ret);
                break;
            case DEC: {
                char name[32];
                // 【修复】：将 c->u.dec.op 改为 c->u.dec.x
                get_operand_name(c->u.dec.x, name);
                if (get_var_offset(name) == 0) {
                    allocate_var_offset(name, c->u.dec.size); // 根据要求分配大数组块
                }
                break;
            }
            default: break;
        }
        curr = curr->next;
    }
}

/* ===== 5. 目标代码生成接口 ===== */

void generate_target_code(CodeList* intercodes, FILE* out_file) {
    if (!intercodes || !out_file) return;

    /* 1. 初始化寄存器环境与清空变量映射 */
    init_regs();
    clear_var_map();

    /* 2. 打印 .data 节和必要的运行时库 */
    fprintf(out_file, ".data\n");
    fprintf(out_file, "_prompt: .asciiz \"Enter an integer:\"\n");
    fprintf(out_file, "_ret: .asciiz \"\\n\"\n");
    fprintf(out_file, ".globl main\n");
    
    fprintf(out_file, "\n.text\n");
    fprintf(out_file, "read:\n");
    fprintf(out_file, "  li $v0, 4\n  la $a0, _prompt\n  syscall\n");
    fprintf(out_file, "  li $v0, 5\n  syscall\n  jr $ra\n");
    fprintf(out_file, "\nwrite:\n");
    fprintf(out_file, "  li $v0, 1\n  syscall\n");
    fprintf(out_file, "  li $v0, 4\n  la $a0, _ret\n  syscall\n");
    fprintf(out_file, "  move $v0, $0\n  jr $ra\n\n");

    /* 3. 逐条翻译 IR 指令 */
    CodeList* curr = intercodes;
    while (curr) {
        InterCode* code = curr->code;
        if (!code) { curr = curr->next; continue; }

        switch (code->kind) {
            case LABEL:
                spill_all(out_file);
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, ":\n");
                break;

            case FUNCTION:
                spill_all(out_file);
                fprintf(out_file, "\n");
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, ":\n");
                
                /* [新增] 重置形参计数器 */
                param_cnt = 0;

                clear_var_map();
                pre_scan_function(curr);
                
                /* MIPS ABI Prologue (已有) */
                fprintf(out_file, "  addi $sp, $sp, -8\n"); 
                fprintf(out_file, "  sw $ra, 4($sp)\n");
                fprintf(out_file, "  sw $fp, 0($sp)\n");
                fprintf(out_file, "  move $fp, $sp\n");    
                if (frame_size > 0) {
                    fprintf(out_file, "  addi $sp, $sp, -%d\n", frame_size); // 腾出局部变量空间
                }
                break;

            case ASSIGN:
                if (code->u.assign.right.kind == OP_CONSTANT) {
                    const char* rz = Allocate(code->u.assign.left, out_file);
                    fprintf(out_file, "  li %s, %d\n", rz, code->u.assign.right.u.val);
                } else {
                    const char* rx = Ensure(code->u.assign.right, out_file);
                    const char* rz = Allocate(code->u.assign.left, out_file);
                    fprintf(out_file, "  move %s, %s\n", rz, rx);
                    /* 注：若将来加入数据流分析，可在此处加上：
                       if (code->u.assign.right 后续死亡) Free(code->u.assign.right); */
                }
                break;

            case PLUS:
                if (code->u.binop.op2.kind == OP_CONSTANT) {
                    const char* rx = Ensure(code->u.binop.op1, out_file);
                    const char* rz = Allocate(code->u.binop.result, out_file);
                    fprintf(out_file, "  addi %s, %s, %d\n", rz, rx, code->u.binop.op2.u.val);
                } else {
                    const char* rx = Ensure(code->u.binop.op1, out_file);
                    const char* ry = Ensure(code->u.binop.op2, out_file);
                    const char* rz = Allocate(code->u.binop.result, out_file);
                    fprintf(out_file, "  add %s, %s, %s\n", rz, rx, ry);
                }
                break;

            case MINUS:
                if (code->u.binop.op2.kind == OP_CONSTANT) {
                    const char* rx = Ensure(code->u.binop.op1, out_file);
                    const char* rz = Allocate(code->u.binop.result, out_file);
                    fprintf(out_file, "  addi %s, %s, %d\n", rz, rx, -code->u.binop.op2.u.val);
                } else {
                    const char* rx = Ensure(code->u.binop.op1, out_file);
                    const char* ry = Ensure(code->u.binop.op2, out_file);
                    const char* rz = Allocate(code->u.binop.result, out_file);
                    fprintf(out_file, "  sub %s, %s, %s\n", rz, rx, ry);
                }
                break;

            case STAR: {
                const char* rx = Ensure(code->u.binop.op1, out_file);
                const char* ry = Ensure(code->u.binop.op2, out_file);
                const char* rz = Allocate(code->u.binop.result, out_file);
                fprintf(out_file, "  mul %s, %s, %s\n", rz, rx, ry);
                break;
            }

            case DIV: {
                const char* rx = Ensure(code->u.binop.op1, out_file);
                const char* ry = Ensure(code->u.binop.op2, out_file);
                const char* rz = Allocate(code->u.binop.result, out_file);
                fprintf(out_file, "  div %s, %s\n", rx, ry);
                fprintf(out_file, "  mflo %s\n", rz);
                break;
            }

            case GET_ADDR: {
                /* 取地址：x := &y 。y 必在内存，所以我们直接利用 y 的相对 $fp 偏移量算出真实地址赋给 x */
                char y_name[32];
                get_operand_name(code->u.assign.right, y_name);
                int offset = get_var_offset(y_name); 
                const char* rz = Allocate(code->u.assign.left, out_file);
                fprintf(out_file, "  addi %s, $fp, %d\n", rz, offset);
                break;
            }

            case READ_MEM: {
                /* x := *y。先加载地址 y，再 load 出数值给 x */
                const char* ry = Ensure(code->u.assign.right, out_file);
                const char* rx = Allocate(code->u.assign.left, out_file);
                fprintf(out_file, "  lw %s, 0(%s)\n", rx, ry);
                break;
            }

            case WRITE_MEM: {
                /* *x := y。这两者在此处都是被读出的！目标是写入内存。 */
                const char* ry = Ensure(code->u.assign.right, out_file);
                const char* rx = Ensure(code->u.assign.left, out_file);
                fprintf(out_file, "  sw %s, 0(%s)\n", ry, rx);
                break;
            }

            case GOTO:
                spill_all(out_file);
                fprintf(out_file, "  j ");
                print_operand_label(out_file, code->u.one.op);
                fprintf(out_file, "\n");
                break;

            case IF_GOTO: {
                /* 请注意先后顺序：必须先 Ensure 把操作数读进寄存器，然后再洗盘，最后输出转移指令 */
                const char* rx = Ensure(code->u.if_goto.x, out_file);
                const char* ry = Ensure(code->u.if_goto.y, out_file);
                spill_all(out_file); 
                fprintf(out_file, "  %s %s, %s, ", get_branch_inst(code->u.if_goto.relop), rx, ry);
                print_operand_label(out_file, code->u.if_goto.z);
                fprintf(out_file, "\n");
                break;
            }

            case RETURN: {
                const char* rx = Ensure(code->u.one.op, out_file);
                fprintf(out_file, "  move $v0, %s\n", rx);
                
                spill_all(out_file); // 退栈前洗盘
                
                /* MIPS ABI Epilogue */
                fprintf(out_file, "  move $sp, $fp\n");
                fprintf(out_file, "  lw $fp, 0($sp)\n");
                fprintf(out_file, "  lw $ra, 4($sp)\n");
                fprintf(out_file, "  addi $sp, $sp, 8\n");
                fprintf(out_file, "  jr $ra\n");
                break;
            }

            case CALL: {
                spill_all(out_file); // 【极度重要】将CALL作为基本块边界，所有寄存器落盘，从而自动完美遵守 Caller-saved 约定！

                // 根据标准C--生成规则，ARG是逆序存入数组的。所以 arg_list[arg_cnt - 1] 才是第 1 个参数
                for (int i = 0; i < arg_cnt; i++) {
                    // 正序第 i 个参数，在 arg_list 里的倒数下标：
                    Operand arg_op = arg_list[arg_cnt - 1 - i]; 
                    const char* rx = Ensure(arg_op, out_file); // 这里会读常数或生成lw，加载进物理寄存器
                    
                    if (i < 4) {
                        fprintf(out_file, "  move $a%d, %s\n", i, rx);
                    } else {
                        // 超过 4 个参数，往下压栈
                        fprintf(out_file, "  addi $sp, $sp, -4\n");
                        fprintf(out_file, "  sw %s, 0($sp)\n", rx);
                    }
                }

                fprintf(out_file, "  jal ");
                print_operand_label(out_file, code->u.call.func);
                fprintf(out_file, "\n");
                
                // 返回后立刻恢复栈由于超长传参而产生的偏移
                if (arg_cnt > 4) {
                    fprintf(out_file, "  addi $sp, $sp, %d\n", (arg_cnt - 4) * 4);
                }
                
                // 返回值 v0 移交给真正要求的接收左值
                const char* rz = Allocate(code->u.call.ret, out_file);
                fprintf(out_file, "  move %s, $v0\n", rz);
                
                arg_cnt = 0; // 【调用完成，清空 ARG 收集缓冲区】
                break;
            }

            case READ: {
                spill_all(out_file);
                fprintf(out_file, "  jal read\n");
                const char* rz = Allocate(code->u.one.op, out_file);
                fprintf(out_file, "  move %s, $v0\n", rz);
                break;
            }

            case WRITE: {
                const char* rx = Ensure(code->u.one.op, out_file);
                fprintf(out_file, "  move $a0, %s\n", rx);
                spill_all(out_file);
                fprintf(out_file, "  jal write\n");
                break;
            }

            case DEC:
                /* 栈空间在 pre_scan 阶段已分配完毕，无需汇编指令 */
                fprintf(out_file, "  # [DEC] \n");
                break;

            case PARAM: {
                // 找到该变量在我们刚开辟的栈帧里的固有 offset
                char name[32];
                get_operand_name(code->u.one.op, name);
                int offset = get_var_offset(name);

                if (param_cnt < 4) {
                    // 前 4 个参数坐在 $a0 ~ $a3 里，直接安置
                    fprintf(out_file, "  sw $a%d, %d($fp) # [PARAM] 接收寄存器参数\n", param_cnt, offset);
                } else {
                    // 第 5 个及以后，坐在调用者的栈里。基于当前 $fp 向上找
                    int caller_offset = 8 + (param_cnt - 4) * 4;
                    fprintf(out_file, "  lw $v1, %d($fp) # [PARAM] 接收栈参数\n", caller_offset);
                    fprintf(out_file, "  sw $v1, %d($fp)\n", offset);
                }
                param_cnt++;
                break;
            }
            case ARG: {
                // 暂时不发射汇编，把实参塞进缓冲队列
                arg_list[arg_cnt++] = code->u.one.op;
                break;
            }

            default:
                break;
        }
        curr = curr->next;
    }
}