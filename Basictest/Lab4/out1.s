.data
_prompt: .asciiz "Enter an integer:"
_ret: .asciiz "\n"
.globl main

.text
read:
  li $v0, 4
  la $a0, _prompt
  syscall
  li $v0, 5
  syscall
  jr $ra

write:
  li $v0, 1
  syscall
  li $v0, 4
  la $a0, _ret
  syscall
  move $v0, $0
  jr $ra


main:
  addi $sp, $sp, -8
  sw $ra, 4($sp)
  sw $fp, 0($sp)
  move $fp, $sp
  addi $sp, $sp, -76
  li $t0, 0
  move $t1, $t0
  li $t2, 1
  move $t3, $t2
  li $t4, 0
  move $t5, $t4
  sw $t0, -4($fp) # [Block End] 洗盘 t1
  sw $t1, -8($fp) # [Block End] 洗盘 a
  sw $t2, -12($fp) # [Block End] 洗盘 t2
  sw $t3, -16($fp) # [Block End] 洗盘 b
  sw $t4, -20($fp) # [Block End] 洗盘 t3
  sw $t5, -24($fp) # [Block End] 洗盘 i
  jal read
  move $t0, $v0
  move $t1, $t0
  sw $t0, -28($fp) # [Block End] 洗盘 t4
  sw $t1, -32($fp) # [Block End] 洗盘 n
label1:
  lw $t0, -24($fp) # [Reload] 装载 i
  lw $t1, -32($fp) # [Reload] 装载 n
  blt $t0, $t1, label2
  j label3
label2:
  lw $t0, -8($fp) # [Reload] 装载 a
  move $t1, $t0
  lw $t2, -16($fp) # [Reload] 装载 b
  move $t3, $t2
  add $t4, $t1, $t3
  move $t5, $t4
  move $t6, $t2
  move $a0, $t6
  sw $t1, -36($fp) # [Block End] 洗盘 t6
  sw $t3, -40($fp) # [Block End] 洗盘 t7
  sw $t4, -44($fp) # [Block End] 洗盘 t5
  sw $t5, -48($fp) # [Block End] 洗盘 c
  sw $t6, -52($fp) # [Block End] 洗盘 t8
  jal write
  lw $t0, -16($fp) # [Reload] 装载 b
  move $t1, $t0
  move $t2, $t1
  lw $t3, -48($fp) # [Reload] 装载 c
  move $t4, $t3
  move $t0, $t4
  lw $t5, -24($fp) # [Reload] 装载 i
  move $t6, $t5
  li $t7, 1
  add $t8, $t6, $t7
  move $t5, $t8
  sw $t0, -16($fp) # [Block End] 洗盘 b
  sw $t1, -56($fp) # [Block End] 洗盘 t10
  sw $t2, -8($fp) # [Block End] 洗盘 a
  sw $t4, -60($fp) # [Block End] 洗盘 t11
  sw $t5, -24($fp) # [Block End] 洗盘 i
  sw $t6, -64($fp) # [Block End] 洗盘 t13
  sw $t7, -68($fp) # [Block End] 洗盘 t14
  sw $t8, -72($fp) # [Block End] 洗盘 t12
  j label1
label3:
  li $t0, 0
  move $v0, $t0
  sw $t0, -76($fp) # [Block End] 洗盘 t15
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addi $sp, $sp, 8
  jr $ra
