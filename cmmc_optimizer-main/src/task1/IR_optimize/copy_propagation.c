//
// Created by hby on 22-12-4.
//

#include <copy_propagation.h>


void Fact_def_use_init(Fact_def_use *fact, bool is_top) {
    fact->is_top = is_top;
    Map_IR_var_IR_var_init(&fact->def_to_use);
    Map_IR_var_IR_var_init(&fact->use_to_def);
}

void Fact_def_use_teardown(Fact_def_use *fact) {
    Map_IR_var_IR_var_teardown(&fact->def_to_use);
    Map_IR_var_IR_var_teardown(&fact->use_to_def);
}

//// ============================ Dataflow Analysis ============================

static void CopyPropagation_teardown(CopyPropagation *t) {
    for_map(IR_block_ptr, Fact_def_use_ptr, i, t->mapInFact)
        RDELETE(Fact_def_use, i->val);
    for_map(IR_block_ptr, Fact_def_use_ptr, i, t->mapOutFact)
        RDELETE(Fact_def_use, i->val);
    Map_IR_block_ptr_Fact_def_use_ptr_teardown(&t->mapInFact);
    Map_IR_block_ptr_Fact_def_use_ptr_teardown(&t->mapOutFact);
}

static bool
CopyPropagation_isForward (CopyPropagation *t) {
    return true; // 复制传播是前向分析: 信息沿控制流从 entry 向 exit 传播
}

static Fact_def_use*
CopyPropagation_newBoundaryFact (CopyPropagation *t, IR_function *func) {
    // 前向 Must 分析: Boundary = OUT[Entry]
    // 函数入口处没有任何复制关系 → BOTTOM (is_top = false)
    return NEW(Fact_def_use, false);
}

static Fact_def_use*
CopyPropagation_newInitialFact (CopyPropagation *t) {
    // Must 分析: 初始假设所有复制对都成立 → TOP (is_top = true)
    // 求解过程中通过 intersect 逐步收窄到真正成立的复制对
    return NEW(Fact_def_use, true);
}

static void
CopyPropagation_setInFact (CopyPropagation *t,
                                        IR_block *blk,
                                        Fact_def_use *fact) {
    VCALL(t->mapInFact, set, blk, fact);
}

static void
CopyPropagation_setOutFact (CopyPropagation *t,
                                         IR_block *blk,
                                         Fact_def_use *fact) {
    VCALL(t->mapOutFact, set, blk, fact);
}

static Fact_def_use*
CopyPropagation_getInFact (CopyPropagation *t, IR_block *blk) {
    return VCALL(t->mapInFact, get, blk);
}

static Fact_def_use*
CopyPropagation_getOutFact (CopyPropagation *t, IR_block *blk) {
    return VCALL(t->mapOutFact, get, blk);
}

static bool
CopyPropagation_meetInto (CopyPropagation *t,
                          Fact_def_use *fact,
                          Fact_def_use *target) {
    if(fact->is_top) return false;
    if(target->is_top) {
        target->is_top = false;
        for_map(IR_var, IR_var, it, fact->def_to_use) {
            VCALL(target->def_to_use, insert, it->key, it->val);
            VCALL(target->use_to_def, insert, it->val, it->key);
        }
        return true;
    }
    // Map intersect
    bool updated = false;
    Map_IR_var_IR_var not_exist;
    Map_IR_var_IR_var_init(&not_exist);
    for_map(IR_var, IR_var, it, target->def_to_use)
        if(!VCALL(fact->def_to_use, exist, it->key) || VCALL(fact->def_to_use, get, it->key) != it->val) {
            VCALL(not_exist, insert, it->key, it->val);
            updated = true;
        }
    for_map(IR_var, IR_var, it, not_exist) {
        VCALL(target->def_to_use, delete, it->key);
        VCALL(target->use_to_def, delete, it->val);
    }
    Map_IR_var_IR_var_teardown(&not_exist);
    return updated;
}

void CopyPropagation_transferStmt (CopyPropagation *t,
                                   IR_stmt *stmt,
                                   Fact_def_use *fact) {
    IR_var new_def = VCALL(*stmt, get_def);
    //// copy_kill: 任何对 x 的重新定义都会 kill 涉及 x 的复制对
    if(new_def != IR_VAR_NONE) {
        // 情况1: new_def 是某个复制对的 def (new_def → ?)
        // new_def 被重新定义后, 它不再是任何变量的副本
        if(VCALL(fact->def_to_use, exist, new_def)) {
            IR_var use = VCALL(fact->def_to_use, get, new_def);
            VCALL(fact->def_to_use, delete, new_def);  // 删除 def → use
            VCALL(fact->use_to_def, delete, use);       // 删除对应的 use → def
        }
        // 情况2: new_def 是某个复制对的 use (? → new_def)
        // new_def 的值变了, 依赖它的复制对不再成立
        if(VCALL(fact->use_to_def, exist, new_def)) {
            IR_var def = VCALL(fact->use_to_def, get, new_def);
            VCALL(fact->use_to_def, delete, new_def);   // 删除 use → def
            VCALL(fact->def_to_use, delete, def);        // 删除对应的 def → use
        }
    }
    //// copy_gen: x := y (y 是变量) → 生成新的复制对 x → y
    if(stmt->stmt_type == IR_ASSIGN_STMT) {
        IR_assign_stmt *assign_stmt = (IR_assign_stmt*)stmt;
        if(!assign_stmt->rs.is_const) {
            IR_var def = assign_stmt->rd, use = assign_stmt->rs.var;
            VCALL(fact->def_to_use, set, def, use);  // def → use
            VCALL(fact->use_to_def, set, use, def);   // use → def
        }
    }
}

bool CopyPropagation_transferBlock (CopyPropagation *t,
                                                 IR_block *block,
                                                 Fact_def_use *in_fact,
                                                 Fact_def_use *out_fact) {
    Fact_def_use *new_out_fact = CopyPropagation_newInitialFact(t);
    CopyPropagation_meetInto(t, in_fact, new_out_fact);
    for_list(IR_stmt_ptr, i, block->stmts) {
        IR_stmt *stmt = i->val;
        CopyPropagation_transferStmt(t, stmt, new_out_fact);
    }
    bool updated = CopyPropagation_meetInto(t, new_out_fact, out_fact);
    RDELETE(Fact_def_use, new_out_fact);
    return updated;
}

void CopyPropagation_print_result (CopyPropagation *t, IR_function *func) {
    printf("Function %s: Copy Propagation Result\n", func->func_name);
    for_list(IR_block_ptr, i, func->blocks) {
        IR_block *blk = i->val;
        printf("=================\n");
        printf("{Block%s %p}\n", blk == func->entry ? "(Entry)" :
                                 blk == func->exit ? "(Exit)" : "",
               blk);
        IR_block_print(blk, stdout);
        Fact_def_use *in_fact = VCALL(*t, getInFact, blk),
                *out_fact = VCALL(*t, getOutFact, blk);
        printf("[In(top:%d)]:  ", in_fact->is_top);
        for_map(IR_var, IR_var, j, in_fact->def_to_use)
            printf("{v%u := v%u} ", j->key, j->val);
        printf("\n");
        printf("[Out(top:%d)]: ", out_fact->is_top);
        for_map(IR_var, IR_var, j, out_fact->def_to_use)
            printf("{v%u := v%u} ", j->key, j->val);
        printf("\n");
        printf("=================\n");
    }
}

void CopyPropagation_init(CopyPropagation *t) {
    const static struct CopyPropagation_virtualTable vTable = {
            .teardown        = CopyPropagation_teardown,
            .isForward       = CopyPropagation_isForward,
            .newBoundaryFact = CopyPropagation_newBoundaryFact,
            .newInitialFact  = CopyPropagation_newInitialFact,
            .setInFact       = CopyPropagation_setInFact,
            .setOutFact      = CopyPropagation_setOutFact,
            .getInFact       = CopyPropagation_getInFact,
            .getOutFact      = CopyPropagation_getOutFact,
            .meetInto        = CopyPropagation_meetInto,
            .transferBlock   = CopyPropagation_transferBlock,
            .printResult     = CopyPropagation_print_result
    };
    t->vTable = &vTable;
    Map_IR_block_ptr_Fact_def_use_ptr_init(&t->mapInFact);
    Map_IR_block_ptr_Fact_def_use_ptr_init(&t->mapOutFact);
}

//// ============================ Optimize ============================

// 将所有use变为copy的def变量
static void block_replace_available_use_copy (CopyPropagation *t, IR_block *blk) {
    Fact_def_use *blk_in_fact = VCALL(*t, getInFact, blk);
    Fact_def_use *new_in_fact = CopyPropagation_newInitialFact(t);
    CopyPropagation_meetInto(t, blk_in_fact, new_in_fact);
    for_list(IR_stmt_ptr, i, blk->stmts) {
        IR_stmt *stmt = i->val;
        IR_use use = VCALL(*stmt, get_use_vec);
        for(int j = 0; j < use.use_cnt; j++)
            if(!use.use_vec[j].is_const) {
                IR_var use_var = use.use_vec[j].var;
                if(VCALL(new_in_fact->def_to_use, exist, use_var))
                    use.use_vec[j].var = VCALL(new_in_fact->def_to_use, get, use_var);
            }
        CopyPropagation_transferStmt(t, stmt, new_in_fact);
    }
    RDELETE(Fact_def_use, new_in_fact);
}

void CopyPropagation_replace_available_use_copy (CopyPropagation *t, IR_function *func) {
    for_list(IR_block_ptr, j, func->blocks) {
        IR_block *blk = j->val;
        block_replace_available_use_copy(t, blk);
    }
}




