#ifndef LEXICO_H
#define LEXICO_H

#include <stdio.h>

#define MAX_NOME_TOKEN 50
#define MAX_LEXEMA     100
#define MAX_CATEGORIA  30

/* Estrutura de token exigida pela especificação. */
typedef struct {
    char nome[MAX_NOME_TOKEN];
    char lexema[MAX_LEXEMA];
    int linha;
    int coluna;
} Token;

/* Estrutura de entrada da Tabela de Símbolos exigida pela especificação. */
typedef struct {
    char lexema[MAX_LEXEMA];
    char categoria[MAX_CATEGORIA];
    int primeiraLinha;
    int primeiraColuna;
} Simbolo;

/*
 * A especificação exige a função void AnaliseLexica(FILE *in, FILE *out),
 * que recebe apenas o arquivo de entrada (já pré-processado) e o arquivo
 * de tokens (.lex) de saída. Como o trabalho também exige a geração dos
 * arquivos .ts (Tabela de Símbolos) e .err (erros léxicos), estes são
 * informados separadamente por meio desta função, que deve ser chamada
 * ANTES de AnaliseLexica().
 */
void DefinirSaidasAuxiliares(FILE *saidaTS, FILE *saidaErr);

/*
 * Executa a análise léxica do conteúdo do arquivo 'in' (já submetido ao
 * pré-processamento), gravando em 'out' um registro por token reconhecido,
 * no formato:
 *     <nome_token, lexema> linha coluna
 * Ao final do arquivo, grava também o token especial TK_EOF, a Tabela de
 * Símbolos completa (no arquivo definido via DefinirSaidasAuxiliares) e os
 * erros léxicos encontrados (idem).
 */
void AnaliseLexica(FILE *in, FILE *out);

#endif /* LEXICO_H */
