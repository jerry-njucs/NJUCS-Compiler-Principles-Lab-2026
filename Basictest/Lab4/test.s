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
  addi $sp, $sp, -400
  # [DEC] 
  # [DEC] 
  li $t0, 0
  move $t1, $t0
  # [DEC] 
  sw $t0, -56($fp) # [Block End] 洗盘 t1
  sw $t1, -60($fp) # [Block End] 洗盘 i
label1:
  lw $t0, -60($fp) # [Reload] 装载 i
  li $t1, 5
  blt $t0, $t1, label2
  j label3
label2:
  addi $t0, $fp, -80
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  sw $t0, -84($fp) # [Block End] 洗盘 t3
  sw $t2, -88($fp) # [Block End] 洗盘 t4
  sw $t4, -92($fp) # [Block End] 洗盘 t5
  sw $t5, -96($fp) # [Block End] 洗盘 t6
  jal read
  move $t0, $v0
  lw $t1, -96($fp) # [Reload] 装载 t6
  sw $t0, 0($t1)
  lw $t2, -60($fp) # [Reload] 装载 i
  move $t3, $t2
  li $t4, 1
  add $t5, $t3, $t4
  move $t2, $t5
  sw $t0, -100($fp) # [Block End] 洗盘 t7
  sw $t2, -60($fp) # [Block End] 洗盘 i
  sw $t3, -104($fp) # [Block End] 洗盘 t9
  sw $t4, -108($fp) # [Block End] 洗盘 t10
  sw $t5, -112($fp) # [Block End] 洗盘 t8
  j label1
label3:
  li $t0, 0
  move $t1, $t0
  sw $t0, -116($fp) # [Block End] 洗盘 t11
  sw $t1, -60($fp) # [Block End] 洗盘 i
label4:
  lw $t0, -60($fp) # [Reload] 装载 i
  li $t1, 4
  blt $t0, $t1, label5
  j label6
label5:
  lw $t0, -60($fp) # [Reload] 装载 i
  move $t1, $t0
  li $t2, 1
  add $t3, $t1, $t2
  move $t4, $t3
  sw $t1, -120($fp) # [Block End] 洗盘 t13
  sw $t2, -124($fp) # [Block End] 洗盘 t14
  sw $t3, -128($fp) # [Block End] 洗盘 t12
  sw $t4, -132($fp) # [Block End] 洗盘 j
label7:
  lw $t0, -132($fp) # [Reload] 装载 j
  li $t1, 5
  blt $t0, $t1, label8
  j label9
label8:
  addi $t0, $fp, -80
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  addi $t7, $fp, -80
  lw $t8, -132($fp) # [Reload] 装载 j
  move $t9, $t8
  mul $s0, $t9, $t3
  add $s1, $t7, $s0
  lw $s2, 0($s1)
  sw $t0, -136($fp) # [Block End] 洗盘 t17
  sw $t2, -140($fp) # [Block End] 洗盘 t18
  sw $t4, -144($fp) # [Block End] 洗盘 t19
  sw $t5, -148($fp) # [Block End] 洗盘 t20
  sw $t6, -152($fp) # [Block End] 洗盘 t15
  sw $t7, -156($fp) # [Block End] 洗盘 t23
  sw $t9, -160($fp) # [Block End] 洗盘 t24
  sw $s0, -164($fp) # [Block End] 洗盘 t25
  sw $s1, -168($fp) # [Block End] 洗盘 t26
  sw $s2, -172($fp) # [Block End] 洗盘 t21
  bgt $t6, $s2, label10
  j label11
label10:
  addi $t0, $fp, -80
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  move $t7, $t6
  addi $t8, $fp, -80
  move $t9, $t1
  mul $s0, $t9, $t3
  add $s1, $t8, $s0
  addi $s2, $fp, -80
  lw $s3, -132($fp) # [Reload] 装载 j
  move $s4, $s3
  mul $s5, $s4, $t3
  add $s6, $s2, $s5
  lw $s7, 0($s6)
  sw $s7, 0($s1)
  sw $t0, -176($fp) # [Spill] 寄存器满，强制写回 t29
  addi $t0, $fp, -80
  move $t1, $s3
  sw $t2, -180($fp) # [Spill] 寄存器满，强制写回 t30
  mul $t2, $t1, $t3
  add $t3, $t0, $t2
  sw $t4, -184($fp) # [Spill] 寄存器满，强制写回 t31
  move $t4, $t7
  sw $t4, 0($t3)
  sw $t0, -236($fp) # [Block End] 洗盘 t45
  sw $t1, -240($fp) # [Block End] 洗盘 t46
  sw $t2, -244($fp) # [Block End] 洗盘 t47
  sw $t3, -248($fp) # [Block End] 洗盘 t48
  sw $t4, -252($fp) # [Block End] 洗盘 t49
  sw $t5, -188($fp) # [Block End] 洗盘 t32
  sw $t6, -192($fp) # [Block End] 洗盘 t27
  sw $t7, -196($fp) # [Block End] 洗盘 t
  sw $t8, -200($fp) # [Block End] 洗盘 t34
  sw $t9, -204($fp) # [Block End] 洗盘 t35
  sw $s0, -208($fp) # [Block End] 洗盘 t36
  sw $s1, -212($fp) # [Block End] 洗盘 t37
  sw $s2, -216($fp) # [Block End] 洗盘 t40
  sw $s4, -220($fp) # [Block End] 洗盘 t41
  sw $s5, -224($fp) # [Block End] 洗盘 t42
  sw $s6, -228($fp) # [Block End] 洗盘 t43
  sw $s7, -232($fp) # [Block End] 洗盘 t38
label11:
  lw $t0, -132($fp) # [Reload] 装载 j
  move $t1, $t0
  li $t2, 1
  add $t3, $t1, $t2
  move $t0, $t3
  sw $t0, -132($fp) # [Block End] 洗盘 j
  sw $t1, -256($fp) # [Block End] 洗盘 t51
  sw $t2, -260($fp) # [Block End] 洗盘 t52
  sw $t3, -264($fp) # [Block End] 洗盘 t50
  j label7
label9:
  lw $t0, -60($fp) # [Reload] 装载 i
  move $t1, $t0
  li $t2, 1
  add $t3, $t1, $t2
  move $t0, $t3
  sw $t0, -60($fp) # [Block End] 洗盘 i
  sw $t1, -268($fp) # [Block End] 洗盘 t54
  sw $t2, -272($fp) # [Block End] 洗盘 t55
  sw $t3, -276($fp) # [Block End] 洗盘 t53
  j label4
label6:
  li $t0, 0
  move $t1, $t0
  sw $t0, -280($fp) # [Block End] 洗盘 t56
  sw $t1, -60($fp) # [Block End] 洗盘 i
label12:
  lw $t0, -60($fp) # [Reload] 装载 i
  li $t1, 5
  blt $t0, $t1, label13
  j label14
label13:
  addi $t0, $fp, -80
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  move $a0, $t6
  sw $t0, -284($fp) # [Block End] 洗盘 t59
  sw $t2, -288($fp) # [Block End] 洗盘 t60
  sw $t4, -292($fp) # [Block End] 洗盘 t61
  sw $t5, -296($fp) # [Block End] 洗盘 t62
  sw $t6, -300($fp) # [Block End] 洗盘 t57
  jal write
  lw $t0, -60($fp) # [Reload] 装载 i
  move $t1, $t0
  li $t2, 1
  add $t3, $t1, $t2
  move $t0, $t3
  sw $t0, -60($fp) # [Block End] 洗盘 i
  sw $t1, -304($fp) # [Block End] 洗盘 t65
  sw $t2, -308($fp) # [Block End] 洗盘 t66
  sw $t3, -312($fp) # [Block End] 洗盘 t64
  j label12
label14:
  lw $t0, -80($fp) # [Reload] 装载 a
  move $t1, $t0
  move $t2, $t1
  move $t3, $t0
  move $t4, $t3
  li $t5, 0
  move $t6, $t5
  sw $t1, -316($fp) # [Block End] 洗盘 t67
  sw $t2, -12($fp) # [Block End] 洗盘 b
  sw $t3, -320($fp) # [Block End] 洗盘 t68
  sw $t4, -52($fp) # [Block End] 洗盘 c
  sw $t5, -324($fp) # [Block End] 洗盘 t69
  sw $t6, -60($fp) # [Block End] 洗盘 i
label15:
  lw $t0, -60($fp) # [Reload] 装载 i
  li $t1, 5
  blt $t0, $t1, label16
  j label17
label16:
  lw $t0, -60($fp) # [Reload] 装载 i
  li $t1, 3
  blt $t0, $t1, label18
  j label19
label18:
  addi $t0, $fp, -12
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  move $a0, $t6
  sw $t0, -328($fp) # [Block End] 洗盘 t72
  sw $t2, -332($fp) # [Block End] 洗盘 t73
  sw $t4, -336($fp) # [Block End] 洗盘 t74
  sw $t5, -340($fp) # [Block End] 洗盘 t75
  sw $t6, -344($fp) # [Block End] 洗盘 t70
  jal write
  addi $t0, $fp, -52
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  move $a0, $t6
  sw $t0, -348($fp) # [Block End] 洗盘 t79
  sw $t2, -352($fp) # [Block End] 洗盘 t80
  sw $t4, -356($fp) # [Block End] 洗盘 t81
  sw $t5, -360($fp) # [Block End] 洗盘 t82
  sw $t6, -364($fp) # [Block End] 洗盘 t77
  jal write
  j label20
label19:
  addi $t0, $fp, -52
  lw $t1, -60($fp) # [Reload] 装载 i
  move $t2, $t1
  li $t3, 4
  mul $t4, $t2, $t3
  add $t5, $t0, $t4
  lw $t6, 0($t5)
  move $a0, $t6
  sw $t0, -368($fp) # [Block End] 洗盘 t86
  sw $t2, -372($fp) # [Block End] 洗盘 t87
  sw $t4, -376($fp) # [Block End] 洗盘 t88
  sw $t5, -380($fp) # [Block End] 洗盘 t89
  sw $t6, -384($fp) # [Block End] 洗盘 t84
  jal write
label20:
  lw $t0, -60($fp) # [Reload] 装载 i
  move $t1, $t0
  li $t2, 1
  add $t3, $t1, $t2
  move $t0, $t3
  sw $t0, -60($fp) # [Block End] 洗盘 i
  sw $t1, -388($fp) # [Block End] 洗盘 t92
  sw $t2, -392($fp) # [Block End] 洗盘 t93
  sw $t3, -396($fp) # [Block End] 洗盘 t91
  j label15
label17:
  li $t0, 0
  move $v0, $t0
  sw $t0, -400($fp) # [Block End] 洗盘 t94
  move $sp, $fp
  lw $fp, 0($sp)
  lw $ra, 4($sp)
  addi $sp, $sp, 8
  jr $ra
