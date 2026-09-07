#include <stdio.h>
#include <stdlib.h>
#include "preprocessador.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo_entrada.asm> <arquivo_saida.pre>\n", argv[0]);
        return 1;
    }

    FILE *entrada = fopen(argv[1], "rb");
    if (entrada == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de entrada '%s'.\n", argv[1]);
        return 1;
    }

    FILE *saida = fopen(argv[2], "wb");
    if (saida == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar o arquivo de saida '%s'.\n", argv[2]);
        fclose(entrada);
        return 1;
    }

    long tamanho = 0;
    char *conteudo = ler_arquivo(entrada, &tamanho);
    fclose(entrada);

    preprocessar(conteudo, saida);

    free(conteudo);
    fclose(saida);

    printf("Pre-processamento concluido com sucesso. Arquivo gerado: %s\n", argv[2]);
    return 0;
}
