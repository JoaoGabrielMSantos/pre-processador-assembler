#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"

char *ler_arquivo(FILE *fp, long *tamanho) {
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Erro: falha ao acessar o arquivo de entrada.\n");
        exit(1);
    }

    long tam = ftell(fp);
    if (tam < 0) {
        fprintf(stderr, "Erro: falha ao determinar o tamanho do arquivo.\n");
        exit(1);
    }
    rewind(fp);

    char *buffer = (char *)malloc((size_t)tam + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Erro: memória insuficiente para ler o arquivo.\n");
        exit(1);
    }

    size_t lidos = fread(buffer, 1, (size_t)tam, fp);
    buffer[lidos] = '\0';

    if (tamanho != NULL) {
        *tamanho = (long)lidos;
    }

    return buffer;
}

/*
 * Remove o comentário de uma linha, respeitando o caractere '#' quando ele está em meio de aspas duplas
 */
static void remover_comentario(char *linha) {
    int dentro_string = 0;

    for (int i = 0; linha[i] != '\0'; i++) {
        if (linha[i] == '"') {
            dentro_string = !dentro_string;
        } else if (linha[i] == '#' && !dentro_string) {
            linha[i] = '\0';
            return;
        }
    }
}

void normalizar_linha(char *linha, char *saida) {
    int dentro_string = 0;
    int j = 0;
    int ultimo_foi_espaco = 0;

    for (int i = 0; linha[i] != '\0' && j < MAX_LINHA - 1; i++) {
        char c = linha[i];

        if (c == '"') {
            dentro_string = !dentro_string;
            saida[j++] = c;
            ultimo_foi_espaco = 0;
            continue;
        }

        if (dentro_string) {
            /* Preservar caractre */
            saida[j++] = c;
            continue;
        }

        if (c == '\t') {
            c = ' ';
        }

        if (c == ' ') {
            if (!ultimo_foi_espaco) {
                saida[j++] = ' ';
                ultimo_foi_espaco = 1;
            }
        } else {
            saida[j++] = c;
            ultimo_foi_espaco = 0;
        }
    }
    saida[j] = '\0';

    /* Remove espaços no início da linha */
    int inicio = 0;
    while (saida[inicio] == ' ') {
        inicio++;
    }
    if (inicio > 0) {
        memmove(saida, saida + inicio, strlen(saida + inicio) + 1);
    }

    /* Remove espaços no final da linha */
    int fim = (int)strlen(saida) - 1;
    while (fim >= 0 && saida[fim] == ' ') {
        saida[fim] = '\0';
        fim--;
    }
}

void processar_linha(char *linha, FILE *saida) {
    remover_comentario(linha);

    char normalizada[MAX_LINHA];
    normalizar_linha(linha, normalizada);

    /* Linha vazia após remoção de comentário ou de espaços é descartada */
    if (normalizada[0] != '\0') {
        fprintf(saida, "%s\n", normalizada);
    }
}

void preprocessar(const char *conteudo, FILE *saida) {
    char linha[MAX_LINHA];
    int pos = 0;
    long i = 0;

    while (conteudo[i] != '\0') {
        char c = conteudo[i];

        if (c == '\r' || c == '\n') {
            linha[pos] = '\0';
            processar_linha(linha, saida);
            pos = 0;

            /* Trata a sequência \r\n como uma única quebra de linha */
            if (c == '\r' && conteudo[i + 1] == '\n') {
                i++;
            }
        } else {
            if (pos < MAX_LINHA - 1) {
                linha[pos++] = c;
            }
        }
        i++;
    }

    /* Processa a última linha, caso o arquivo não termine com quebra de linha */
    if (pos > 0) {
        linha[pos] = '\0';
        processar_linha(linha, saida);
    }
}

void PreProcessamento(FILE *in, FILE *out) {
    long tamanho;
    char *conteudo = ler_arquivo(in, &tamanho);
    preprocessar(conteudo, out);
    free(conteudo);
}
