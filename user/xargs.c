#include <kernel/types.h>
#include <user/user.h>
char* getline();
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs [command]\n");
        exit(1);
    }
    char *cmd = argv[1];
    char *args[argc];
    for (int i = 1; i < argc; i++) {
        args[i-1] = argv[i];
    }
    while(strcmp((args[argc-1] = getline()),"") != 0) {
        if (fork() == 0) {
            exec(cmd, args);
        } else {
            wait((int *) 0);
        }
    }
    exit(0);
}
char* getline() {
    static char buf[512];
    int i = 0;
    char c;
    while(read(0, &c, 1) > 0 && c != '\n') {
        buf[i++] = c;
    }
    buf[i] = '\0';
    return buf;
}