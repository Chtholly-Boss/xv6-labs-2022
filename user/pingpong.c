#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int pid;
    int p[2];
    char buf[512];
    pipe(p);
    if ((pid = fork()) < 0) {
        fprintf(2, "fork error\n");
        exit(1);
    } else if (pid > 0) {
        // parent
        write(p[1], "h", 1);
        close(p[1]);
        wait(0);
        if (read(p[0], buf, sizeof buf) > 0) {
            fprintf(1, "%d: received pong\n", getpid());
        }
    } else {
        // child
        if (read(p[0], buf, sizeof buf) > 0) {
            fprintf(1, "%d: received ping\n", getpid());
            write(p[1], buf, 1);
            close(p[1]);
        }        
    }
    exit(0);
}