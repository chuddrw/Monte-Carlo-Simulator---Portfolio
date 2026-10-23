#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "distributions.h"
#include "evaluate.h"
#include "protocols.h"

#define MAXLINE 256
#define MAXVAR 5
#define MAXPARAM 17
#define WORKERS 5 // total worker number
#define CHUNK 1000 // a chunk of work

char allowed_variables[] = "xyzwv"; // read only

/* HELPER DECLARATIONS: */
void to_lowercase(char *str); // won't be used by main, it's a helper for helper parse_variables
void parse_variables(char *formula, char *variable_list);
void get_distributions(char* variables, VarDist *vardist_list, int *vardist_list_length);
// +errorcheck added an argument to address aggregator pipe leak, also added a line in function defintion
void spawn_process(const char *executable, int to_child[2], int from_child[2], pid_t *pid, int agg_write_fd);


/* MAIN FUNCTION BLOCK: */
int main(void) {
  long N; // store number of simulations to run
  char formula[MAXLINE];
  char all_variables[MAXVAR+1];
  int vardist_list_length = 0; // store length of vardist
  VarDist all_vardist[MAXVAR+1]; // store all parsed var dist in here 
  // assume all_var_dist has all the parsed info, see distributions.h for struct VarDist definition

  all_variables[0] = '\0'; // initialize all_variables list

  /* 1. Get variables and distributions from user, parse them, store them in all_vardist and all_variables */
  printf("%s\n", "Please enter a formula (only x, y, z, w, v are allowed as variables): ");
  
  if(fgets(formula, MAXLINE, stdin) == NULL) {
      perror("fgets didn't read anything!");
      exit(1);
  }

  int has_var = 0;
  for (int i = 0; formula[i]; i++) {
    if (isalpha(formula[i])) {
      has_var = 1;
      break;
    }
  }
  if (!has_var) {
      fprintf(stderr, "Invalid formula; at least one variable must be present\n");
      exit(1);
  }

  parse_variables(formula, all_variables); // store result in all_variables
  // printf("before get distribution");
  get_distributions(all_variables, all_vardist, &vardist_list_length); // store result in all_vardist
  // printf("Get distribution was done");

  /* 2. Get number of simulation from user, N */
  printf("%s", "Enter number of simulations: ");
  if (scanf("%ld", &N) != 1 || N <= 0) {
      fprintf(stderr, "Invalid number of simulations\n");
      exit(1);
  }

  /* 3. Make template task to send to workers using parsed info stored above */ 
  Task template = make_task(N, formula, all_vardist, vardist_list_length);
  

  /* Step 4. Make aggregator child (the child that collects all Results and 
  compute final estimate so parent doesn't do final math) before worker pool */
  int to_aggregator[2]; // to make pipe to send stuff to aggregator
  int from_aggregator[2]; // to make pipe to receive stuff from aggregator
  pid_t aggregator_pid;

  spawn_process("./aggregator", to_aggregator, from_aggregator, &aggregator_pid, -1); // spawn aggregator process
  // +errorcheck, updated spawn_process call with final parameter -1


  /* 5. Set up worker pool */
  long int remaining_num = N; // remaining number of samples
  long int workers_num;
  int total_tasks_dispatched = 0;
  int total_tasks_done = 0;

  if (N < WORKERS) { workers_num = N; } // shouldnt be more workers than samples
  else { workers_num = WORKERS; } 

  // the pids:
  pid_t pids[workers_num]; // to track each worker

  // the pipes:
  int to_worker[workers_num][2];
  int from_worker[workers_num][2];

  // busy workers list:
  int busy[workers_num];

  Task last_task[workers_num]; // the last task each worker was sent, used for worker replacement
  // if worker i crashes, you know what task it was running by looking at last_task[i]

   // initializes the busy array to all zeros
  memset(busy, 0, sizeof(busy));

  // spawn workers
  for (int i = 0; i < workers_num; i++) {
      spawn_process("./worker", to_worker[i], from_worker[i], &pids[i], to_aggregator[1]);
      // +errorcheck added argument to spawn_process
  }

  /* 6. Initial dispatch of work to workers */
  for (int i = 0; i < workers_num && remaining_num > 0; i++) {
      Task t = template; // make a template task
      if ((remaining_num < CHUNK)) {t.num_samples = remaining_num; } 
      else {t.num_samples = CHUNK; } // change number of samples sent to worker
      remaining_num -= t.num_samples; // reduce number of remaining number of samples

      if (write(to_worker[i][1], &t, sizeof(Task)) != sizeof(Task)) { // write to worker with the Task
          perror("write initial task");
          exit(1);
      }

      busy[i] = 1; // change index of this ith worker in busy to 1 to indicate it's busy
      last_task[i] = t; // change task sent to this ith worker in last_task to task t
      total_tasks_dispatched++; // increment total tasks dispatched by 1
  }

  /* 7. Select loop */
  while (total_tasks_done < total_tasks_dispatched || remaining_num > 0) {
      /* Crash Recovery*/
      // Check all busy workers with WNOHANG before calling select
      // If a worker has died, close its pipes, respawn it, resend its last task
      for (int i = 0; i < workers_num; i++) {
          // if worker idle, go to next one cuz don't need to check if idle worker crashed
          if (!busy[i]) continue;

          int status;
          pid_t result = waitpid(pids[i], &status, WNOHANG); // asks whether child died (WNOHANG prevents blocking), 0 if worker is fine
          if (result == -1) { perror("waitpid"); exit(1); } //+errorcheck adding check for -1, where pid itself failed
          if (result > 0) { // if crashed
              //fprintf(stderr, "worker %d crashed, respawning...\n", i); // debug purpose
              close(to_worker[i][1]);
              close(from_worker[i][0]);

              // spawn a worker — pass the specific child's pipe arrays to rewire to worker process
              spawn_process("./worker", to_worker[i], from_worker[i], &pids[i], to_aggregator[1]);
              // +errorcheck added argument to spawn_process

              if (write(to_worker[i][1], &last_task[i], sizeof(Task)) != sizeof(Task)) {
                  perror("write recovery task");
                  exit(1);
              }
              // else, == 0: busy[i] stays 1, still working on same task
          }
      }

      // build fd_set of busy workers to watch
      fd_set set;
      FD_ZERO(&set);
      int maxfd = -1;

      for (int i = 0; i < workers_num; i++) {
          if (busy[i]) {
              FD_SET(from_worker[i][0], &set);
              if (from_worker[i][0] > maxfd) {
                  maxfd = from_worker[i][0];
              }
          }
      }

      // if (maxfd == -1) continue; // no busy workers yet, loop again
      // +errorcheck, continue could cause infinite loop
      if (maxfd == -1) {
          fprintf(stderr, "no busy workers but work remains\n");
          break;
      }

      if (select(maxfd + 1, &set, NULL, NULL, NULL) == -1) {
          perror("select");
          exit(1);
      }

      for (int i = 0; i < workers_num; i++) {
          if (!busy[i] || !FD_ISSET(from_worker[i][0], &set)) continue;
          /* Distinguishing progress report from final result */
          char tag;
          ssize_t tag_bytes = read(from_worker[i][0], &tag, 1);
          //fprintf(stderr, "parent: read tag '%c' (%d) from worker %d\n", tag, (int)tag, i);
          if (tag_bytes == 0) {
            fprintf(stderr, "worker %d closed pipe unexpectedly\n", i);
            // respawn new worker and resend the last task (busy[i] doesn't change to 0 since task is incomplete)
            close(from_worker[i][0]);
            close(to_worker[i][1]);
            spawn_process("./worker", to_worker[i], from_worker[i], &pids[i], to_aggregator[1]);
            if (write(to_worker[i][1], &last_task[i], sizeof(Task)) != sizeof(Task)) {
                perror("write recovery task");
                exit(1);
            }
            // busy[i] stays 1, task is being retried
            continue;
          }
          if (tag_bytes != 1) {
              perror("read tag");
              exit(1);
          }

          Result r;
          if (read(from_worker[i][0], &r, sizeof(Result)) != sizeof(Result)) {
              perror("read result");
              exit(1);
          }

          /* Running signal heartbeat from worker to parent while working, print to user
          so user knows the simulation is running normally */
          if (tag == MSG_HEARTBEAT) {
              printf("worker %d running...\n", i);

          } else if (tag == MSG_RESULT) {
              // Info sent back is final result, s.t. worker is done with its task.
              busy[i] = 0;
              total_tasks_done++;

              /* Result sent to aggregator child instead of accumulating here in parent */
              if (write(to_aggregator[1], &r, sizeof(Result)) != sizeof(Result)) {
                  perror("write to aggregator");
                  exit(1);
              }

              // redispatch to this worker if there is remaining work
              if (remaining_num > 0) {
                  Task t = template;
                  if (remaining_num < CHUNK) { t.num_samples = remaining_num; }
                  else {t.num_samples = CHUNK;}
                  remaining_num -= t.num_samples;

                  if (write(to_worker[i][1], &t, sizeof(Task)) != sizeof(Task)) {
                      perror("write redispatch");
                      exit(1);
                  }
                  busy[i] = 1;
                  last_task[i] = t;
                  total_tasks_dispatched++;
                  FD_CLR(from_worker[i][0], &set); // remove from set so we don't read again this iteration
              }
          } else {
              //fprintf(stderr, "parent: unknown tag '%c' (%d) from worker %d\n", tag, (int)tag, i);
              exit(1);
          }
      }

      /* After processing all ready workers this iteration, fill any idle workers if work remains. */
      // This ensures workers that finished earlier but didn't get redispatched above get new tasks.
      for (int i = 0; i < workers_num && remaining_num > 0; i++) {
          if (busy[i]) continue; // skip busy ones

          Task t = template;
          if (remaining_num < CHUNK) {t.num_samples = remaining_num;} 
          else {t.num_samples = CHUNK; }
          remaining_num -= t.num_samples;

          if (write(to_worker[i][1], &t, sizeof(Task)) != sizeof(Task)) {
              perror("write fill idle worker");
              exit(1);
          }
          busy[i] = 1;
          last_task[i] = t;
          total_tasks_dispatched++;
      }
  }


  /* 8: After no more tasks, shutdown workers and collect children */
  for (int i = 0; i < workers_num; i++) {
      Task t;
      memset(&t, 0, sizeof(Task)); // num_samples=0 signals shutdown
      // write(to_worker[i][1], &t, sizeof(Task));
      // +errorcheck
      if (write(to_worker[i][1], &t, sizeof(Task)) != sizeof(Task)) {
          perror("write shutdown task");
          exit(1);
      }
      close(to_worker[i][1]);
      close(from_worker[i][0]);
      // waitpid(pids[i], NULL, 0);
      // +errorcheck
      if (waitpid(pids[i], NULL, 0) == -1) {
          perror("waitpid shutdown");
          exit(1);
      }
  }

  
  /* 9: shut down aggregator, which computes and writes back final sum 
  (a Result struct), which is then read by parent */ 
  close(to_aggregator[1]);

  Result final_sum;
  if (read(from_aggregator[0], &final_sum, sizeof(Result)) != sizeof(Result)) {
      perror("read final sum from aggregator");
      exit(1);
  }
  if (final_sum.trials == 0) {
      fprintf(stderr, "aggregator returned zero trials\n");
      exit(1);
  }
  
  // Compute final estimate by dividing final sum by number of trials done
  double estimate = final_sum.sum / (double)final_sum.trials; // E[X]
  double mean_squared  = final_sum.sum_sq / final_sum.trials; // E[X^2]
  double variance = mean_squared - (estimate * estimate);  // E[X^2] - E[X]^2

  // double estimate = final_result.sum / final_result.trials; +errorcheck, casting and additional check
  close(from_aggregator[0]);
  // waitpid(aggregator_pid, NULL, 0);
  // +errorcheck
  if (waitpid(aggregator_pid, NULL, 0) == -1) {
      perror("waitpid aggregator");
      exit(1);
  }
  printf("\n------------- Results -------------\n");
  printf("Number of trials ran=%ld\n", final_sum.trials);
  printf("E[f] = %f\n", estimate);
  printf("E[f^2] = %.6f\n", mean_squared);
  printf("Var[f] = %.6f\n", variance);
  
  return 0;
}



/* HELPER DEFINITIONS: */ 
void to_lowercase(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        str[i] = tolower(str[i]);
    }
}

/* Given string of formula, return a list of variables*/
// var_dist_list will be NULL terminated
void parse_variables(char *formula, char *variable_list) {
    to_lowercase(formula); // turn formula to lowercase
    // printf("%s", formula); // check it's been lowercased
    int formula_length = strlen(formula);
    int variable_list_length = 0;
    
    // loop through formula string, get variables, put in variable_list
    for (int i = 0; i < formula_length; i++) {
        // is the current character an alphabet?
        if (isalpha(formula[i])) { 
          // it's an alphabet
    
          // is the alphabet one of the allowed variables?
          if (strchr(allowed_variables, formula[i]) == NULL) {
              // it's not an allowed variable
              fprintf(stderr, "Please only use x, y, z, w, v, as variables.\n");
              exit(1); // we can standardize exit codes later (?)
          }
          
          // it's an allowed variable, so check if it was already added into variable_list
          if (strchr(variable_list, formula[i]) == NULL) {
              // it's not in the variable list, so we add it
              variable_list[variable_list_length] = formula[i]; // "a,b,c,\0" will return 3, we want to insert at index 3
              variable_list[variable_list_length + 1] = '\0'; // strlen only works if u null terminate the string!! so u need to null terminate right after!!!!
              // this automatically handles null terminate at end of list
              variable_list_length++;
          }
        } // it's not an alphabet, do nothing, go to next char
      }

    // check that it worked:
    printf("%s", "Your variables: ");
    for (int i = 0; i < variable_list_length; i++) {
        printf("%c, ", variable_list[i]);
    }
  }

void get_distributions(char* variables, VarDist *vardist_list, int *vardist_list_length) {
  // Give user menu of options, ask user for distribution of each variable:
  printf("%s\n", "\n-------------");
  printf("%s\n", "Distribution Options: \n"
                "Continuous: (1) Uniform(0,1), (2) Uniform(a,b), (3) Normal(mean, std), (4) Exponential(lambda) \n"
                "Discrete: (5) Bernoulli(p), (6) Binomial(n, p), (7) Poisson(lambda) \n");
  
  printf("%s\n", "Please enter the number (1-7) of the distribution of your choice for each variable listed below.\n");
  // Loop through variables, make VarDist, add into vardist_list
  int var_index = 0;
  int i = 0;
            
  while(variables[var_index] != '\0') {
      long int curr_distribution = -1;
      double curr_param1 = -1;
      double curr_param2 = -1;

      char distribution_str[MAXPARAM+1]; // because 1 for single number digit, +1 for \0, 2 is enough
      // but we want to tell user if they entered more than 1 digit that it's wrong?
      char param1_str[MAXPARAM+1]; // double holds 15-17 digit at most, this is 1 more than enough for \0?
      char param2_str[MAXPARAM+1];

      // Get distribution int
      printf("Enter distribution (1-7) for variable %c:", variables[var_index]);
      while(1){
        i++;
        if(fgets(distribution_str, MAXPARAM, stdin) == NULL) {
          perror("fgets didn't read anything!");
          exit(1);
        }
        
        // Change read string into numeric value
        curr_distribution = strtol(distribution_str, NULL, 10);

        // if valid choice
        if (curr_distribution >= 1 && curr_distribution <= 7) {
          // Confirm choice:
          printf("You chose distribution %ld! \n", curr_distribution);
          break;
        }
        else {printf("Please enter a valid distribution for variable (1-7) %c:\n", variables[var_index]); }
      }
      

      // *****UNIFORM distribution, wrong bounds loop
      i = 0;
      while (curr_distribution == 2 && i < 3) {
        i++;
        printf("%s","Enter Lower Bound: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);
        // confirm value
        printf("Lower Bound: %f\n", curr_param1);

        printf("%s", "Enter Upper Bound: ");
        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param2_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // change read strings into numeric values
        curr_param2 = strtod(param2_str, NULL);
        
        if (curr_param1 >= curr_param2) {
          fprintf(stderr, "Lower bound must be strictly less than upper bound, please try again!\n");
          continue; // if bounds are invalid, loop starts over
        }
        // confirm value
        printf("Upper Bound: %f\n", curr_param2);
        break; // if bounds are valid, loop breaks
      }

      // *****NORMAL distribution, invalid SD (standard deviation) loop
      i = 0;
      while (curr_distribution == 3 && i < 3) {
        i++;
        printf("%s","Enter mean of normal distribution: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);
        // confirm value
        printf("Mean: %f\n", curr_param1);

        printf("%s", "Enter standard deviation of normal distribution: ");
        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param2_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // change read strings into numeric values
        curr_param2 = strtod(param2_str, NULL);
        
        if (curr_param2 <= 0) {
          fprintf(stderr, "Standard deviation must be positive, please try again!\n");
          continue; // if SD ≤ 0, loop restarts
        }
        // confirm value
        printf("Standard Deviation: %f\n", curr_param2);
        break; // if SD is valid, loop breaks
      }


      // *****EXPONENTIAL distribution, invalid lambda loop
      i = 0;
      while (curr_distribution == 4 && i < 3) {
        i++;
        printf("%s","Enter lambda of exponential distribution: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);

        if (curr_param1 <= 0) {
          fprintf(stderr, "Lambda must be positive, please try again!\n");
          continue; // if lambda ≤ 0, loop restarts
        }

        // confirm value
        printf("Lambda: %f\n", curr_param1);
        break; // if lambda is valid, loop breaks
      }


      // *****BERNOULLI distribution, invalid p loop
      i = 0;
      while (curr_distribution == 5 && i < 3) {
        i++;
        printf("%s","Enter p of bernoulli distribution: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);

        if (curr_param1 < 0 || curr_param1 > 1) {
          fprintf(stderr, "p must be between 0 and 1, please try again!\n");
          continue; // if p invalid, loop restarts
        }

        // confirm value
        printf("p: %f\n", curr_param1);
        break; // if p is valid, loop breaks
      }

      // *****BINOMIAL distribution, invalid SD loop
      i = 0;
      while (curr_distribution == 6 && i < 3) {
        i++;
        printf("%s","Enter trial number (n) of binomial distribution: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);

        if (curr_param1 <= 0) {
          fprintf(stderr, "n must be positive, please try again!\n");
          continue; // if SD ≤ 0, loop restarts
        }

        // confirm value (n is an integer)
        printf("n: %d\n", (int) curr_param1);

        printf("%s", "Enter p of binomial distribution: ");
        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param2_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // change read strings into numeric values
        curr_param2 = strtod(param2_str, NULL);
        
        if (curr_param2 < 0 || curr_param2 > 1) {
          fprintf(stderr, "p must be between 0 and 1, please try again!\n");
          continue; // if SD ≤ 0, loop restarts
        }
        // confirm value
        printf("p: %f\n", curr_param2);
        break; // if SD is valid, loop breaks
      }

      // *****POISSON distribution, invalid lambda loop
      i = 0;
      while (curr_distribution == 7 && i < 3) {
        i++;
        printf("%s","Enter lambda of poisson distribution: ");

        // empty value loop
        i = 0;        
        while(1 && i < 3){
          i++;
          if(fgets(param1_str, MAXPARAM, stdin) == NULL) {
            fprintf(stderr, "fgets didn't read anything, please try again!: ");
            continue;
          }
          break;
        }

        // Change read strings into numeric values
        curr_param1 = strtod(param1_str, NULL);

        if (curr_param1 <= 0) {
          fprintf(stderr, "Lambda must be positive, please try again!\n");
          continue; // if lambda ≤ 0, loop restarts
        }

        // confirm value
        printf("Lambda: %f\n", curr_param1);
        break; // if lambda is valid, loop breaks
      }


      // make a VarDist, store in vardist_list
      vardist_list[*vardist_list_length] = make_vardist(variables[var_index], curr_distribution, curr_param1, curr_param2);

      // increment length of vardist list
      (*vardist_list_length)++;
      // increment index
      var_index++;
  }
}



void spawn_process(const char *executable, int to_child[2], int from_child[2], pid_t *pid, int agg_write_fd) {
    if (pipe(to_child) == -1)   { perror("pipe to_child failed");   exit(1); }
    if (pipe(from_child) == -1) { perror("pipe from_child failed"); exit(1); }

    *pid = fork();
    if (*pid == -1) { perror("fork failed"); exit(1); }

    if (*pid == 0) {
        close(to_child[1]); // close writing
        close(from_child[0]); // close reading

        // dup2(to_child[0],   STDIN_FILENO); // redirect pipe read end to stdin
        // dup2(from_child[1], STDOUT_FILENO); // redirect pipe write end to stdout
        // +errorcheck
        if (dup2(to_child[0],   STDIN_FILENO)  == -1) { perror("dup2 stdin");  exit(1); } // redirect pipe read end to stdin
        if (dup2(from_child[1], STDOUT_FILENO) == -1) { perror("dup2 stdout"); exit(1); } // redirect pipe write end to stdout

        close(to_child[0]); // finished rewiring, close original fd
        close(from_child[1]); // finished rewiring, close original fd

        // close inherited aggregator pipe if this is a worker (not the aggregator itself)
        // since worker doesn't directly communicate with aggregator. Technically we should
        // close all other aggregator connected pipes, but this is the only one that can cause
        // deadlock, but 
        // +errorcheck
        if (agg_write_fd != -1) { close(agg_write_fd); }
        
        execl(executable, executable, NULL); // replace child process with executable
        perror("execl failed"); // only runs if execl failed
        exit(1);
    }

    // parent here
    close(to_child[0]); // Only child reads
    close(from_child[1]); // only child writes
}