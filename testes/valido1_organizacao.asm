# Caso valido 1 - organizacao do programa
# Inicia diretamente por .text, alterna secoes, tem secao vazia,
# linhas vazias, comentarios, rotulos isolados e .globl nas duas secoes.

.text
.globl main
main:
   li $t0, 10          # rotulo isolado na linha anterior

loop: addi $t0, $t0, -1
   bne $t0, $zero, loop
fim:

.data
.globl valor
valor: .word 5

.data
.text
   la $a0, valor
   lw $t1, valor
   j inexistente       # rotulo inexistente: nao e erro sintatico
   syscall