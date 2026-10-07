#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"
#include "lexico.h"

/*
 * Deriva um nome de arquivo substituindo a extensão 'extAntiga' por
 * 'extNova'. Se 'base' nao terminar com 'extAntiga', a extensao nova e
 * apenas concatenada ao final. O resultado e escrito em 'destino'.
 */
static void derivarNomeArquivo(const char *base, const char *extAntiga,
                                const char *extNova, char *destino, size_t tamanho) {
    size_t lenBase = strlen(base);
    size_t lenExtAntiga = strlen(extAntiga);

    if (lenBase >= lenExtAntiga &&
        strcmp(base + (lenBase - lenExtAntiga), extAntiga) == 0) {
        size_t lenRaiz = lenBase - lenExtAntiga;
        if (lenRaiz >= tamanho) lenRaiz = tamanho - 1;
        strncpy(destino, base, lenRaiz);
        destino[lenRaiz] = '\0';
        strncat(destino, extNova, tamanho - strlen(destino) - 1);
    } else {
        strncpy(destino, base, tamanho - 1);
        destino[tamanho - 1] = '\0';
        strncat(destino, extNova, tamanho - strlen(destino) - 1);
    }
}

static FILE *abrirOuFalhar(const char *caminho, const char *modo, const char *descricao) {
    FILE *f = fopen(caminho, modo);
    if (f == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir/criar o arquivo %s '%s'.\n", descricao, caminho);
    }
    return f;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Uso: %s <arquivo_entrada.asm> <arquivo_saida.pre> <arquivo_saida.lex>\n",
                argv[0]);
        return 1;
    }

    const char *caminhoAsm = argv[1];
    const char *caminhoPre = argv[2];
    const char *caminhoLex = argv[3];

    char caminhoTs[1024];
    char caminhoErr[1024];
    derivarNomeArquivo(caminhoLex, ".lex", ".ts", caminhoTs, sizeof(caminhoTs));
    derivarNomeArquivo(caminhoLex, ".lex", ".err", caminhoErr, sizeof(caminhoErr));

    /* Etapa 1: pre-processamento */
    FILE *entradaAsm = abrirOuFalhar(caminhoAsm, "rb", "de entrada");
    if (entradaAsm == NULL) return 1;

    FILE *saidaPre = abrirOuFalhar(caminhoPre, "wb", "de pre-processamento");
    if (saidaPre == NULL) {
        fclose(entradaAsm);
        return 1;
    }

    PreProcessamento(entradaAsm, saidaPre);

    fclose(entradaAsm);
    fclose(saidaPre);

    /* Etapa 2: analise lexica (le o .pre gerado na etapa anterior) */
    FILE *entradaPre = abrirOuFalhar(caminhoPre, "rb", "pre-processado");
    if (entradaPre == NULL) return 1;

    FILE *saidaLex = abrirOuFalhar(caminhoLex, "wb", "de tokens (.lex)");
    if (saidaLex == NULL) {
        fclose(entradaPre);
        return 1;
    }

    FILE *saidaTs = abrirOuFalhar(caminhoTs, "wb", "da tabela de simbolos (.ts)");
    if (saidaTs == NULL) {
        fclose(entradaPre);
        fclose(saidaLex);
        return 1;
    }

    FILE *saidaErr = abrirOuFalhar(caminhoErr, "wb", "de erros lexicos (.err)");
    if (saidaErr == NULL) {
        fclose(entradaPre);
        fclose(saidaLex);
        fclose(saidaTs);
        return 1;
    }

    DefinirSaidasAuxiliares(saidaTs, saidaErr);
    AnaliseLexica(entradaPre, saidaLex);

    fclose(entradaPre);
    fclose(saidaLex);
    fclose(saidaTs);
    fclose(saidaErr);

    printf("Pre-processamento e analise lexica concluidos com sucesso.\n");
    printf("  Pre-processado: %s\n", caminhoPre);
    printf("  Tokens:         %s\n", caminhoLex);
    printf("  Tabela simb.:   %s\n", caminhoTs);
    printf("  Erros lexicos:  %s\n", caminhoErr);

    return 0;
}
