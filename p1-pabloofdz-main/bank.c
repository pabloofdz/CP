#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    pthread_mutex_t *mutex;
};

struct iters{
    int iterationsd;
    int iterationst;
    pthread_mutex_t mutex;
};

struct args {
    int           thread_num;  // application defined thread #
    int           delay;       // delay between operations
    int           net_total;   // total amount deposited by this thread
    struct iters *iters;
    struct bank  *bank;        // pointer to the bank (shared with other threads)
};

struct args2 {
    bool            terminado;
    struct bank    *bank2;
    int             delay2;
    pthread_mutex_t mutex2;
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

int aux2(struct iters *iters, bool isdeposit){
    bool fin = false;

    pthread_mutex_lock(&iters->mutex);
    if(isdeposit){
        iters->iterationsd--;
        if(iters->iterationsd < 0)
            fin = true;
    }else{
        iters->iterationst--;
        if(iters->iterationst < 0)
            fin = true;
    }
    pthread_mutex_unlock(&iters->mutex);
    return fin;
}
// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;
    bool fin = false;

    while(true) {
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;


        pthread_mutex_lock(&args->bank->mutex[account]);
        fin = aux2(args->iters, true);
        if(fin){
            pthread_mutex_unlock(&args->bank->mutex[account]);
            break;
        }
        printf("Thread %d depositing %d on account %d\n",
            args->thread_num, amount, account);

        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;

        pthread_mutex_unlock(&args->bank->mutex[account]);
    }
    return NULL;
}

void *transfer(void *ptr)
{
  struct args *args =  ptr;
  int amount, account1, account2, balance1, balance2;
  bool fin = false;

  while(true) {
      account1 = rand() % args->bank->num_accounts;
      while(1){
          pthread_mutex_lock(&args->bank->mutex[account1]);

          if(args->bank->accounts[account1] == 0)
              amount = 0;
          else
              amount  = rand() % args->bank->accounts[account1];

          account2 = rand() % args->bank->num_accounts;
          if (pthread_mutex_trylock(&args->bank->mutex[account2])){
              pthread_mutex_unlock(&args->bank->mutex[account1]);
              continue;
          }
          break;
      }
      while(account1 == account2){
          account2 = rand() % args->bank->num_accounts;
      }
      fin = aux2(args->iters, false);
      if(fin){
          pthread_mutex_unlock(&args->bank->mutex[account1]);
          pthread_mutex_unlock(&args->bank->mutex[account2]);
          break;
      }

      printf("Thread %d transfering %d from account %d on account %d\n",
              args->thread_num, amount, account1, account2);
      balance1 = args->bank->accounts[account1];
      if(args->delay) usleep(args->delay); // Force a context switch

      balance1 -= amount;
      if(args->delay) usleep(args->delay);

      args->bank->accounts[account1] = balance1;
      if(args->delay) usleep(args->delay);

      balance2 = args->bank->accounts[account2];
      if(args->delay) usleep(args->delay);

      balance2 += amount;
      if(args->delay) usleep(args->delay);

      args->bank->accounts[account2] = balance2;
      if(args->delay) usleep(args->delay);

      pthread_mutex_unlock(&args->bank->mutex[account1]);
      pthread_mutex_unlock(&args->bank->mutex[account2]);
  }
  return NULL;
}

// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank)
{
    struct thread_info *threads;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * 2 * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    return threads;
}
struct thread_info *start_threads_deposit(struct options opt, struct bank *bank, struct iters *iters, struct thread_info *threads)
{
    int i;
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> iters       = iters;
        threads[i].args -> delay      = opt.delay;

        if (0 != pthread_create(&threads[i].id, NULL, deposit, threads[i].args)) {
              printf("Could not create thread #%d", i);
              exit(1);
        }
    }
    return threads;

}
struct thread_info *start_threads_transfer(struct options opt, struct bank *bank, struct iters *iters, struct thread_info *threads)
{
      int i;
      for (i = opt.num_threads; i < 2 * opt.num_threads; i++) {
          threads[i].args = malloc(sizeof(struct args));

          threads[i].args -> thread_num = i;
          threads[i].args -> net_total  = 0;
          threads[i].args -> bank       = bank;
          threads[i].args -> iters       = iters;
          threads[i].args -> delay      = opt.delay;

          if (0 != pthread_create(&threads[i].id, NULL, transfer, threads[i].args)) {
              printf("Could not create thread #%d", i);
              exit(1);
          }
      }
      return threads;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads, bool isdeposit) {
    int total_deposits=0, bank_total=0;
    if(isdeposit){
        printf("\nNet deposits by thread\n");

        for(int i=0; i < num_threads; i++) {
            printf("%d: %d\n", i, thrs[i].args->net_total);
            total_deposits += thrs[i].args->net_total;
        }
        printf("Total: %d\n", total_deposits);
    }

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n\n", bank_total);
}

bool aux(struct args2 *args2){
    bool copia;
    pthread_mutex_lock(&args2->mutex2);
    copia = args2->terminado;
    pthread_mutex_unlock(&args2->mutex2);
    return copia;
}

void *thread_function(void *ptr){
    struct args2 *args2 =  ptr;
    int bank_total = 0;
    while(!(aux(args2))){
        bank_total = 0;
        for(int i=0; i < args2->bank2->num_accounts; i++) {//for para bloquear las cuentas
            pthread_mutex_lock(&args2->bank2->mutex[i]);
        }
        printf("\nAccount balance\n");
        for(int i=0; i < args2->bank2->num_accounts; i++) {
            printf("%d: %d\n", i, args2->bank2->accounts[i]);
            bank_total += args2->bank2->accounts[i];
        }
        printf("Total: %d\n\n", bank_total);
        for(int i=0; i < args2->bank2->num_accounts; i++) {//for para desbloquear las cuentas
            pthread_mutex_unlock(&args2->bank2->mutex[i]);
        }

        usleep((args2->delay2)/1000);
    }
    return NULL;
}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads, bool isdeposit, struct args2 *args2) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    if(!isdeposit){
        pthread_mutex_lock(&args2->mutex2);
        args2->terminado = true;
        pthread_mutex_unlock(&args2->mutex2);
    }
    print_balances(bank, threads, opt.num_threads, isdeposit);
}

void do_frees(struct options opt, struct bank *bank, struct thread_info *threads){
    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
    free(bank->accounts);
    free(bank->mutex);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank->mutex        = malloc(bank->num_accounts * sizeof(pthread_mutex_t));


    for(int i=0; i < bank->num_accounts; i++){
        bank->accounts[i] = 0;
        pthread_mutex_init(&bank->mutex[i], NULL);
    }
}

void init_iters(struct iters *iters, int iterations2) {
    iters->iterationsd = iterations2;
    iters->iterationst = iterations2;
    pthread_mutex_init(&iters->mutex, NULL);

}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct iters        iters;
    struct thread_info *thrs;
    pthread_t           thread;
    struct args2       *args2 = malloc(sizeof(struct args2));
    pthread_mutex_init(&args2->mutex2, NULL);

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);
    init_iters(&iters, opt.iterations);

    args2->terminado = false; args2->bank2 = &bank; args2->delay2 = opt.delay;

    thrs = start_threads(opt, &bank);
    thrs = start_threads_deposit(opt, &bank, &iters, thrs);
    wait(opt, &bank, thrs, true, args2);
    thrs = start_threads_transfer(opt, &bank, &iters, thrs);
    pthread_create(&thread, NULL, thread_function, args2);
    wait(opt, &bank, thrs, false, args2);
    do_frees(opt, &bank, thrs);

    return 0;
}
