#ifndef DISTRIBUTIONS_H
#define DISTRIBUTIONS_H
#define MAXLINE 256
#define MAXVAR 5

double uniform_01(void); // internal use only
double uniform_ab(double a, double b);
double normal(double mean, double sd);
double exponential(double lambda);
int bernoulli(double p);
int binomial(int n, double p);
int poisson(double lambda);

typedef struct VariableDistribution {
    char     variable;   // the character name, e.g. 'x'
    int      distribution; // the distribution, indicated by number
    double   param1; // the parameter 1, if it exists
    double   param2; // the parameter 2, if it exists, otherwise it's -1
} VarDist;

VarDist make_vardist(char var, int dist, double param1, double param2);

#endif
