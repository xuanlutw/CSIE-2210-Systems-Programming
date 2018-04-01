/* B05202043 呂佳軒 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argv, char* argc[]){
    int fd;
    int ret;
    char filename[NAME_MAX];
    char tmp[PIPE_BUF];
// open file
    fscanf(stdin, "%s", filename);
    fd = open(filename, O_RDONLY);
    if (fd < 0){
        fprintf(stderr, "GCI: %s, PID: %d, open filename: %s fail\n", argc[0], getpid(), filename);
        exit(1);
    }
    fprintf(stderr, "GCI: %s, PID: %d, open filename: %s success\n", argc[0], getpid(), filename);
// write file into pipe
    ret = 1;
    while (ret){
        ret = read(fd, tmp, PIPE_BUF);
        write(STDOUT_FILENO, tmp, ret);
    }
    fprintf(stderr, "GCI: %s, PID: %d, finish\n", argc[0], getpid());
#ifdef SLOW
    sleep(30);
#endif
    exit(0);
}
