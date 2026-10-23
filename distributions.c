#include <stdlib.h>
#include <math.h>
#include "distributions.h"

// Continuous Distributions
// U[0, 1]
double uniform_01(void) {
    return rand() / (double)RAND_MAX;
}

// U[a, b]
double uniform_ab(double a, double b) {
    return a + (b - a) * uniform_01();
}

double normal(double mean, double sd) {
    double u1 = uniform_01();
    double u2 = uniform_01();
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + sd * z;
}

double exponential(double lambda) {
    return -log(1.0 - uniform_01()) / lambda;
}

// Discrete Distributions
int bernoulli(double p) {
    if (uniform_01() < p) { return 1;} 
    else {return 0;}
}

int binomial(int n, double p) {
    int successes = 0;
    for (int i = 0; i < n; i++)
        if (uniform_01() < p) successes++;
    return successes;
}

int poisson(double lambda) {
    // For small lambda:
    if (lambda <= 30) {
        double L = exp(-lambda);
        double p = 1.0;
        int k = 0;
    
    // returns k-1 because k is incremented one final time
    // on the iteration that causes the loop to stop
        do {
            k++;
            p *= uniform_01();
        } while (p > L);

        return k - 1;
    }

    // else:
    // For big lambda, can approximate with normal
    double val = normal(lambda, sqrt(lambda));
    if (val < 0) {return 0;}
    else {return (int)round(val);}
}

