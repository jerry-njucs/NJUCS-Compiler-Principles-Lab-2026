## IMPORTANT INFORMATION

当参数是结构体或者数组时，ARG参数是地址，也就是传引用

ARG传入参数的顺序和PARAM声明参数的顺序相反
caller:
    ARG a1
    ARG a2
    ARG a3
    t1 := CALL func

func:
    PARAM p3
    PARAM P2
    PARAM p1

READ&WRITE命令和控制台交互，读写INT
注意中间代码没有作用域，所以避免变量重名

别忘了在符号表里面写read和write函数