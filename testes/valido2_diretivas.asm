# Caso valido 2 - todas as diretivas de dados e operandos
.data
numero:   .word 10
vetor:    .word 10, 20, -30, 0xFF, 0X1a
pequeno:  .byte 65, 0x41
valor:    .half 1000, -2
mensagem: .asciiz "Resultado: "
texto:    .ascii "A, B > \"C\" # nao e comentario\n"
buffer:   .space 100
.align 2
isolado:
          .word 7
.globl numero

.text
main:
   la $a0, mensagem
   li $v0, 4
   syscall
   li $v0, 10
   syscall
