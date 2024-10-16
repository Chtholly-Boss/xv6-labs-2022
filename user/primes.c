#include "kernel/types.h"
#include "user/user.h"

void filt_primes(int *p);

int main(int argc, char *argv[])
{
    int p[2];
    pipe(p);
    // feed the pipe 
    for (int i = 2; i < 36; i++) {
        write(p[1], &i, sizeof(int));
    }
    close(p[1]);
    // fork the prime filter
    if (fork() == 0) {
        filt_primes(p);
    } else {
        wait(0);
    }
    exit(0);
}

void filt_primes(int *p) {
    // p is the previous pipe
    int prime;
    if (read(p[0], &prime, sizeof(int)) == 0) {
        exit(0);
    }
    fprintf(1, "prime %d\n", prime);
    // filt the previous pipe with the current prime
    int pp[2];
    pipe(pp);
    int buf;
    while(read(p[0], &buf, sizeof(int)) != 0) {
        if (buf % prime != 0) {
            write(pp[1], &buf, sizeof(int));
        }
    }
    close(pp[1]);
    if (fork() == 0) {
        filt_primes(pp);
    } else {
        wait(0);
    }
}