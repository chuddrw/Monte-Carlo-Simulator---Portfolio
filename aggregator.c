#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "protocols.h"
#include "distributions.h"
#include "evaluate.h"

int main(void) {
    long total_trials = 0;
    double total_sum = 0.0;
    double total_sum_sq = 0.0;

    Result r;
    ssize_t bytes_read;

    // keep reading Results from stdin until EOF (parent closes the pipe)
    while ((bytes_read = read(STDIN_FILENO, &r, sizeof(Result))) > 0) {
        if (bytes_read != sizeof(Result)) {
            fprintf(stderr, "aggregator: partial read, expected %zu got %zd\n", sizeof(Result), bytes_read);
            exit(1);
        }
        total_trials += r.trials;
        total_sum    += r.sum;
        total_sum_sq += r.sum_sq;
    }

    if (bytes_read == -1) {
        perror("aggregator: read");
        exit(1);
    }

    // compute final estimates (for debugging purpose only)
    // double mean     = total_sum / total_trials;
    // double mean_sq  = total_sum_sq / total_trials;
    // double variance = mean_sq - mean * mean;  // E[X^2] - E[X]^2

    // write a final sum "result" back to parent, this Result contains the total sum
    // across all workers
    Result final = make_result(total_trials, total_sum, total_sum_sq);
    ssize_t bytes_written = write(STDOUT_FILENO, &final, sizeof(Result));
    if (bytes_written != sizeof(Result)) {
        perror("aggregator: write");
        exit(1);
    }

    // print final estimates to stderr for debugging purpose
    // fprintf(stderr, "aggregator: trials=%ld\n", total_trials);
    // fprintf(stderr, "aggregator: E[f]   = %.6f\n", mean);
    // fprintf(stderr, "aggregator: E[f^2] = %.6f\n", mean_sq);
    // fprintf(stderr, "aggregator: Var[f] = %.6f\n", variance);

    return 0;
}