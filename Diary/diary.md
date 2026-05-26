.l文件中的/**/注释不会被识别，会变成报错

词法分析 make lexical

语法分析 make parser

/*
[0-9]+[_a-zA-Z][_0-9a-zA-Z]* {
    lexicalError += 1;
    printf("Error type A at Line %d: Illegal identifier \"%s\"\n", yylineno, yytext);
}
*/


抑制同行报错

python3 test.py -p /home/jerry/Desktop/Lab/Code/parser -g 3


jerry@ubuntu:~/Desktop/Lab/fulltest$ python3 test.py -p /home/jerry/Desktop/Lab/Code/parser -g 3
[base/2023/A-1.cmm] AC
[base/2023/A-10.cmm] AC
[base/2023/A-2.cmm] AC
[base/2023/A-3.cmm] AC
[base/2023/A-4.cmm] AC
[base/2023/A-5.cmm] AC
[base/2023/A-6.cmm] AC
[base/2023/A-7.cmm] AC
[base/2023/A-8.cmm] AC
[base/2023/A-9.cmm] AC
[base/2023/B-1.cmm] WA: got ['B5'], expect ['B5', 'B7', 'B11', 'B24']
[base/2023/B-2.cmm] WA: got ['B5'], expect ['B5', 'B18', 'B30', 'B32']
[base/2023/C-1.cmm] AC
[base/2023/C-2.cmm] AC
[base/2020/A_1.cmm] AC
[base/2020/A_2.cmm] AC
[base/2020/A_3.cmm] AC
[base/2020/A_4.cmm] AC
[base/2020/A_5.cmm] AC
[base/2020/A_6.cmm] AC
[base/2020/A_7.cmm] AC
[base/2020/A_8.cmm] AC
[base/2020/A_9.cmm] AC
[base/2020/B_1.cmm] WA: got ['B4'], expect ['B3', 'B16', 'A20', 'B24']
[base/2020/B_2.cmm] WA: got ['B5'], expect ['B5', 'B18', 'B30', 'B31']
[base/2020/C_1.cmm] AC
[base/2020/C_2.cmm] AC
[base/2021/A_1.cmm] AC
[base/2021/A_2.cmm] AC
[base/2021/A_3.cmm] AC
[base/2021/A_4.cmm] AC
[base/2021/A_5.cmm] AC
[base/2021/A_6.cmm] AC
[base/2021/A_7.cmm] AC
[base/2021/A_8.cmm] AC
[base/2021/A_9.cmm] AC
[base/2021/B_1.cmm] WA: got ['B4'], expect ['B4', 'B14']
[base/2021/B_2.cmm] WA: got ['B7'], expect ['B7', 'B19', 'B23', 'B39']
[base/2021/C_1.cmm] AC
[base/2021/C_2.cmm] AC
[base/2024/A-1.cmm] AC
[base/2024/A-10.cmm] AC
[base/2024/A-2.cmm] AC
[base/2024/A-3.cmm] AC
[base/2024/A-4.cmm] AC
[base/2024/A-5.cmm] AC
[base/2024/A-6.cmm] AC
[base/2024/A-7.cmm] AC
[base/2024/A-8.cmm] AC
[base/2024/A-9.cmm] AC
[base/2024/B-1.cmm] WA: got ['B2'], expect ['B2', 'B5', 'B8', 'B11', 'B15']
[base/2024/B-2.cmm] WA: got ['B6'], expect ['B6', 'B9', 'B13', 'B15', 'B16', 'B18']
[base/2024/C-1.cmm] AC
[base/2024/C-2.cmm] AC
[extend/3/2023/D-3.cmm] AC
[extend/3/2023/E3-1.cmm] AC
[extend/3/2023/E3-2.cmm] AC
[extend/3/2020/E3_1.cmm] AC
[extend/3/2020/E3_2.cmm] WA: got ['B10'], expect ['B9', 'A21']
[extend/3/2021/D_3.cmm] AC
[extend/3/2021/E_3_1.cmm] WA: got ['B49'], expect ['B48', 'A60']
[extend/3/2021/E_3_2.cmm] AC
[extend/3/2024/D-3.cmm] AC
[extend/3/2024/E3-1.cmm] AC
[extend/3/2024/E3-2.cmm] AC
[extend/not1/2023/D-1.cmm] WA: got ['B2'], expect ['A2', 'A4', 'A7', 'A10']
[extend/not1/2021/D_1.cmm] WA: got ['B2'], expect ['A2', 'A3', 'A4']
[extend/not1/2024/D-1.cmm] WA: got ['B2'], expect ['A2', 'A3', 'A4', 'A5']
[extend/not2/2023/D-2.cmm] WA: got ['B3'], expect ['A3', 'A4', 'A5', 'A6']
[extend/not2/2021/D_2.cmm] WA: got ['B2'], expect ['A2', 'A3', 'A4', 'A5']
[extend/not2/2024/D-2.cmm] WA: got ['B2'], expect ['A2', 'A3']
AC: 55, WA: 16, total: 71