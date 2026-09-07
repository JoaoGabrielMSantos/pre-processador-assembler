#ifndef PREPROCESSADOR_H
#define PREPROCESSADOR_H

#include <stdio.h>

/* Tamanho máximo de uma linha do arquivo de entrada, incluindo terminador */
#define MAX_LINHA 4096

/*
 * Lê todo o conteúdo de um arquivo já aberto para um buffer
 * alocado dinamicamente. O buffer retornado é terminado em '\0' e deve ser
 * liberado pelo free().
 */
char *ler_arquivo(FILE *fp, long *tamanho);

/*
 * Executa todas as etapas de pré-processamento sobre o conteúdo de entrada
 * e grava o resultado no arquivo de saída.
 */
void preprocessar(const char *conteudo, FILE *saida);

/*
 * Aplica as regras de pré-processamento, comentário é removido em memória.
 */
void processar_linha(char *linha, FILE *saida);

/*
 * Substitui tabulações por espaço, colapsa espaços consecutivos e remove
 * espaços nas extremidades da linha, preservando integralmente o conteúdo
 * de strings delimitadas por aspas duplas.
 *
 * linha - linha de entrada (já sem comentário)
 * saida - buffer onde a linha normalizada é escrita (deve ter tamanho
 *         suficiente, ex.: MAX_LINHA)
 */
void normalizar_linha(char *linha, char *saida);

#endif /* PREPROCESSADOR_H */
