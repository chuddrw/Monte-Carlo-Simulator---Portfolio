


// receives "Task" from parent
// samples a number from distribution
// worker calls evaluate() after it sampled values from distributions
// accumulate local sum


// send a "Result" to parent via pipe
// include in result number of samples it ran (cuz it might've crashed)
// so we want to tell parent how many samples were actually ran


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "distributions.h"
#include "protocols.h"
#include "evaluate.h"

//idea: the parent will fork() and exec("./worker") (this file) for every worker in the pool (yay thanks christina)
// the worker reads one Task from stdin, runs its simulation, then writes one Result to stdout and exits

/*
 * distribution type codes (must match what the parent sends:
 *   0 = normal       params: mean, sd
 *   1 = uniform(0,1)      params:none
 *   2 = exp  params: lambda
 *   3 = bernoulli    params: p
 *   4 = binomial     params: n, p
 *   5 = poisson      params: lambda
 */

//Draw one sample from the distribution described by dist_type/param1/param2
// Returns the sample as a double
static double sample_variable(int dist_type, double param1, double param2) {
    switch (dist_type) {
        //sample variables using the distributions from Task 1 from the .c and .h files (yay thanks jenny)
        case 1: return uniform_01();
        case 2:     // Uniform(min, max): scale the [0,1) draw
                return uniform_ab(param1, param2);  //TODO: this has changed, pull updated code and fix this line
        case 3: return normal(param1, param2);
        case 4: return exponential(param1);
        case 5: return (double)bernoulli(param1);
        case 6: return (double)binomial((int)param1, param2);
        case 7: return (double)poisson(param1);
        default:
            //fprintf(stderr, "worker: unknown dist_type %d\n", dist_type);
            exit(1);
    }
  
}


int main(void) {
    // seed the RNG differently for every worker by mixing pid + time
    // without this every forked child would start with the same seed
    srand((unsigned int)(time(NULL) ^ (unsigned int)getpid()));

    Task task;
    ssize_t bytes_read;

    // loop: read tasks from parent until shutdown or EOF
    while ((bytes_read = read(STDIN_FILENO, &task, sizeof(Task))) > 0) {
        if (bytes_read != sizeof(Task)) {
            perror("worker: partial read");
            exit(1);
        }

        // num_samples=0 is the shutdown signal from parent
        if (task.num_samples == 0) {
            //fprintf(stderr, "worker %d: received shutdown signal\n", getpid());
            exit(0);
        }

        //fprintf(stderr, "worker %d: got task with num_samples=%ld num_vars=%d formula=%s\n",
        //    getpid(), task.num_samples, task.num_vars, task.formula);

        // basic checks
        if (task.num_vars <= 0 || task.num_vars > MAX_VARS) {
            fprintf(stderr, "worker: invalid num_vars %d\n", task.num_vars);
            exit(1);
        }

        // run the Monte Carlo chunk
        Result result;
        result.trials  = 0;
        result.sum     = 0.0;
        result.sum_sq  = 0.0;

        double sampled_vars[MAX_VARS];

        for (long s = 0; s < task.num_samples; s++) {
            for (int v = 0; v < task.num_vars; v++) {
                sampled_vars[v] = sample_variable(task.vars[v].distribution,
                                                task.vars[v].param1,
                                                task.vars[v].param2);
            }

            char var_names[MAX_VARS + 1];
            for (int i = 0; i < task.num_vars; i++) {
                var_names[i] = task.vars[i].variable;
            }
            var_names[task.num_vars] = '\0';

            double val = evaluate(task.formula, sampled_vars, var_names);

            result.sum    += val;
            result.sum_sq += val * val;
            result.trials++;

            // send a running signal "heartbeat" to parent every 500 samples to indicate it's still running
            if (s > 0 && s % 500 == 0) {
                char htag = MSG_HEARTBEAT;
                write(STDOUT_FILENO, &htag, 1);
                write(STDOUT_FILENO, &result, sizeof(Result));
            }
        }

        // send Result back to parent over stdout, the write end of the pipe
        //fprintf(stderr, "worker %d: finished simulation, writing result\n", getpid());

        char tag = MSG_RESULT;
        //ssize_t tag_written = write(STDOUT_FILENO, &tag, 1);
	    write(STDOUT_FILENO, &tag, 1);
        //fprintf(stderr, "worker %d: tag written: %zd\n", getpid(), tag_written);

        ssize_t bytes_written = write(STDOUT_FILENO, &result, sizeof(Result));
        //fprintf(stderr, "worker %d: result written: %zd\n", getpid(), bytes_written);

        if (bytes_written != sizeof(Result)) {
            perror("worker: write result");
            exit(1);
        }
    }

    // bytes_read == 0 means parent closed the pipe
    exit(0);
}
