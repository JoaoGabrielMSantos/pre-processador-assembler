#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"
#include "lexico.h"
#include "sintatico.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* Códigos de encerramento exigidos pela especificação */
#define SAIDA_VALIDO      0  /* execução concluída e programa sintaticamente válido */
#define SAIDA_REJEITADO   1  /* rejeição por erros léxicos ou sintáticos            */
#define SAIDA_FALHA       2  /* falha operacional ou uso incorreto                   */

#define MAX_CAMINHO 1024

#define MSG_SEM_SINTATICA "ANÁLISE SINTÁTICA NÃO EXECUTADA: EXISTEM ERROS LÉXICOS."

/* Arquivos manipulados por uma execução */
typedef struct {
    FILE *asmIn;    /* <entrada>.asm   */
    FILE *pre;      /* <prefixo>.pre   */
    FILE *lex;      /* <prefixo>.lex   */
    FILE *ts;       /* <prefixo>.ts    */
    FILE *lexErr;   /* <prefixo>.lex.err */
    FILE *sinErr;   /* <prefixo>.sin.err */
    FILE *dot;      /* <prefixo>.dot   */
} Arquivos;

/* Monta "<prefixo><extensao>"; retorna 0 se o nome não couber no buffer. */
static int montarNome(char *destino, const char *prefixo, const char *extensao) {
    int n = snprintf(destino, MAX_CAMINHO, "%s%s", prefixo, extensao);
    return n > 0 && n < MAX_CAMINHO;
}

static FILE *abrir(const char *caminho, const char *modo, const char *descricao) {
    FILE *f = fopen(caminho, modo);
    if (f == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo %s '%s'.\n", descricao, caminho);
    }
    return f;
}

/* Fecha todos os arquivos abertos; retorna 0 se algum fechamento falhar. */
static int fecharTodos(Arquivos *a) {
    FILE **lista[] = {&a->asmIn, &a->pre, &a->lex, &a->ts, &a->lexErr, &a->sinErr, &a->dot};
    int ok = 1;
    for (size_t i = 0; i < sizeof(lista) / sizeof(lista[0]); i++) {
        if (*lista[i] != NULL) {
            if (fclose(*lista[i]) != 0) ok = 0;
            *lista[i] = NULL;
        }
    }
    if (!ok) fprintf(stderr, "Erro: falha ao gravar/fechar um dos arquivos de saida.\n");
    return ok;
}

static int falhaOperacional(Arquivos *a, const char *mensagem) {
    if (mensagem != NULL) fprintf(stderr, "Erro: %s\n", mensagem);
    fecharTodos(a);
    return SAIDA_FALHA;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Arquivos arq = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    if (argc != 3 || argv[1][0] == '\0' || argv[2][0] == '\0') {
        fprintf(stderr, "Uso: %s <arquivo_entrada.asm> <prefixo_saida>\n", argc > 0 ? argv[0] : "main.exe");
        fprintf(stderr, "Exemplo: %s teste.asm resultado\n", argc > 0 ? argv[0] : "main.exe");
        return SAIDA_FALHA;
    }

    const char *caminhoAsm = argv[1];
    const char *prefixo = argv[2];

    char nPre[MAX_CAMINHO], nLex[MAX_CAMINHO], nTs[MAX_CAMINHO];
    char nLexErr[MAX_CAMINHO], nSinErr[MAX_CAMINHO], nDot[MAX_CAMINHO];

    if (!montarNome(nPre, prefixo, ".pre") || !montarNome(nLex, prefixo, ".lex") ||
        !montarNome(nTs, prefixo, ".ts") || !montarNome(nLexErr, prefixo, ".lex.err") ||
        !montarNome(nSinErr, prefixo, ".sin.err") || !montarNome(nDot, prefixo, ".dot")) {
        fprintf(stderr, "Erro: prefixo de saida muito longo.\n");
        return SAIDA_FALHA;
    }

    /* Abre a entrada e (re)cria todas as saídas, descartando resultados antigos */
    if ((arq.asmIn  = abrir(caminhoAsm, "rb", "de entrada")) == NULL ||
        (arq.pre    = abrir(nPre, "wb+", "de pre-processamento (.pre)")) == NULL ||
        (arq.lex    = abrir(nLex, "wb+", "de tokens (.lex)")) == NULL ||
        (arq.ts     = abrir(nTs, "wb", "da tabela de simbolos (.ts)")) == NULL ||
        (arq.lexErr = abrir(nLexErr, "wb", "de erros lexicos (.lex.err)")) == NULL ||
        (arq.sinErr = abrir(nSinErr, "wb", "de erros sintaticos (.sin.err)")) == NULL ||
        (arq.dot    = abrir(nDot, "wb", "da arvore sintatica (.dot)")) == NULL) {
        return falhaOperacional(&arq, NULL);
    }

    /* (1) Pré-processamento */
    PreProcessamento(arq.asmIn, arq.pre);
    if (fflush(arq.pre) != 0 || ferror(arq.pre)) {
        return falhaOperacional(&arq, "falha ao gravar o arquivo pre-processado.");
    }

    /* (2) Análise léxica sobre o .pre, posicionado no início */
    rewind(arq.pre);
    DefinirSaidasAuxiliares(arq.ts, arq.lexErr);
    AnaliseLexica(arq.pre, arq.lex);
    if (fflush(arq.lex) != 0 || ferror(arq.lex) || ferror(arq.ts) || ferror(arq.lexErr)) {
        return falhaOperacional(&arq, "falha ao gravar as saidas da analise lexica.");
    }

    int errosLexicos = QuantidadeErrosLexicos();
    int codigo;

    if (errosLexicos > 0) {
        /* (3) não executada: .dot permanece vazio e .sin.err registra o motivo */
        fprintf(arq.sinErr, "%s\n", MSG_SEM_SINTATICA);
        printf("%s\n", MSG_SEM_SINTATICA);
        printf("Erros lexicos: %d (detalhes em %s)\n", errosLexicos, nLexErr);
        codigo = SAIDA_REJEITADO;
    } else {
        /* (3) Análise sintática sobre o .lex, posicionado no início */
        rewind(arq.lex);
        int r = AnaliseSintatica(arq.lex, arq.dot, arq.sinErr);
        if (r < 0) {
            return falhaOperacional(&arq, "falha operacional durante a analise sintatica.");
        }
        if (r == 0) {
            printf("PROGRAMA SINTATICAMENTE VÁLIDO\n");
            codigo = SAIDA_VALIDO;
        } else {
            printf("PROGRAMA SINTATICAMENTE INVÁLIDO\n");
            printf("Diagnosticos em %s\n", nSinErr);
            codigo = SAIDA_REJEITADO;
        }
    }

    if (!fecharTodos(&arq)) return SAIDA_FALHA;
    return codigo;
}
