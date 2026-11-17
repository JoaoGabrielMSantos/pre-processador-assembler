# Compilador Assembly MIPS (subconjunto MARS) — LFAC/UCB

Pré-processador, analisador léxico e analisador sintático (descendente recursivo) em C para um
subconjunto de Assembly MIPS.

## Compilar e executar

```sh
gcc main.c preprocessador.c lexico.c sintatico.c -o main.exe
./main.exe <arquivo_entrada.asm> <prefixo_saida>
```

Saídas: `<prefixo>.pre`, `.lex`, `.ts`, `.lex.err`, `.sin.err` e `.dot` (árvore sintática
concreta, somente para programas válidos). Para renderizar a árvore:

```sh
dot -Tpng <prefixo>.dot -o <prefixo>.png
```

Códigos de saída: `0` válido, `1` erros léxicos ou sintáticos, `2` falha operacional ou uso incorreto.

## Testes

```sh
sh testes/executar_testes.sh
```

Detalhes da implementação, casos de teste e resultados estão em [RELATORIO.md](RELATORIO.md).
