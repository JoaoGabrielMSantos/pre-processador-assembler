#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexico.h"

/* ========================================================================
 * Tabelas de palavras reservadas (diretivas, instruções e registradores)
 * ======================================================================== */

typedef struct {
    const char *lexema;
    const char *token;
} ParReservado;

static const ParReservado TAB_DIRETIVAS[] = {
    {".data",   "DIR_DATA"},
    {".text",   "DIR_TEXT"},
    {".word",   "DIR_WORD"},
    {".half",   "DIR_HALF"},
    {".byte",   "DIR_BYTE"},
    {".space",  "DIR_SPACE"},
    {".ascii",  "DIR_ASCII"},
    {".asciiz", "DIR_ASCIIZ"},
    {".globl",  "DIR_GLOBL"},
    {".align",  "DIR_ALIGN"}
};
#define N_DIRETIVAS (int)(sizeof(TAB_DIRETIVAS) / sizeof(TAB_DIRETIVAS[0]))

static const ParReservado TAB_INSTRUCOES[] = {
    {"li",      "INS_LI"},
    {"la",      "INS_LA"},
    {"move",    "INS_MOVE"},
    {"lw",      "INS_LW"},
    {"sw",      "INS_SW"},
    {"lb",      "INS_LB"},
    {"sb",      "INS_SB"},
    {"lh",      "INS_LH"},
    {"sh",      "INS_SH"},
    {"add",     "INS_ADD"},
    {"addi",    "INS_ADDI"},
    {"addu",    "INS_ADDU"},
    {"sub",     "INS_SUB"},
    {"subu",    "INS_SUBU"},
    {"mul",     "INS_MUL"},
    {"div",     "INS_DIV"},
    {"mflo",    "INS_MFLO"},
    {"mfhi",    "INS_MFHI"},
    {"and",     "INS_AND"},
    {"or",      "INS_OR"},
    {"xor",     "INS_XOR"},
    {"nor",     "INS_NOR"},
    {"sll",     "INS_SLL"},
    {"srl",     "INS_SRL"},
    {"slt",     "INS_SLT"},
    {"beq",     "INS_BEQ"},
    {"bne",     "INS_BNE"},
    {"blt",     "INS_BLT"},
    {"ble",     "INS_BLE"},
    {"bgt",     "INS_BGT"},
    {"bge",     "INS_BGE"},
    {"j",       "INS_J"},
    {"jal",     "INS_JAL"},
    {"jr",      "INS_JR"},
    {"syscall", "INS_SYSCALL"},
    {"nop",     "INS_NOP"}
};
#define N_INSTRUCOES (int)(sizeof(TAB_INSTRUCOES) / sizeof(TAB_INSTRUCOES[0]))

/* Registradores nomeados (formas numéricas $0..$31 são tratadas à parte) */
static const ParReservado TAB_REGISTRADORES[] = {
    {"$zero", "REG"},
    {"$v0", "REG"}, {"$v1", "REG"},
    {"$a0", "REG"}, {"$a1", "REG"}, {"$a2", "REG"}, {"$a3", "REG"},
    {"$t0", "REG"}, {"$t1", "REG"}, {"$t2", "REG"}, {"$t3", "REG"}, {"$t4", "REG"},
    {"$t5", "REG"}, {"$t6", "REG"}, {"$t7", "REG"}, {"$t8", "REG"}, {"$t9", "REG"},
    {"$s0", "REG"}, {"$s1", "REG"}, {"$s2", "REG"}, {"$s3", "REG"},
    {"$s4", "REG"}, {"$s5", "REG"}, {"$s6", "REG"}, {"$s7", "REG"},
    {"$k0", "REG"}, {"$k1", "REG"},
    {"$gp", "REG"}, {"$sp", "REG"}, {"$fp", "REG"}, {"$ra", "REG"}
};
#define N_REGISTRADORES (int)(sizeof(TAB_REGISTRADORES) / sizeof(TAB_REGISTRADORES[0]))

static void paraMinusculas(const char *origem, char *destino) {
    int i;
    for (i = 0; origem[i] != '\0'; i++) {
        destino[i] = (char)tolower((unsigned char)origem[i]);
    }
    destino[i] = '\0';
}

static const char *buscarToken(const ParReservado *tabela, int n, const char *chaveMin) {
    for (int i = 0; i < n; i++) {
        if (strcmp(tabela[i].lexema, chaveMin) == 0) {
            return tabela[i].token;
        }
    }
    return NULL;
}

/* ========================================================================
 * Tabela de símbolos
 * ======================================================================== */

static Simbolo *g_tabela = NULL;
static int g_tabelaQtd = 0;
static int g_tabelaCap = 0;

static void tabelaReset(void) {
    free(g_tabela);
    g_tabela = NULL;
    g_tabelaQtd = 0;
    g_tabelaCap = 0;
}

static int tabelaBuscar(const char *lexema) {
    for (int i = 0; i < g_tabelaQtd; i++) {
        if (strcmp(g_tabela[i].lexema, lexema) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Insere um novo símbolo caso o lexema ainda não exista. Caso já exista e
 * sua posição esteja zerada (entrada reservada ainda não utilizada), essa
 * posição é atualizada para a primeira ocorrência real encontrada no
 * programa. Retorna o índice do símbolo.
 */
static int tabelaInserirOuAtualizar(const char *lexema, const char *categoria, int linha, int coluna) {
    int idx = tabelaBuscar(lexema);

    if (idx >= 0) {
        if (g_tabela[idx].primeiraLinha == 0 && g_tabela[idx].primeiraColuna == 0 && linha != 0) {
            g_tabela[idx].primeiraLinha = linha;
            g_tabela[idx].primeiraColuna = coluna;
        }
        return idx;
    }

    if (g_tabelaQtd >= g_tabelaCap) {
        int novaCap = (g_tabelaCap == 0) ? 64 : g_tabelaCap * 2;
        Simbolo *novaTabela = (Simbolo *)realloc(g_tabela, (size_t)novaCap * sizeof(Simbolo));
        if (novaTabela == NULL) {
            fprintf(stderr, "Erro: memoria insuficiente para a tabela de simbolos.\n");
            exit(1);
        }
        g_tabela = novaTabela;
        g_tabelaCap = novaCap;
    }

    Simbolo *s = &g_tabela[g_tabelaQtd];
    strncpy(s->lexema, lexema, MAX_LEXEMA - 1);
    s->lexema[MAX_LEXEMA - 1] = '\0';
    strncpy(s->categoria, categoria, MAX_CATEGORIA - 1);
    s->categoria[MAX_CATEGORIA - 1] = '\0';
    s->primeiraLinha = linha;
    s->primeiraColuna = coluna;

    return g_tabelaQtd++;
}

static void tabelaInicializarReservados(void) {
    for (int i = 0; i < N_DIRETIVAS; i++) {
        tabelaInserirOuAtualizar(TAB_DIRETIVAS[i].lexema, "diretiva", 0, 0);
    }
    for (int i = 0; i < N_INSTRUCOES; i++) {
        tabelaInserirOuAtualizar(TAB_INSTRUCOES[i].lexema, "instrução", 0, 0);
    }
    for (int i = 0; i < N_REGISTRADORES; i++) {
        tabelaInserirOuAtualizar(TAB_REGISTRADORES[i].lexema, "registrador", 0, 0);
    }
}

/* ========================================================================
 * Leitura de caracteres com controle de linha/coluna (lookahead de 1)
 * ======================================================================== */

static FILE *g_in;
static int g_prox;
static int g_proxLinha;
static int g_proxColuna;

static void leituraIniciar(FILE *in) {
    g_in = in;
    g_proxLinha = 1;
    g_proxColuna = 1;
    g_prox = fgetc(g_in);
}

#define espiar() (g_prox)

/* Consome o caractere atualmente "espiado", devolvendo-o e, se solicitado,
 * sua posição (linha/coluna). Já deixa pronto o próximo caractere. */
static int consumir(int *linha, int *coluna) {
    int c = g_prox;

    if (linha != NULL) *linha = g_proxLinha;
    if (coluna != NULL) *coluna = g_proxColuna;

    if (c == '\n') {
        g_proxLinha++;
        g_proxColuna = 1;
    } else if (c != EOF) {
        g_proxColuna++;
    }

    if (c != EOF) {
        g_prox = fgetc(g_in);
    }

    return c;
}

/* ========================================================================
 * Registro de erros léxicos
 * ======================================================================== */

static FILE *g_saidaErr = NULL;
static FILE *g_saidaTS = NULL;
static int g_qtdErros = 0;

void DefinirSaidasAuxiliares(FILE *saidaTS, FILE *saidaErr) {
    g_saidaTS = saidaTS;
    g_saidaErr = saidaErr;
}

static void registrarErro(const char *tipo, const char *lexema, int linha, int coluna) {
    if (g_saidaErr != NULL) {
        fprintf(g_saidaErr, "<%s, %s> %d %d\n", tipo, lexema, linha, coluna);
    }
    g_qtdErros++;
}

/* ========================================================================
 * Reconhecimento de lexemas (núcleo do AFD)
 * ======================================================================== */

static void adicionarCharNoBuffer(char *buffer, int *i, int c) {
    if (*i < MAX_LEXEMA - 1) {
        buffer[*i] = (char)c;
    }
    (*i)++;
}

/* Processa identificador/instrução: (letra|_)(letra|digito|_)* */
static int tratarIdentificador(int c, int linhaInicio, int colunaInicio, Token *token) {
    char buffer[MAX_LEXEMA];
    char bufferMin[MAX_LEXEMA];
    int i = 0;

    adicionarCharNoBuffer(buffer, &i, c);
    while (isalnum((unsigned char)espiar()) || espiar() == '_') {
        adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
    }
    if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
    buffer[i] = '\0';

    paraMinusculas(buffer, bufferMin);
    const char *tokenInstrucao = buscarToken(TAB_INSTRUCOES, N_INSTRUCOES, bufferMin);

    if (tokenInstrucao != NULL) {
        tabelaInserirOuAtualizar(bufferMin, "instrução", linhaInicio, colunaInicio);
        strncpy(token->nome, tokenInstrucao, MAX_NOME_TOKEN - 1);
        token->nome[MAX_NOME_TOKEN - 1] = '\0';
    } else {
        /* Identificador: usado como rótulo (se seguido de ':') ou referência */
        tabelaInserirOuAtualizar(buffer, "identificador/rótulo", linhaInicio, colunaInicio);
        strncpy(token->nome, "ID", MAX_NOME_TOKEN - 1);
        token->nome[MAX_NOME_TOKEN - 1] = '\0';
    }

    strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
    token->lexema[MAX_LEXEMA - 1] = '\0';
    token->linha = linhaInicio;
    token->coluna = colunaInicio;
    return 1; /* token válido */
}

/* Processa diretiva: '.' seguido de letras. Retorna 0 se gerou erro (sem token). */
static int tratarDiretiva(int linhaInicio, int colunaInicio, Token *token) {
    char buffer[MAX_LEXEMA];
    char bufferMin[MAX_LEXEMA];
    int i = 0;

    adicionarCharNoBuffer(buffer, &i, '.');
    while (isalpha((unsigned char)espiar())) {
        adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
    }
    if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
    buffer[i] = '\0';

    paraMinusculas(buffer, bufferMin);
    const char *tokenDiretiva = buscarToken(TAB_DIRETIVAS, N_DIRETIVAS, bufferMin);

    if (tokenDiretiva != NULL) {
        tabelaInserirOuAtualizar(bufferMin, "diretiva", linhaInicio, colunaInicio);
        strncpy(token->nome, tokenDiretiva, MAX_NOME_TOKEN - 1);
        token->nome[MAX_NOME_TOKEN - 1] = '\0';
        strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
        token->lexema[MAX_LEXEMA - 1] = '\0';
        token->linha = linhaInicio;
        token->coluna = colunaInicio;
        return 1;
    }

    registrarErro("ERRO_DIRETIVA_INVALIDA", buffer, linhaInicio, colunaInicio);
    return 0;
}

/* Processa registrador: '$' seguido de letras/dígitos. Retorna 0 em erro. */
static int tratarRegistrador(int linhaInicio, int colunaInicio, Token *token) {
    char buffer[MAX_LEXEMA];
    char bufferMin[MAX_LEXEMA];
    int i = 0;

    adicionarCharNoBuffer(buffer, &i, '$');
    while (isalnum((unsigned char)espiar())) {
        adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
    }
    if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
    buffer[i] = '\0';

    /* Forma numérica: $0 .. $31 */
    int ehNumerico = (i > 1);
    for (int k = 1; k < i && ehNumerico; k++) {
        if (!isdigit((unsigned char)buffer[k])) ehNumerico = 0;
    }

    if (ehNumerico) {
        int valor = atoi(buffer + 1);
        if (valor >= 0 && valor <= 31) {
            tabelaInserirOuAtualizar(buffer, "registrador", linhaInicio, colunaInicio);
            strncpy(token->nome, "REG", MAX_NOME_TOKEN - 1);
            token->nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
            token->lexema[MAX_LEXEMA - 1] = '\0';
            token->linha = linhaInicio;
            token->coluna = colunaInicio;
            return 1;
        }
        registrarErro("ERRO_REGISTRADOR_INVALIDO", buffer, linhaInicio, colunaInicio);
        return 0;
    }

    paraMinusculas(buffer, bufferMin);
    const char *tokenReg = buscarToken(TAB_REGISTRADORES, N_REGISTRADORES, bufferMin);

    if (tokenReg != NULL) {
        tabelaInserirOuAtualizar(bufferMin, "registrador", linhaInicio, colunaInicio);
        strncpy(token->nome, tokenReg, MAX_NOME_TOKEN - 1);
        token->nome[MAX_NOME_TOKEN - 1] = '\0';
        strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
        token->lexema[MAX_LEXEMA - 1] = '\0';
        token->linha = linhaInicio;
        token->coluna = colunaInicio;
        return 1;
    }

    registrarErro("ERRO_REGISTRADOR_INVALIDO", buffer, linhaInicio, colunaInicio);
    return 0;
}

/* Verifica se um caractere de escape é válido: n, t, ", \\, 0 */
static int escapeValido(int c) {
    return c == 'n' || c == 't' || c == '"' || c == '\\' || c == '0';
}

/* Processa cadeia de caracteres delimitada por aspas duplas. Retorna 0 em erro. */
static int tratarString(int linhaInicio, int colunaInicio, Token *token) {
    char buffer[MAX_LEXEMA];
    int i = 0;

    adicionarCharNoBuffer(buffer, &i, '"');

    for (;;) {
        int pc = espiar();

        if (pc == EOF) {
            if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
            buffer[i] = '\0';
            registrarErro("ERRO_STRING_NAO_FECHADA_EOF", buffer, linhaInicio, colunaInicio);
            return 0;
        }

        if (pc == '\n') {
            if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
            buffer[i] = '\0';
            registrarErro("ERRO_STRING_NAO_FECHADA_LINHA", buffer, linhaInicio, colunaInicio);
            return 0;
        }

        int linhaCh, colunaCh;
        int c = consumir(&linhaCh, &colunaCh);

        if (c == '"') {
            adicionarCharNoBuffer(buffer, &i, '"');
            if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
            buffer[i] = '\0';
            strncpy(token->nome, "STRING", MAX_NOME_TOKEN - 1);
            token->nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
            token->lexema[MAX_LEXEMA - 1] = '\0';
            token->linha = linhaInicio;
            token->coluna = colunaInicio;
            return 1;
        }

        if (c == '\\') {
            int pc2 = espiar();
            if (escapeValido(pc2)) {
                adicionarCharNoBuffer(buffer, &i, c);
                adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
            } else {
                char escBuf[4];
                escBuf[0] = '\\';
                if (pc2 != EOF) {
                    escBuf[1] = (char)pc2;
                    escBuf[2] = '\0';
                } else {
                    escBuf[1] = '\0';
                }
                registrarErro("ERRO_ESCAPE_INVALIDO", escBuf, linhaCh, colunaCh);
                adicionarCharNoBuffer(buffer, &i, c);
                if (pc2 != EOF) {
                    adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
                }
            }
        } else {
            adicionarCharNoBuffer(buffer, &i, c);
        }
    }
}

/* Processa número inteiro (decimal ou hexadecimal, com sinal opcional). */
static int tratarNumero(int primeiroDigito, int negativo, int linhaInicio, int colunaInicio, Token *token) {
    char buffer[MAX_LEXEMA];
    int i = 0;

    if (negativo) {
        adicionarCharNoBuffer(buffer, &i, '-');
    }
    adicionarCharNoBuffer(buffer, &i, primeiroDigito);

    if (primeiroDigito == '0' && (espiar() == 'x' || espiar() == 'X')) {
        adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
        int inicioHex = i;
        while (isalnum((unsigned char)espiar())) {
            adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
        }
        if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
        buffer[i] = '\0';

        int qtd = i - inicioHex;
        int valido = (qtd > 0);
        for (int k = inicioHex; k < i && valido; k++) {
            if (!isxdigit((unsigned char)buffer[k])) valido = 0;
        }

        if (valido) {
            strncpy(token->nome, "NUM_INT", MAX_NOME_TOKEN - 1);
            token->nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
            token->lexema[MAX_LEXEMA - 1] = '\0';
            token->linha = linhaInicio;
            token->coluna = colunaInicio;
            return 1;
        }

        registrarErro("ERRO_NUMERO_MALFORMADO", buffer, linhaInicio, colunaInicio);
        return 0;
    }

    while (isdigit((unsigned char)espiar())) {
        adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
    }

    if (isalpha((unsigned char)espiar()) || espiar() == '_') {
        while (isalnum((unsigned char)espiar()) || espiar() == '_') {
            adicionarCharNoBuffer(buffer, &i, consumir(NULL, NULL));
        }
        if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
        buffer[i] = '\0';
        registrarErro("ERRO_NUMERO_MALFORMADO", buffer, linhaInicio, colunaInicio);
        return 0;
    }

    if (i >= MAX_LEXEMA) i = MAX_LEXEMA - 1;
    buffer[i] = '\0';
    strncpy(token->nome, "NUM_INT", MAX_NOME_TOKEN - 1);
    token->nome[MAX_NOME_TOKEN - 1] = '\0';
    strncpy(token->lexema, buffer, MAX_LEXEMA - 1);
    token->lexema[MAX_LEXEMA - 1] = '\0';
    token->linha = linhaInicio;
    token->coluna = colunaInicio;
    return 1;
}

/* ========================================================================
 * Função principal do AFD: retorna o próximo token reconhecido, tratando
 * erros internamente (registra e continua a varredura) até encontrar um
 * token válido ou o fim do arquivo.
 * ======================================================================== */

static Token proximoToken(void) {
    Token token;

    for (;;) {
        /* Ignora espaços, tabulações e quebras de linha (não geram token) */
        while (espiar() == ' ' || espiar() == '\t' || espiar() == '\n' || espiar() == '\r') {
            consumir(NULL, NULL);
        }

        if (espiar() == EOF) {
            strncpy(token.nome, "TK_EOF", MAX_NOME_TOKEN - 1);
            token.nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token.lexema, "EOF", MAX_LEXEMA - 1);
            token.lexema[MAX_LEXEMA - 1] = '\0';
            token.linha = g_proxLinha;
            token.coluna = g_proxColuna;
            return token;
        }

        /* Defesa: comentário remanescente (não deveria ocorrer após o
         * pré-processamento, mas evita tokens espúrios se ocorrer). */
        if (espiar() == '#') {
            while (espiar() != '\n' && espiar() != EOF) {
                consumir(NULL, NULL);
            }
            continue;
        }

        int linhaInicio, colunaInicio;
        int c = consumir(&linhaInicio, &colunaInicio);

        if (isalpha((unsigned char)c) || c == '_') {
            if (tratarIdentificador(c, linhaInicio, colunaInicio, &token)) return token;
            continue;
        }

        if (c == '.') {
            if (tratarDiretiva(linhaInicio, colunaInicio, &token)) return token;
            continue;
        }

        if (c == '$') {
            if (tratarRegistrador(linhaInicio, colunaInicio, &token)) return token;
            continue;
        }

        if (c == '"') {
            if (tratarString(linhaInicio, colunaInicio, &token)) return token;
            continue;
        }

        if (c == '-') {
            if (isdigit((unsigned char)espiar())) {
                int digito = consumir(NULL, NULL);
                if (tratarNumero(digito, 1, linhaInicio, colunaInicio, &token)) return token;
                continue;
            }
            char buf[2] = { (char)c, '\0' };
            registrarErro("ERRO_CARACTERE_INVALIDO", buf, linhaInicio, colunaInicio);
            continue;
        }

        if (isdigit((unsigned char)c)) {
            if (tratarNumero(c, 0, linhaInicio, colunaInicio, &token)) return token;
            continue;
        }

        if (c == ',') {
            strncpy(token.nome, "SMB_COM", MAX_NOME_TOKEN - 1); token.nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token.lexema, ",", MAX_LEXEMA - 1); token.lexema[MAX_LEXEMA - 1] = '\0';
            token.linha = linhaInicio; token.coluna = colunaInicio;
            return token;
        }
        if (c == ':') {
            strncpy(token.nome, "SMB_COL", MAX_NOME_TOKEN - 1); token.nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token.lexema, ":", MAX_LEXEMA - 1); token.lexema[MAX_LEXEMA - 1] = '\0';
            token.linha = linhaInicio; token.coluna = colunaInicio;
            return token;
        }
        if (c == '(') {
            strncpy(token.nome, "SMB_OPA", MAX_NOME_TOKEN - 1); token.nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token.lexema, "(", MAX_LEXEMA - 1); token.lexema[MAX_LEXEMA - 1] = '\0';
            token.linha = linhaInicio; token.coluna = colunaInicio;
            return token;
        }
        if (c == ')') {
            strncpy(token.nome, "SMB_CPA", MAX_NOME_TOKEN - 1); token.nome[MAX_NOME_TOKEN - 1] = '\0';
            strncpy(token.lexema, ")", MAX_LEXEMA - 1); token.lexema[MAX_LEXEMA - 1] = '\0';
            token.linha = linhaInicio; token.coluna = colunaInicio;
            return token;
        }

        /* Qualquer outro caractere não reconhecido */
        char buf[2] = { (char)c, '\0' };
        registrarErro("ERRO_CARACTERE_INVALIDO", buf, linhaInicio, colunaInicio);
    }
}

/* ========================================================================
 * Função principal exigida pela especificação
 * ======================================================================== */

void AnaliseLexica(FILE *in, FILE *out) {
    tabelaReset();
    g_qtdErros = 0;
    tabelaInicializarReservados();
    leituraIniciar(in);

    for (;;) {
        Token t = proximoToken();
        fprintf(out, "<%s, %s> %d %d\n", t.nome, t.lexema, t.linha, t.coluna);
        if (strcmp(t.nome, "TK_EOF") == 0) {
            break;
        }
    }

    if (g_saidaTS != NULL) {
        fprintf(g_saidaTS, "LEXEMA,CATEGORIA,LINHA,COLUNA\n");
        for (int i = 0; i < g_tabelaQtd; i++) {
            fprintf(g_saidaTS, "%s,%s,%d,%d\n",
                    g_tabela[i].lexema, g_tabela[i].categoria,
                    g_tabela[i].primeiraLinha, g_tabela[i].primeiraColuna);
        }
    }

    if (g_saidaErr != NULL && g_qtdErros == 0) {
        fprintf(g_saidaErr, "Nenhum erro léxico encontrado.\n");
    }
}
