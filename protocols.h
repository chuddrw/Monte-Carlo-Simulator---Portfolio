#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_VARS 5
#define MSG_HEARTBEAT 'H'
#define MSG_RESULT    'R'
#include "distributions.h"

typedef struct Task {
    long num_samples;
    char formula[256];      // e.g. "x*y^2"
    VarDist vars[MAXVAR];   // each variable with its distribution + params
    int num_vars;
} Task;

typedef struct {
    long trials;
    double sum;
    double sum_sq;
} Result;

VarDist make_vardist(char var, int dist, double param1, double param2);
Result make_result(long trials, double sum, double sum_sq);
Task make_task(long num_samples, char *formula, VarDist *vars, int num_vars);


#endif