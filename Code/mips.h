#ifndef MIPS_H
#define MIPS_H

#include <stdio.h>
#include "intercode.h" // 确保后端系统认识 IR 的数据结构

void generate_target_code(CodeList* intercodes, FILE* out_file);

#endif