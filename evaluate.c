// evaluate.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "evaluate.h"

/* HELPER DECLARATIONS: */
// these will be used to handle different parts of evaluation
static double expression(char *formula, int *pos, double *values, char *vars);
static double term(char *formula, int *pos, double *values, char *vars);
static double power(char *formula, int *pos, double *values, char *vars);
static double primary(char *formula, int *pos, double *values, char *vars);


/* Given a formula and current position pointer, move the position pointer to next if space is encountered */
static void skip_spaces(char *formula, int *pos) {
    while (formula[*pos] == ' ') {
        (*pos)++;
    }
}

/* This function is the entry point of formula evaluation, it calls expression() at position 0 */
double evaluate(char *formula, double *values, char *vars) {
    int pos = 0;
    return expression(formula, &pos, values, vars);
}

/* Handles  + and - (lowest precendence)*/
static double expression(char *formula, int *pos, double *values, char *vars) {
    double left = term(formula, pos, values, vars);
    skip_spaces(formula, pos);

    while (formula[*pos] == '+' || formula[*pos] == '-') {
        char op = formula[*pos];
        (*pos)++;
        skip_spaces(formula, pos);
        double right = term(formula, pos, values, vars);
        if (op == '+') left = left + right;
        else           left = left - right;
        skip_spaces(formula, pos);
    }
    return left;
}

// handles * and /
static double term(char *formula, int *pos, double *values, char *vars) {
    double left = power(formula, pos, values, vars);
    skip_spaces(formula, pos);

    while (formula[*pos] == '*' || formula[*pos] == '/') {
        char op = formula[*pos];
        (*pos)++;
        skip_spaces(formula, pos);
        double right = power(formula, pos, values, vars);
        if (op == '*') left = left * right;
        else {
            if (right == 0.0) {
                fprintf(stderr, "Error: division by zero\n");
                exit(1);
            }
            left = left / right;
        }
        skip_spaces(formula, pos);
    }
    return left;
}

// handles ^ (right-associative)
static double power(char *formula, int *pos, double *values, char *vars) {
    double base = primary(formula, pos, values, vars);
    skip_spaces(formula, pos);

    if (formula[*pos] == '^') {
        (*pos)++;
        skip_spaces(formula, pos);
        double exp = power(formula, pos, values, vars); // recursive for right-associativity
        return pow(base, exp);
    }
    return base;
}

// handles numbers, variables, and parentheses (highest precedence)
static double primary(char *formula, int *pos, double *values, char *vars) {
    skip_spaces(formula, pos);

    // parentheses — recurse inside
    if (formula[*pos] == '(') {
        (*pos)++;  // skip (
        double result = expression(formula, pos, values, vars);
        skip_spaces(formula, pos);
        if (formula[*pos] != ')') {
            fprintf(stderr, "Error: missing closing parenthesis\n");
            exit(1);
        }
        (*pos)++;  // skip )
        return result;
    }

    // variable — look up its sampled value
    if (isalpha(formula[*pos])) {
        char var = formula[*pos];
        (*pos)++;
        char *found = strchr(vars, var);
        if (found == NULL) {
            fprintf(stderr, "Error: unknown variable '%c'\n", var);
            exit(1);
        }
        int idx = found - vars;  // index into values array
        return values[idx];
    }

    // number — read it directly from the string
    if (isdigit(formula[*pos]) || formula[*pos] == '.') {
        char *end;
        double num = strtod(formula + *pos, &end);
        *pos += end - (formula + *pos);  // advance pos by how many chars were consumed
        return num;
    }

    fprintf(stderr, "Error: unexpected character '%c'\n", formula[*pos]);
    exit(1);
}