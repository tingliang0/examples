#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    int a[2];
    char buff[10];
    if (pipe(a) == -1) {
        perror("pipe");
        exit(1);
    }

    write(a[1], "code", 5);
    printf("\n");

    read(a[0], buff, 5);
    printf("%s", buff);

    return 0;
}
