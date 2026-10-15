#!/bin/sh
# Tests : on lance la calculatrice en mode rapide et on compare le resultat.
cd "$(dirname "$0")" || exit 1
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 calc.c -o calc -lm || exit 1

fail=0
ok() {
    expr=$1; expected=$2
    got=$(./calc "$expr" 2>&1)
    if [ "$got" = "$expected" ]; then echo "ok   - $expr = $got"
    else echo "FAIL - $expr : attendu '$expected', obtenu '$got'"; fail=1; fi
}
bad() {
    expr=$1
    if ./calc "$expr" >/dev/null 2>&1; then echo "FAIL - '$expr' aurait du echouer"; fail=1
    else echo "ok   - '$expr' est refuse"; fi
}

# operations de base et priorites
ok "2+3*4" 14
ok "(2+3)*4" 20
ok "10/4" 2.5
ok "7-10" -3
ok "-2^2" -4
ok "2^-2" 0.25
ok "2^3^2" 512
ok "0.1+0.2" 0.3
ok "2,5*2" 5
ok "2e3+1" 2001
# factorielle
ok "5!" 120
ok "0!" 1
ok "3!!" 720
ok "(2+3)!" 120
ok "2^3!" 64
ok "170!" 7.257415615e+306
# valeur absolue
ok "|3-9|" 6
ok "|9-3|" 6
ok "|1-|2-5||" 2
ok "|3|-|4|" -1
# puissance, exp, ln
ok "2^10" 1024
ok "4^0.5" 2
ok "exp(0)" 1
ok "exp(1)" 2.718281828
ok "ln(e)" 1
ok "ln(10)" 2.302585093
ok "ln(exp(5))" 5
ok "pi" 3.141592654
# melange
ok "5!+|3-9|*2^3" 168
ok " 2 +  3 " 5
ok "EXP(0) + LN(E)" 2

# erreurs
bad "1/0"
bad "5/(3-3)"
bad "ln(0)"
bad "ln(-1)"
bad "(-1)!"
bad "2.5!"
bad "171!"
bad "(-8)^0.5"
bad "0^-1"
bad "exp(1000)"
bad "2+"
bad "(2+3"
bad "2+3)"
bad "|3-9"
bad "foo"
bad "2 3"
bad "exp 2"
bad "*3"
bad "."

exit $fail
