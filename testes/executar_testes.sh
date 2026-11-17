#!/bin/sh
# Compila o projeto e executa todos os casos de teste.
# Uso (a partir da raiz do repositório):  sh testes/executar_testes.sh
# As saídas de cada caso ficam em testes/saidas/<caso>.{pre,lex,ts,lex.err,sin.err,dot}
# e, para os programas válidos, a árvore é renderizada em PNG se o Graphviz estiver instalado.

set -u
cd "$(dirname "$0")/.." || exit 2

gcc -Wall -Wextra -std=c99 main.c preprocessador.c lexico.c sintatico.c -o main.exe || exit 2
mkdir -p testes/saidas

falhas=0
for entrada in testes/*.asm; do
    caso=$(basename "$entrada" .asm)
    case "$caso" in
        valido*)         esperado=0 ;;
        *)               esperado=1 ;;
    esac

    saida=$(./main.exe "$entrada" "testes/saidas/$caso")
    obtido=$?

    if [ "$obtido" -eq "$esperado" ]; then status="OK"; else status="FALHOU"; falhas=$((falhas + 1)); fi
    printf '%-30s esperado=%d obtido=%d  [%s]  %s\n' "$caso" "$esperado" "$obtido" "$status" "$(echo "$saida" | head -1)"

    if [ "$obtido" -eq 0 ] && command -v dot >/dev/null 2>&1; then
        dot -Tpng "testes/saidas/$caso.dot" -o "testes/saidas/$caso.png"
    fi
done

echo
if [ "$falhas" -eq 0 ]; then echo "Todos os casos produziram o resultado esperado."; else echo "$falhas caso(s) divergente(s)."; fi
exit "$falhas"
