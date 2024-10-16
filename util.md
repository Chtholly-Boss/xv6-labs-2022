# Util
[MIT6.1810 Util Page](https://pdos.csail.mit.edu/6.S081/2022/labs/util.html)

This lab serves as a warmup for the rest of the course.

In summary, we will write several user programs:
* sleep
* pingpong
* primes
* find
* xargs

To start, add each program to the Makefile and create corresponding `*.c` file under `user/`.

```sh
# in Makefile
...
UPROGS=\
    $U/_xargs\
    $U/_find\
    $U/_primes\
    $U/_pingpong\
    $U/_sleep\
    ...
```

## Sleep
We can learn how a C program get input from the command line through `argc` and `argv`, the work can be done easily:
* check the argument
* call syscall `sleep`

You can find the solution in `user/sleep.c` in branch `util`

## pingpong
We can learn how to use pipe to communicate between two processes:
* create a pipe
* parent write and child read
* child write and parent read

You can find the solution in `user/pingpong.c` in branch `util`

Note that after writing, we'd better close the write end.

## primes
We can learn how to use pipe to achieve multiprocess communication.

The core logic of this program is showed in the `filt_primes`:
```c
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
```
In summary, we filter the input from the previous pipe and feed them to the next pipe. Notice that the process is parallel, not sequential.

You can find the solution in `user/primes.c` in branch `util`

## find
We can learn how to access the file system API.
The task can be done easily after figure out how `user/ls.c` works.

The core logic is doing `switch-case` on file types:
```c
switch (st.type) {
        case T_DEVICE:
        case T_FILE: {
            ...
        }
        case T_DIR: {
            ...
        }
    }
```
We just need to care about not to follow `.` and `..`.

You can find the solution in `user/find.c` in branch `util`

## xargs
We can learn how to use `exec` to run a program with arguments.

The core logic is:
* concatete the arguments with the standard input
* call `exec` to run the program

```c
while(strcmp((args[argc-1] = getline()),"") != 0) {
        if (fork() == 0) {
            exec(cmd, args);
        } else {
            wait((int *) 0);
        }
    }
```

You can find the solution in `user/xargs.c` in branch `util`

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.