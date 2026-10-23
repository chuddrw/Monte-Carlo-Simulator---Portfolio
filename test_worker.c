#include <stdio.h>
#include <unistd.h>
#include "distributions.h"
#include "protocols.h"

int main(void) {
    // build a Task to send to the worker.
    // we use one variable, uniform(0, 1), so the expected value of f(x) = x is exactly 0.5 and E[x^2] = 1/3
    // making this easy to verify
    Task dummy_task;

    dummy_task.num_samples = 1000;
    strcpy(dummy_task.formula, "x*y^2 + z");
    dummy_task.num_vars = 3;

    // fill vars using make_vardist
    dummy_task.vars[0] = make_vardist('x', 1, 0.0, 1.0);  // Uniform(0,1)
    dummy_task.vars[1] = make_vardist('y', 3, 0.0, 1.0);  // Normal(0,1)
    dummy_task.vars[2] = make_vardist('z', 7, 5.0, -1.0); // Poisson(5)
    

    // create two pipes:
    // - to_worker: parent writes the Task, worker reads it via stdin
    // - from_worker: worker writes the Result, parent reads it via stdout
    int to_worker[2], from_worker[2];
    pipe(to_worker);
    pipe(from_worker);

    if (fork() == 0) {
        ////////// child
        // wire up the pipes
        dup2(to_worker[0], STDIN_FILENO);    // stdin  <- read end of to_worker
        dup2(from_worker[1], STDOUT_FILENO); // stdout -> write end of from_worker

        // close the ends we no longer need after dup2
        close(to_worker[1]);
        close(from_worker[0]);

        // replace this child process with the worker
        // the worker will block on read(STDIN_FILENO) until the parent writes the Task below
        execl("./worker", "worker", NULL);
        perror("execl failed");  // execl only returns if it failed
        exit(1);
    }

    ///////// parent
    // close the ends the parent doesn't use; we only write to to_worker and read from from_worker.
    close(to_worker[0]);
    close(from_worker[1]);

    // send the Task struct to the worker over the pipe
    // the worker is blocked while waiting for this
    write(to_worker[1], &dummy_task, sizeof(Task));

    // close the write end so the worker sees EOF if it reads again and so it isn't left hanging
    close(to_worker[1]);

    // block until the worker writes its Result back
    Result r;
    // read(from_worker[0], &r, sizeof(Result));
    ssize_t bytes = read(from_worker[0], &r, sizeof(Result));
    printf("bytes read: %ld\n", bytes);

    // Print and verify.
    // With uniform(0,1) and the current dummy evaluate_formula summing variables:
    // - E[x]   = 0.5    (exact)
    // - E[x^2] = 0.333  (exact)
    // so if the output is close to these values, the worker pipeline is correct.
    printf("trials: %ld\n", r.trials);
    printf("estimate: %f\n", r.sum / r.trials);        // should be ~0.5
    printf("sum_sq estimate: %f\n", r.sum_sq / r.trials); // should be ~0.333
}

//wolf:~/sing2838/group_51548/a3$ gcc -o worker worker.c distributions.c -lm
//wolf:~/sing2838/group_51548/a3$ gcc -o test_worker test_worker.c -lm
//wolf:~/sing2838/group_51548/a3$ ./test_worker
//trials: 1000000
//estimate: 0.500036
//sum_sq estimate: 0.333292
//wolf:~/sing2838/group_51548/a3$ ./test_worker
//trials: 1000000
//estimate: 0.500299
//sum_sq estimate: 0.333633
//looks correct! yayyy