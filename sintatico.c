#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "sintatico.h"

/* ========================================================================
 * Estado do módulo
 * ======================================================================== */

/* Sequência de tokens (com EOL e TK_EOF já inseridos) */
static Token *g_tokens = NULL;
static int g_qtdTokens = 0;
static int g_capTokens = 0;
static int g_pos = 0;

/* Árvore sintática concreta */
static NoArvore *g_nos = NULL;
static int g_qtdNos = 0;
static int g_capNos = 0;

/* Diagnósticos */
static FILE *g_erros = NULL;
static int g_qtdErros = 0;

/* Desvio para falhas operacionais (memória) ocorridas durante a análise */
static jmp_buf g_falha;

static void liberarEstado(void) {
    free(g_tokens);
    free(g_nos);
    g_tokens = NULL;
    g_nos = NULL;
    g_qtdTokens = g_capTokens = g_pos = 0;
    g_qtdNos = g_capNos = 0;
}

/* ========================================================================
 * Carga dos tokens a partir do arquivo .lex
 * Formato de cada linha:  <CATEGORIA, lexema> linha coluna
 * ======================================================================== */

static void copiarCampo(char *destino, size_t tamanho, const char *origem) {
    strncpy(destino, origem, tamanho - 1);
    destino[tamanho - 1] = '\0';
}

static void adicionarToken(const char *categoria, const char *lexema, int linha, int coluna) {
    if (g_qtdTokens >= g_capTokens) {
        int novaCap = (g_capTokens == 0) ? 256 : g_capTokens * 2;
        Token *novo = (Token *)realloc(g_tokens, (size_t)novaCap * sizeof(Token));
        if (novo == NULL) longjmp(g_falha, 1);
        g_tokens = novo;
        g_capTokens = novaCap;
    }
    Token *t = &g_tokens[g_qtdTokens++];
    copiarCampo(t->nome, sizeof(t->nome), categoria);
    copiarCampo(t->lexema, sizeof(t->lexema), lexema);
    t->linha = linha;
    t->coluna = coluna;
}

static int lerInteiro(const char *s, int *valor) {
    char *fim;
    long v = strtol(s, &fim, 10);
    if (fim == s || *fim != '\0') return 0;
    *valor = (int)v;
    return 1;
}

/*
 * Interpreta uma linha do .lex. Retorna 1 se um token foi lido, 0 se a linha
 * estava vazia e -1 se o formato é inválido. A categoria termina na primeira
 * sequência ", " (categorias não contêm vírgulas); linha e coluna são os dois
 * últimos campos. Assim, lexemas de STRING com vírgulas, espaços ou '>' são
 * preservados integralmente.
 */
static int interpretarLinhaLex(char *s, Token *t) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    if (n == 0) return 0;
    if (s[0] != '<') return -1;

    char *sep = strstr(s, ", ");
    if (sep == NULL) return -1;

    char *espaco = strrchr(s, ' ');
    if (espaco == NULL || espaco < sep) return -1;
    if (!lerInteiro(espaco + 1, &t->coluna)) return -1;
    *espaco = '\0';

    espaco = strrchr(s, ' ');
    if (espaco == NULL || espaco < sep) return -1;
    if (!lerInteiro(espaco + 1, &t->linha)) return -1;
    *espaco = '\0';

    n = strlen(s);
    if (n == 0 || s[n - 1] != '>') return -1;
    s[n - 1] = '\0';

    *sep = '\0';
    copiarCampo(t->nome, sizeof(t->nome), s + 1);
    copiarCampo(t->lexema, sizeof(t->lexema), sep + 2);
    return 1;
}

/*
 * Lê os tokens do .lex e produz a sequência usada pelo reconhecedor:
 *  - um EOL é inserido após o último token de cada linha que contém tokens
 *    (inclusive a última linha, mesmo sem quebra de linha final);
 *  - TK_EOF é acrescentado se não estiver presente e nunca é duplicado.
 * Retorna 0 em sucesso e -1 em falha de leitura ou formato.
 */
static int carregarTokens(FILE *in) {
    char buffer[4096];
    Token lidos = {"", "", 0, 0};
    Token *brutos = NULL;
    int qtd = 0, cap = 0;
    int temEOF = 0;
    Token tokEOF = {"", "", 0, 0};

    rewind(in);
    while (fgets(buffer, sizeof(buffer), in) != NULL) {
        size_t len = strlen(buffer);
        if (len == sizeof(buffer) - 1 && buffer[len - 1] != '\n' && !feof(in)) {
            free(brutos);
            return -1; /* linha excessivamente longa */
        }
        int r = interpretarLinhaLex(buffer, &lidos);
        if (r < 0) { free(brutos); return -1; }
        if (r == 0) continue;

        if (strcmp(lidos.nome, CAT_TK_EOF) == 0) {
            temEOF = 1;
            tokEOF = lidos;
            break; /* nada após TK_EOF é considerado */
        }

        if (qtd >= cap) {
            int novaCap = (cap == 0) ? 256 : cap * 2;
            Token *novo = (Token *)realloc(brutos, (size_t)novaCap * sizeof(Token));
            if (novo == NULL) { free(brutos); return -1; }
            brutos = novo;
            cap = novaCap;
        }
        brutos[qtd++] = lidos;
    }
    if (ferror(in)) { free(brutos); return -1; }

    for (int i = 0; i < qtd; i++) {
        adicionarToken(brutos[i].nome, brutos[i].lexema, brutos[i].linha, brutos[i].coluna);
        if (i == qtd - 1 || brutos[i + 1].linha != brutos[i].linha) {
            /* EOL logo após o último lexema da linha */
            adicionarToken(CAT_EOL, "", brutos[i].linha,
                           brutos[i].coluna + (int)strlen(brutos[i].lexema));
        }
    }

    if (temEOF) {
        adicionarToken(CAT_TK_EOF, "", tokEOF.linha, tokEOF.coluna);
    } else {
        int linha = (qtd > 0) ? brutos[qtd - 1].linha + 1 : 1;
        adicionarToken(CAT_TK_EOF, "", linha, 1);
    }

    free(brutos);
    return 0;
}

/* ========================================================================
 * Árvore sintática concreta
 * ======================================================================== */

static int novoNo(int pai, const char *rotulo, const char *lexema, int terminal) {
    if (g_qtdNos >= g_capNos) {
        int novaCap = (g_capNos == 0) ? 512 : g_capNos * 2;
        NoArvore *novo = (NoArvore *)realloc(g_nos, (size_t)novaCap * sizeof(NoArvore));
        if (novo == NULL) longjmp(g_falha, 1);
        g_nos = novo;
        g_capNos = novaCap;
    }
    int id = g_qtdNos++;
    NoArvore *no = &g_nos[id];
    copiarCampo(no->rotulo, sizeof(no->rotulo), rotulo);
    copiarCampo(no->lexema, sizeof(no->lexema), lexema ? lexema : "");
    no->terminal = terminal;
    no->primeiroFilho = no->ultimoFilho = no->proximoIrmao = -1;

    if (pai >= 0) {
        if (g_nos[pai].ultimoFilho < 0) {
            g_nos[pai].primeiroFilho = id;
        } else {
            g_nos[g_nos[pai].ultimoFilho].proximoIrmao = id;
        }
        g_nos[pai].ultimoFilho = id;
    }
    return id;
}

/* Cria um nó para um não terminal: o rótulo é "<nome>". */
static int naoTerminal(int pai, const char *nome) {
    char rotulo[MAX_ROTULO_NO];
    snprintf(rotulo, sizeof(rotulo), "<%s>", nome);
    return novoNo(pai, rotulo, NULL, 0);
}

/* Escreve 'texto' em um rótulo DOT entre aspas, escapando " e \ */
static void escreverEscapado(FILE *out, const char *texto) {
    for (const char *p = texto; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out);  break;
            case '\r': break;
            default:   fputc(*p, out);
        }
    }
}

static void escreverNos(FILE *out, int id, int *contador, int *numero) {
    const NoArvore *no = &g_nos[id];
    numero[id] = (*contador)++;

    fprintf(out, "    n%d [label=\"", numero[id]);
    escreverEscapado(out, no->rotulo);
    if (no->terminal && no->lexema[0] != '\0') {
        fputs("\\n", out);
        escreverEscapado(out, no->lexema);
    }
    fputs("\"];\n", out);

    for (int f = no->primeiroFilho; f >= 0; f = g_nos[f].proximoIrmao) {
        escreverNos(out, f, contador, numero);
    }
}

static void escreverArestas(FILE *out, int id, const int *numero) {
    const NoArvore *no = &g_nos[id];
    for (int f = no->primeiroFilho; f >= 0; f = g_nos[f].proximoIrmao) {
        fprintf(out, "    n%d -> n%d;\n", numero[id], numero[f]);
    }
    for (int f = no->primeiroFilho; f >= 0; f = g_nos[f].proximoIrmao) {
        escreverArestas(out, f, numero);
    }
}

/* Gera a árvore em DOT; identificadores em pré-ordem (raiz = n0). */
static int gerarDot(FILE *out) {
    int *numero = (int *)malloc((size_t)(g_qtdNos > 0 ? g_qtdNos : 1) * sizeof(int));
    if (numero == NULL) return -1;
    int contador = 0;

    fputs("digraph ArvoreSintatica {\n", out);
    fputs("    graph [ordering=out];\n", out);
    fputs("    node [shape=box, fontname=\"Arial\"];\n", out);
    fputs("    edge [arrowhead=none];\n\n", out);
    escreverNos(out, 0, &contador, numero);
    fputs("\n", out);
    escreverArestas(out, 0, numero);
    fputs("}\n", out);

    free(numero);
    return ferror(out) ? -1 : 0;
}

/* ========================================================================
 * Acesso aos tokens
 * ======================================================================== */

static const Token *atual(void) { return &g_tokens[g_pos]; }

static int eh(const char *categoria) { return strcmp(atual()->nome, categoria) == 0; }

static int ehUmDe(const char *const lista[]) {
    for (int i = 0; lista[i] != NULL; i++) {
        if (eh(lista[i])) return 1;
    }
    return 0;
}

/* Nunca avança além de TK_EOF (último elemento da sequência) */
static void avancar(void) {
    if (g_pos < g_qtdTokens - 1) g_pos++;
}

/* Conjuntos de categorias usados nas decisões (lookahead de 1 token) */
static const char *const SECOES[]           = {"DIR_DATA", "DIR_TEXT", NULL};
static const char *const DIRETIVAS_DADOS[]  = {"DIR_WORD", "DIR_HALF", "DIR_BYTE", "DIR_ASCII",
                                               "DIR_ASCIIZ", "DIR_SPACE", "DIR_ALIGN", NULL};
static const char *const DIR_INTEIROS[]     = {"DIR_WORD", "DIR_HALF", "DIR_BYTE", NULL};
static const char *const DIR_STRING[]       = {"DIR_ASCII", "DIR_ASCIIZ", NULL};
static const char *const OP_TRES_REG[]      = {"INS_ADD", "INS_ADDU", "INS_SUB", "INS_SUBU", "INS_MUL",
                                               "INS_AND", "INS_OR", "INS_XOR", "INS_NOR", "INS_SLT", NULL};
static const char *const OP_DOIS_REG[]      = {"INS_MOVE", "INS_DIV", NULL};
static const char *const OP_UM_REG[]        = {"INS_MFLO", "INS_MFHI", "INS_JR", NULL};
static const char *const OP_DESLOCAMENTO[]  = {"INS_SLL", "INS_SRL", NULL};
static const char *const OP_MEMORIA[]       = {"INS_LW", "INS_SW", "INS_LB", "INS_SB", "INS_LH", "INS_SH", NULL};
static const char *const OP_DESVIO[]        = {"INS_BEQ", "INS_BNE", "INS_BLT", "INS_BLE", "INS_BGT", "INS_BGE", NULL};
static const char *const OP_SALTO[]         = {"INS_J", "INS_JAL", NULL};
static const char *const OP_SEM_OPERANDOS[] = {"INS_SYSCALL", "INS_NOP", NULL};

static int ehInstrucao(void) { return strncmp(atual()->nome, "INS_", 4) == 0; }

/* ========================================================================
 * Diagnóstico e recuperação de erros
 * ======================================================================== */

static void erroSintatico(const char *esperado) {
    const Token *t = atual();
    g_qtdErros++;

    fprintf(g_erros, "Linha %d, coluna %d: Encontrado ", t->linha, t->coluna);
    if (strcmp(t->nome, CAT_EOL) == 0) {
        fputs("o término da linha (EOL)", g_erros);
    } else if (strcmp(t->nome, CAT_TK_EOF) == 0) {
        fputs("TK_EOF", g_erros);
    } else {
        fprintf(g_erros, "%s(\"%s\")", t->nome, t->lexema);
    }
    fprintf(g_erros, "; esperado %s.\n", esperado);
}

/*
 * Modo pânico por linha: descarta os tokens restantes da linha corrente até
 * EOL (inclusive). Se o token corrente já for EOL, consome apenas ele; se for
 * TK_EOF, nada é consumido. Nunca descarta tokens de uma linha posterior.
 */
static void recuperar(void) {
    while (!eh(CAT_EOL) && !eh(CAT_TK_EOF)) avancar();
    if (eh(CAT_EOL)) avancar();
}

/* Consome o token corrente como folha do nó 'pai'. */
static void folha(int pai) {
    const Token *t = atual();
    novoNo(pai, t->nome, t->lexema, 1);
    avancar();
}

/* Verifica a categoria esperada; em caso de divergência registra o erro. */
static int esperar(int pai, const char *categoria, const char *descricao) {
    if (eh(categoria)) {
        folha(pai);
        return 1;
    }
    erroSintatico(descricao);
    return 0;
}

#define ESP_REG     "REG"
#define ESP_COM     "SMB_COM(\",\")"
#define ESP_COL     "SMB_COL(\":\")"
#define ESP_OPA     "SMB_OPA(\"(\")"
#define ESP_CPA     "SMB_CPA(\")\")"
#define ESP_NUM     "NUM_INT"
#define ESP_ID      "ID"
#define ESP_STRING  "STRING"
#define ESP_EOL     "EOL (término da linha)"

/* ========================================================================
 * Reconhecedores: uma função para cada não terminal da gramática.
 * Retornam 1 se a construção foi reconhecida e 0 se houve erro sintático.
 * ======================================================================== */

/* <rotulo> ::= ID ":" */
static int rotulo(int pai) {
    int n = naoTerminal(pai, "rotulo");
    return esperar(n, "ID", ESP_ID) && esperar(n, "SMB_COL", ESP_COL);
}

/* <diretiva_global> ::= ".globl" ID */
static int diretiva_global(int pai) {
    int n = naoTerminal(pai, "diretiva_global");
    return esperar(n, "DIR_GLOBL", "DIR_GLOBL(\".globl\")") && esperar(n, "ID", ESP_ID);
}

/* <diretiva_inteiros> ::= ".word" | ".half" | ".byte" */
static int diretiva_inteiros(int pai) {
    int n = naoTerminal(pai, "diretiva_inteiros");
    if (ehUmDe(DIR_INTEIROS)) { folha(n); return 1; }
    erroSintatico("DIR_WORD, DIR_HALF ou DIR_BYTE");
    return 0;
}

/* <lista_inteiros> ::= NUM_INT { "," NUM_INT } */
static int lista_inteiros(int pai) {
    int n = naoTerminal(pai, "lista_inteiros");
    if (!esperar(n, "NUM_INT", ESP_NUM)) return 0;
    while (eh("SMB_COM")) {
        folha(n);
        if (!esperar(n, "NUM_INT", ESP_NUM)) return 0;
    }
    return 1;
}

/* <diretiva_string> ::= ".ascii" | ".asciiz" */
static int diretiva_string(int pai) {
    int n = naoTerminal(pai, "diretiva_string");
    if (ehUmDe(DIR_STRING)) { folha(n); return 1; }
    erroSintatico("DIR_ASCII ou DIR_ASCIIZ");
    return 0;
}

/*
 * <diretiva_dados> ::= <diretiva_inteiros> <lista_inteiros>
 *                    | <diretiva_string> STRING
 *                    | ".space" NUM_INT
 *                    | ".align" NUM_INT
 */
static int diretiva_dados(int pai) {
    int n = naoTerminal(pai, "diretiva_dados");
    if (ehUmDe(DIR_INTEIROS)) {
        return diretiva_inteiros(n) && lista_inteiros(n);
    }
    if (ehUmDe(DIR_STRING)) {
        return diretiva_string(n) && esperar(n, "STRING", ESP_STRING);
    }
    if (eh("DIR_SPACE") || eh("DIR_ALIGN")) {
        folha(n);
        return esperar(n, "NUM_INT", ESP_NUM);
    }
    erroSintatico("diretiva de dados (.word, .half, .byte, .ascii, .asciiz, .space ou .align)");
    return 0;
}

/* Reconhece um operador pertencente a 'lista' como filho de um nó <nome>. */
static int operador(int pai, const char *nome, const char *const lista[], const char *descricao) {
    int n = naoTerminal(pai, nome);
    if (ehUmDe(lista)) { folha(n); return 1; }
    erroSintatico(descricao);
    return 0;
}

/* <op_tres_registradores> ::= "add" | "addu" | "sub" | "subu" | "mul" | "and" | "or" | "xor" | "nor" | "slt" */
static int op_tres_registradores(int pai) {
    return operador(pai, "op_tres_registradores", OP_TRES_REG, "operação com três registradores");
}

/* <op_dois_registradores> ::= "move" | "div" */
static int op_dois_registradores(int pai) {
    return operador(pai, "op_dois_registradores", OP_DOIS_REG, "INS_MOVE ou INS_DIV");
}

/* <op_um_registrador> ::= "mflo" | "mfhi" | "jr" */
static int op_um_registrador(int pai) {
    return operador(pai, "op_um_registrador", OP_UM_REG, "INS_MFLO, INS_MFHI ou INS_JR");
}

/* <op_deslocamento> ::= "sll" | "srl" */
static int op_deslocamento(int pai) {
    return operador(pai, "op_deslocamento", OP_DESLOCAMENTO, "INS_SLL ou INS_SRL");
}

/* <op_memoria> ::= "lw" | "sw" | "lb" | "sb" | "lh" | "sh" */
static int op_memoria(int pai) {
    return operador(pai, "op_memoria", OP_MEMORIA, "operação de acesso à memória");
}

/* <op_desvio> ::= "beq" | "bne" | "blt" | "ble" | "bgt" | "bge" */
static int op_desvio(int pai) {
    return operador(pai, "op_desvio", OP_DESVIO, "operação de desvio condicional");
}

/* <op_salto> ::= "j" | "jal" */
static int op_salto(int pai) {
    return operador(pai, "op_salto", OP_SALTO, "INS_J ou INS_JAL");
}

/* <instrucao_tres_registradores> ::= <op_tres_registradores> REG "," REG "," REG */
static int instrucao_tres_registradores(int pai) {
    int n = naoTerminal(pai, "instrucao_tres_registradores");
    return op_tres_registradores(n)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG);
}

/* <instrucao_dois_registradores> ::= <op_dois_registradores> REG "," REG */
static int instrucao_dois_registradores(int pai) {
    int n = naoTerminal(pai, "instrucao_dois_registradores");
    return op_dois_registradores(n)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG);
}

/* <instrucao_um_registrador> ::= <op_um_registrador> REG */
static int instrucao_um_registrador(int pai) {
    int n = naoTerminal(pai, "instrucao_um_registrador");
    return op_um_registrador(n) && esperar(n, "REG", ESP_REG);
}

/* <instrucao_imediato> ::= "addi" REG "," REG "," NUM_INT */
static int instrucao_imediato(int pai) {
    int n = naoTerminal(pai, "instrucao_imediato");
    return esperar(n, "INS_ADDI", "INS_ADDI(\"addi\")")
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "NUM_INT", ESP_NUM);
}

/* <instrucao_deslocamento> ::= <op_deslocamento> REG "," REG "," NUM_INT */
static int instrucao_deslocamento(int pai) {
    int n = naoTerminal(pai, "instrucao_deslocamento");
    return op_deslocamento(n)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "NUM_INT", ESP_NUM);
}

/* <instrucao_li> ::= "li" REG "," NUM_INT */
static int instrucao_li(int pai) {
    int n = naoTerminal(pai, "instrucao_li");
    return esperar(n, "INS_LI", "INS_LI(\"li\")")
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "NUM_INT", ESP_NUM);
}

/* <instrucao_la> ::= "la" REG "," ID */
static int instrucao_la(int pai) {
    int n = naoTerminal(pai, "instrucao_la");
    return esperar(n, "INS_LA", "INS_LA(\"la\")")
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "ID", ESP_ID);
}

/* <endereco> ::= ID | NUM_INT "(" REG ")" */
static int endereco(int pai) {
    int n = naoTerminal(pai, "endereco");
    if (eh("ID")) { folha(n); return 1; }
    if (eh("NUM_INT")) {
        folha(n);
        return esperar(n, "SMB_OPA", ESP_OPA)
            && esperar(n, "REG", ESP_REG)
            && esperar(n, "SMB_CPA", ESP_CPA);
    }
    erroSintatico("ID ou NUM_INT (endereço)");
    return 0;
}

/* <instrucao_memoria> ::= <op_memoria> REG "," <endereco> */
static int instrucao_memoria(int pai) {
    int n = naoTerminal(pai, "instrucao_memoria");
    return op_memoria(n)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && endereco(n);
}

/* <instrucao_desvio> ::= <op_desvio> REG "," REG "," ID */
static int instrucao_desvio(int pai) {
    int n = naoTerminal(pai, "instrucao_desvio");
    return op_desvio(n)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "REG", ESP_REG) && esperar(n, "SMB_COM", ESP_COM)
        && esperar(n, "ID", ESP_ID);
}

/* <instrucao_salto> ::= <op_salto> ID */
static int instrucao_salto(int pai) {
    int n = naoTerminal(pai, "instrucao_salto");
    return op_salto(n) && esperar(n, "ID", ESP_ID);
}

/* <instrucao_sem_operandos> ::= "syscall" | "nop" */
static int instrucao_sem_operandos(int pai) {
    int n = naoTerminal(pai, "instrucao_sem_operandos");
    if (ehUmDe(OP_SEM_OPERANDOS)) { folha(n); return 1; }
    erroSintatico("INS_SYSCALL ou INS_NOP");
    return 0;
}

/* <instrucao> ::= um dos onze grupos, escolhido pelo próximo token */
static int instrucao(int pai) {
    int n = naoTerminal(pai, "instrucao");
    if (ehUmDe(OP_TRES_REG))      return instrucao_tres_registradores(n);
    if (ehUmDe(OP_DOIS_REG))      return instrucao_dois_registradores(n);
    if (ehUmDe(OP_UM_REG))        return instrucao_um_registrador(n);
    if (eh("INS_ADDI"))           return instrucao_imediato(n);
    if (ehUmDe(OP_DESLOCAMENTO))  return instrucao_deslocamento(n);
    if (eh("INS_LI"))             return instrucao_li(n);
    if (eh("INS_LA"))             return instrucao_la(n);
    if (ehUmDe(OP_MEMORIA))       return instrucao_memoria(n);
    if (ehUmDe(OP_DESVIO))        return instrucao_desvio(n);
    if (ehUmDe(OP_SALTO))         return instrucao_salto(n);
    if (ehUmDe(OP_SEM_OPERANDOS)) return instrucao_sem_operandos(n);
    erroSintatico("instrução");
    return 0;
}

/*
 * <linha_dados> ::= <rotulo> [ <diretiva_dados> ] EOL
 *                 | <diretiva_dados> EOL
 *                 | <diretiva_global> EOL
 */
static void linha_dados(int pai) {
    int n = naoTerminal(pai, "linha_dados");
    int ok;

    if (eh("ID")) {
        ok = rotulo(n);
        if (ok) {
            if (ehUmDe(DIRETIVAS_DADOS)) {
                ok = diretiva_dados(n) && esperar(n, CAT_EOL, ESP_EOL);
            } else if (ehInstrucao()) {
                erroSintatico("diretiva de dados ou EOL (instruções só são admitidas na seção .text)");
                ok = 0;
            } else {
                ok = esperar(n, CAT_EOL, "diretiva de dados ou EOL");
            }
        }
    } else if (ehUmDe(DIRETIVAS_DADOS)) {
        ok = diretiva_dados(n) && esperar(n, CAT_EOL, ESP_EOL);
    } else if (eh("DIR_GLOBL")) {
        ok = diretiva_global(n) && esperar(n, CAT_EOL, ESP_EOL);
    } else if (ehInstrucao()) {
        erroSintatico("rótulo, diretiva de dados ou .globl (instruções só são admitidas na seção .text)");
        ok = 0;
    } else {
        erroSintatico("rótulo (ID), diretiva de dados ou DIR_GLOBL(\".globl\")");
        ok = 0;
    }

    if (!ok) recuperar();
}

/*
 * <linha_codigo> ::= <rotulo> [ <instrucao> ] EOL
 *                  | <instrucao> EOL
 *                  | <diretiva_global> EOL
 */
static void linha_codigo(int pai) {
    int n = naoTerminal(pai, "linha_codigo");
    int ok;

    if (eh("ID")) {
        ok = rotulo(n);
        if (ok) {
            if (ehInstrucao()) {
                ok = instrucao(n) && esperar(n, CAT_EOL, ESP_EOL);
            } else if (ehUmDe(DIRETIVAS_DADOS)) {
                erroSintatico("instrução ou EOL (diretivas de dados só são admitidas na seção .data)");
                ok = 0;
            } else {
                ok = esperar(n, CAT_EOL, "instrução ou EOL");
            }
        }
    } else if (ehInstrucao()) {
        ok = instrucao(n) && esperar(n, CAT_EOL, ESP_EOL);
    } else if (eh("DIR_GLOBL")) {
        ok = diretiva_global(n) && esperar(n, CAT_EOL, ESP_EOL);
    } else if (ehUmDe(DIRETIVAS_DADOS)) {
        erroSintatico("rótulo, instrução ou .globl (diretivas de dados só são admitidas na seção .data)");
        ok = 0;
    } else {
        erroSintatico("rótulo (ID), instrução ou DIR_GLOBL(\".globl\")");
        ok = 0;
    }

    if (!ok) recuperar();
}

/* <secao_dados> ::= ".data" EOL { <linha_dados> } */
static void secao_dados(int pai) {
    int n = naoTerminal(pai, "secao_dados");
    folha(n); /* DIR_DATA, já verificado por <secao> */
    if (!esperar(n, CAT_EOL, ESP_EOL)) recuperar();
    while (!ehUmDe(SECOES) && !eh(CAT_TK_EOF)) {
        linha_dados(n);
    }
}

/* <secao_codigo> ::= ".text" EOL { <linha_codigo> } */
static void secao_codigo(int pai) {
    int n = naoTerminal(pai, "secao_codigo");
    folha(n); /* DIR_TEXT, já verificado por <secao> */
    if (!esperar(n, CAT_EOL, ESP_EOL)) recuperar();
    while (!ehUmDe(SECOES) && !eh(CAT_TK_EOF)) {
        linha_codigo(n);
    }
}

/* <secao> ::= <secao_dados> | <secao_codigo> */
static void secao(int pai) {
    int n = naoTerminal(pai, "secao");
    if (eh("DIR_DATA")) secao_dados(n);
    else                secao_codigo(n);
}

/* <programa> ::= <secao> { <secao> } TK_EOF */
static void programa(void) {
    int n = naoTerminal(-1, "programa");
    int secoes = 0;

    /* Conteúdo antes da primeira seção é erro; cada linha é diagnosticada
     * e descartada até que uma seção seja aberta ou a entrada termine. */
    while (!eh(CAT_TK_EOF)) {
        if (ehUmDe(SECOES)) {
            secao(n);
            secoes++;
        } else {
            erroSintatico("abertura de seção: DIR_DATA(\".data\") ou DIR_TEXT(\".text\")");
            recuperar();
        }
    }

    if (secoes == 0 && g_qtdErros == 0) {
        /* Arquivo vazio ou apenas com comentários */
        erroSintatico("abertura de seção: DIR_DATA(\".data\") ou DIR_TEXT(\".text\")");
    }

    folha(n); /* TK_EOF: a entrada é consumida integralmente */
}

/* ========================================================================
 * Interface pública
 * ======================================================================== */

int AnaliseSintatica(FILE *tokens, FILE *arvore, FILE *erros) {
    if (tokens == NULL || arvore == NULL || erros == NULL) return -1;

    liberarEstado();
    g_erros = erros;
    g_qtdErros = 0;

    if (setjmp(g_falha) != 0) {
        /* Falha de alocação durante a carga ou o reconhecimento */
        liberarEstado();
        return -1;
    }

    if (carregarTokens(tokens) != 0) {
        liberarEstado();
        return -1;
    }

    programa();

    int resultado;
    if (g_qtdErros == 0) {
        fputs("Nenhum erro sintático encontrado.\n", erros);
        resultado = (gerarDot(arvore) == 0) ? 0 : -1;
    } else {
        /* Programa inválido: o .dot permanece vazio */
        resultado = 1;
    }

    if (ferror(erros) || ferror(arvore)) resultado = -1;

    liberarEstado();
    return resultado;
}
