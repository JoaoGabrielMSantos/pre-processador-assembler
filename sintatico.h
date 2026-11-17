#ifndef SINTATICO_H
#define SINTATICO_H

#include <stdio.h>
#include "lexico.h"

/*
 * Categorias geradas internamente pelo analisador sintático.
 * EOL     - término de uma linha lógica (inserido a partir da linha dos tokens).
 * TK_EOF  - final da sequência de tokens (acrescentado se ausente no .lex).
 */
#define CAT_EOL    "EOL"
#define CAT_TK_EOF "TK_EOF"

/* Rótulos dos nós da árvore sintática concreta. */
#define MAX_ROTULO_NO 64

/*
 * Nó da árvore sintática concreta. Os nós ficam em um vetor dinâmico e se
 * referenciam por índice (primeiro filho / último filho / próximo irmão),
 * o que preserva a ordem dos filhos e evita alocações individuais.
 */
typedef struct {
    char rotulo[MAX_ROTULO_NO];   /* "<nao_terminal>" ou categoria do terminal */
    char lexema[MAX_LEXEMA];      /* lexema do terminal (vazio se não aplicável) */
    int  terminal;                /* 1 = folha (terminal), 0 = não terminal     */
    int  primeiroFilho;
    int  ultimoFilho;
    int  proximoIrmao;
} NoArvore;

/*
 * Executa a análise sintática descendente recursiva.
 *
 *   tokens - arquivo .lex (aberto para leitura)
 *   arvore - arquivo .dot (aberto para escrita); recebe a árvore sintática
 *            concreta somente quando o programa é válido
 *   erros  - arquivo .sin.err (aberto para escrita)
 *
 * Retorno:  0 programa sintaticamente válido
 *           1 programa sintaticamente inválido
 *          -1 falha operacional (leitura, gravação ou memória)
 */
int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros);

#endif /* SINTATICO_H */
