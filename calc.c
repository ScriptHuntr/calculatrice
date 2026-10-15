/* Calculatrice en ligne de commande.
   Operations : + - * /  ^ (puissance)  ! (factorielle)  |x| (valeur absolue)  exp()  ln()
   Constantes : pi, e, ans (dernier resultat).
   Les calculs sont lus comme du texte (ex. "5! + |3-9| * 2^3") par un petit analyseur recursif. */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define isatty _isatty
#define STDOUT_FD 1
#else
#include <unistd.h>
#define STDOUT_FD STDOUT_FILENO
#endif

#define MAX_LINE 256
#define MAX_HISTORY 50
#define PI 3.14159265358979323846
#define EULER 2.71828182845904523536

/* Codes de style (couleurs du terminal) */
#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define CYAN "\x1b[36m"

/* ---------- Etat de l'analyseur ---------- */

static char text[MAX_LINE]; /* le calcul en cours */
static int pos;             /* position de lecture dans text */
static int failed;          /* 1 des qu'une erreur est trouvee */
static int err_pos;         /* position de l'erreur (pour la fleche ^) */
static char err_msg[128];

static double last_answer = 0;
static int use_color = 0;

static char hist_text[MAX_HISTORY][MAX_LINE];
static double hist_value[MAX_HISTORY];
static int hist_count = 0;

/* Enregistre la premiere erreur rencontree et renvoie 0 pour pouvoir ecrire "return fail(...)". */
static double fail(const char *message, int at) {
    if (!failed) {
        failed = 1;
        err_pos = at;
        snprintf(err_msg, sizeof err_msg, "%s", message);
    }
    return 0.0;
}

/* Saute les espaces et retourne le caractere courant (sans l'avancer). */
static char peek(void) {
    while (text[pos] == ' ' || text[pos] == '\t') pos++;
    return text[pos];
}

/* ---------- Operations mathematiques (avec verification des cas impossibles) ---------- */

static double divide(double a, double b, int at) {
    if (b == 0) return fail("division par zéro impossible", at);
    return a / b;
}

static double factorial(double x, int at) {
    if (x < 0 || x != floor(x)) return fail("la factorielle n'existe que pour les entiers positifs", at);
    if (x > 170) return fail("trop grand : la factorielle dépasse la limite (170!)", at);
    double result = 1;
    for (int i = 2; i <= (int)x; i++) result *= i;
    return result;
}

static double power(double base, double exponent, int at) {
    if (base == 0 && exponent < 0) return fail("division par zéro impossible (0 puissance négative)", at);
    if (base < 0 && exponent != floor(exponent)) return fail("le résultat n'est pas un nombre réel", at);
    double result = pow(base, exponent);
    if (isinf(result) || isnan(result)) return fail("le résultat est trop grand pour être calculé", at);
    return result;
}

/* ---------- Analyseur : de la priorite la plus faible a la plus forte ----------
   expression : terme (+ | -) terme ...
   terme      : unaire (* | /) unaire ...
   unaire     : - unaire | puissance
   puissance  : postfixe [^ unaire]
   postfixe   : primaire [!]...
   primaire   : nombre | (expression) | |expression| | mot            */

static double parse_expr(void);

static double parse_number(void) {
    char buf[64];
    int n = 0;
    while (isdigit((unsigned char)text[pos]) && n < 60) buf[n++] = text[pos++];
    if (text[pos] == '.' || text[pos] == ',') { /* la virgule francaise est acceptee */
        buf[n++] = '.';
        pos++;
        while (isdigit((unsigned char)text[pos]) && n < 60) buf[n++] = text[pos++];
    }
    /* notation scientifique : 2e3 = 2000 (seulement si un chiffre suit le "e") */
    int sign = (text[pos + 1] == '+' || text[pos + 1] == '-');
    if (text[pos] == 'e' && (isdigit((unsigned char)text[pos + 1]) || (sign && isdigit((unsigned char)text[pos + 2])))) {
        buf[n++] = text[pos++];
        if (sign) buf[n++] = text[pos++];
        while (isdigit((unsigned char)text[pos]) && n < 62) buf[n++] = text[pos++];
    }
    buf[n] = '\0';
    if (n == 1 && buf[0] == '.') return fail("un point tout seul n'est pas un nombre", pos - 1);
    return strtod(buf, NULL);
}

static double parse_word(void) {
    char word[16];
    int n = 0, start = pos;
    while (isalpha((unsigned char)text[pos])) {
        if (n < 15) word[n++] = text[pos];
        pos++;
    }
    word[n] = '\0';

    if (!strcmp(word, "pi")) return PI;
    if (!strcmp(word, "e")) return EULER;
    if (!strcmp(word, "ans")) return last_answer;

    int is_exp = !strcmp(word, "exp");
    int is_ln = !strcmp(word, "ln");
    int is_abs = !strcmp(word, "abs");
    if (!is_exp && !is_ln && !is_abs) return fail("je ne connais pas ce mot (tape « aide »)", start);

    if (peek() != '(') return fail("il faut des parenthèses, par exemple exp(2)", pos);
    pos++;
    double x = parse_expr();
    if (failed) return 0;
    if (peek() != ')') return fail("il manque une parenthèse fermante", pos);
    pos++;

    if (is_abs) return fabs(x);
    if (is_ln) {
        if (x <= 0) return fail("ln n'est défini que pour les nombres strictement positifs", start);
        return log(x);
    }
    double result = exp(x);
    if (isinf(result)) return fail("le résultat est trop grand pour être calculé", start);
    return result;
}

static double parse_primary(void) {
    if (failed) return 0;
    char c = peek();

    if (c == '\0') return fail("le calcul est incomplet : il manque un nombre", pos);
    if (isdigit((unsigned char)c) || c == '.' || c == ',') return parse_number();
    if (isalpha((unsigned char)c)) return parse_word();

    if (c == '(') {
        pos++;
        double v = parse_expr();
        if (failed) return 0;
        if (peek() != ')') return fail("il manque une parenthèse fermante", pos);
        pos++;
        return v;
    }
    if (c == '|') { /* valeur absolue : |x - y| */
        pos++;
        double v = parse_expr();
        if (failed) return 0;
        if (peek() != '|') return fail("il manque le | de fermeture", pos);
        pos++;
        return fabs(v);
    }
    if (c == ')') return fail("il manque quelque chose avant cette parenthèse", pos);
    if (c == '*' || c == '/' || c == '^' || c == '!') return fail("il manque un nombre avant ce signe", pos);
    return fail("je ne comprends pas ce caractère", pos);
}

static double parse_postfix(void) {
    double v = parse_primary();
    while (!failed && peek() == '!') {
        int at = pos;
        pos++;
        v = factorial(v, at);
    }
    return v;
}

static double parse_unary(void);

static double parse_power(void) {
    double base = parse_postfix();
    if (!failed && peek() == '^') {
        int at = pos;
        pos++;
        double exponent = parse_unary(); /* parse_unary rappelle parse_power : 2^3^2 = 2^(3^2) */
        if (failed) return 0;
        return power(base, exponent, at);
    }
    return base;
}

static double parse_unary(void) {
    char c = peek();
    if (c == '-') {
        pos++;
        return -parse_unary();
    }
    if (c == '+') {
        pos++;
        return parse_unary();
    }
    return parse_power();
}

static double parse_term(void) {
    double v = parse_unary();
    while (!failed) {
        char c = peek();
        int at = pos;
        if (c == '*') {
            pos++;
            v *= parse_unary();
        } else if (c == '/') {
            pos++;
            double d = parse_unary();
            if (failed) return 0;
            v = divide(v, d, at);
        } else {
            break;
        }
    }
    return v;
}

static double parse_expr(void) {
    double v = parse_term();
    while (!failed) {
        char c = peek();
        if (c == '+') {
            pos++;
            v += parse_term();
        } else if (c == '-') {
            pos++;
            v -= parse_term();
        } else {
            break;
        }
    }
    return v;
}

/* Calcule une expression. Retourne 1 si tout va bien (resultat dans *result), 0 sinon. */
static int evaluate(const char *line, double *result) {
    int n = 0;
    for (; line[n] && n < MAX_LINE - 1; n++) text[n] = (char)tolower((unsigned char)line[n]);
    text[n] = '\0';
    pos = 0;
    failed = 0;

    double v = parse_expr();
    if (!failed && peek() != '\0') {
        if (text[pos] == ')') fail("parenthèse fermante en trop", pos);
        else fail("il manque un opérateur (+ - * / ^) avant ceci", pos);
    }
    if (!failed && (isnan(v) || isinf(v))) fail("le résultat est trop grand pour être calculé", 0);
    if (failed) return 0;
    *result = v;
    return 1;
}

/* ---------- Affichage ---------- */

static void style(const char *code) {
    if (use_color) fputs(code, stdout);
}

/* Entiers : "3628800" -> "3 628 800" ; sinon 10 chiffres significatifs au maximum. */
static void format_number(double v, char out[], int size, int group) {
    if (fabs(v) < 1e15 && v == floor(v)) {
        char digits[32];
        snprintf(digits, sizeof digits, "%.0f", fabs(v));
        int len = (int)strlen(digits), n = 0;
        if (v < 0) out[n++] = '-';
        for (int i = 0; i < len; i++) {
            if (group && len >= 5 && i > 0 && (len - i) % 3 == 0) out[n++] = ' ';
            out[n++] = digits[i];
        }
        out[n] = '\0';
    } else {
        snprintf(out, (size_t)size, "%.10g", v);
    }
}

static void show_result(double v) {
    char buf[64];
    format_number(v, buf, sizeof buf, 1);
    style(DIM);
    printf("    =");
    style(RESET);
    printf(" ");
    style(BOLD GREEN);
    printf("%s", buf);
    style(RESET);
    printf("\n");
}

static void show_error(const char *line) {
    style(DIM);
    printf("    %s\n", line);
    style(RESET RED);
    printf("    %*s^ %s\n", err_pos, "", err_msg);
    style(RESET);
}

static void show_banner(void) {
    printf("\n  ");
    style(BOLD);
    printf("Calculatrice");
    style(RESET);
    printf("\n  ");
    style(DIM);
    printf("─────────────────────────────────────────\n");
    style(RESET);
    printf("  Tape un calcul, puis Entrée. Par exemple :\n\n");
    printf("    5! + |3 - 9|            2^10\n");
    printf("    exp(2) * ln(10)         (4 + 6) / 4\n\n");
    style(DIM);
    printf("  aide · historique · effacer · quitter\n");
    style(RESET);
}

static void show_help(void) {
    printf("\n  Opérations\n");
    printf("    + - * /       les quatre opérations      12 / 5 = 2.4\n");
    printf("    x^y           puissance                  2^10 = 1 024\n");
    printf("    n!            factorielle                5! = 120\n");
    printf("    |x - y|       valeur absolue             |3 - 9| = 6\n");
    printf("    exp(x)        exponentielle              exp(1) = 2.718281828\n");
    printf("    ln(x)         logarithme népérien        ln(10) = 2.302585093\n");
    printf("\n  Le reste\n");
    printf("    pi, e         constantes\n");
    printf("    ans           ton dernier résultat       ans * 2\n");
    printf("    ( )           parenthèses                (2 + 3) * 4 = 20\n");
    printf("    2,5 ou 2.5    les deux écritures marchent\n");
}

static void show_history(void) {
    if (hist_count == 0) {
        printf("\n  Rien dans l'historique pour l'instant.\n");
        return;
    }
    printf("\n");
    for (int i = 0; i < hist_count; i++) {
        char buf[64];
        format_number(hist_value[i], buf, sizeof buf, 1);
        printf("  %2d.  %-28s = %s\n", i + 1, hist_text[i], buf);
    }
}

static void remember(const char *line, double value) {
    if (hist_count == MAX_HISTORY) { /* plein : on oublie le plus ancien */
        for (int i = 1; i < MAX_HISTORY; i++) {
            memcpy(hist_text[i - 1], hist_text[i], MAX_LINE);
            hist_value[i - 1] = hist_value[i];
        }
        hist_count--;
    }
    snprintf(hist_text[hist_count], MAX_LINE, "%s", line);
    hist_value[hist_count] = value;
    hist_count++;
}

/* Enleve les espaces autour et passe en minuscules. */
static void clean(char line[]) {
    int start = 0, end = (int)strlen(line);
    while (end > start && isspace((unsigned char)line[end - 1])) end--;
    while (start < end && isspace((unsigned char)line[start])) start++;
    int n = 0;
    for (int i = start; i < end; i++) line[n++] = (char)tolower((unsigned char)line[i]);
    line[n] = '\0';
}

int main(int argc, char **argv) {
    double value;

    /* Mode rapide : ./calc "5! + 2" affiche juste le resultat (pratique pour les tests). */
    if (argc > 1) {
        char joined[MAX_LINE] = "";
        for (int i = 1; i < argc; i++) {
            if (strlen(joined) + strlen(argv[i]) + 2 >= MAX_LINE) break;
            strcat(joined, argv[i]);
            strcat(joined, " ");
        }
        if (!evaluate(joined, &value)) {
            fprintf(stderr, "Erreur : %s\n", err_msg);
            return 1;
        }
        char buf[64];
        format_number(value, buf, sizeof buf, 0);
        puts(buf);
        return 0;
    }

    use_color = isatty(STDOUT_FD) && !getenv("NO_COLOR");
#ifdef _WIN32
    SetConsoleOutputCP(65001); /* UTF-8 pour les accents et les traits */
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    if (GetConsoleMode(console, &mode)) SetConsoleMode(console, mode | 0x0004); /* couleurs ANSI */
#endif

    show_banner();

    char line[MAX_LINE];
    for (;;) {
        printf("\n  ");
        style(CYAN);
        printf("›");
        style(RESET);
        printf(" ");
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) break; /* Ctrl+D / fin du fichier */
        if (!strchr(line, '\n')) { /* ligne trop longue : on jette la suite */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {}
            printf("    Ligne trop longue (maximum %d caractères).\n", MAX_LINE - 2);
            continue;
        }
        clean(line);
        if (line[0] == '\0') continue;

        if (!strcmp(line, "quitter") || !strcmp(line, "exit") || !strcmp(line, "q")) break;
        if (!strcmp(line, "aide") || !strcmp(line, "help") || !strcmp(line, "?")) {
            show_help();
        } else if (!strcmp(line, "historique") || !strcmp(line, "history")) {
            show_history();
        } else if (!strcmp(line, "effacer") || !strcmp(line, "clear")) {
            hist_count = 0;
            last_answer = 0;
            if (use_color) printf("\x1b[2J\x1b[H");
            show_banner();
        } else if (evaluate(line, &value)) {
            show_result(value);
            last_answer = value;
            remember(line, value);
        } else {
            show_error(line);
        }
    }

    printf("\n  À bientôt !\n\n");
    return 0;
}
