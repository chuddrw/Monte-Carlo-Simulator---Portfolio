#include <string.h>
#include "protocols.h"
#include "distributions.h"
// added #include <string.h> 
Task make_task(long num_samples, char *formula, VarDist *vars, int num_vars) {
    Task t;
    memset(&t, 0, sizeof(Task));
    t.num_samples = num_samples;
    strncpy(t.formula, formula, sizeof(t.formula) - 1);
    t.num_vars = num_vars;
    memcpy(t.vars, vars, num_vars * sizeof(VarDist));
    return t;
}

// Initialize a VarDist:
VarDist make_vardist(char var, int dist, double param1, double param2) {
    VarDist new_vardist = {var, dist, param1, param2};
    return new_vardist;
}

Result make_result(long trials, double sum, double sum_sq) {
    Result r;
    r.trials = trials;
    r.sum = sum;
    r.sum_sq = sum_sq;
    return r;
}