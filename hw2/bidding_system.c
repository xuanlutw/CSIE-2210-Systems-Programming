/*b05202043 呂佳軒*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

int main(int argv, char* argc[]){
    int host_num;
    int player_num;
    int r_pipe[20][2];
    int w_pipe[20][2];
    fd_set fd_read;
    int n_fd_read = -1;
    pid_t host_pid[20];
    int done[20] = {0};
    struct timeval wait_time;
    wait_time.tv_sec = 10;
    wait_time.tv_usec = 500000;
    int score[32] = {0};
    int final_rank[32] = {0};

//do phaser
    if (argv != 3){
        fprintf(stderr, "Err argv\n");
        exit(0);
    }
    host_num = atoi(argc[1]);
    player_num = atoi(argc[2]);
    //printf("%d %d\n", host_num, player_num);

//fork host
    FD_ZERO(&fd_read);
    for (int i = 1;i <= host_num;++i){ 
        pipe(r_pipe[i]);
        pipe(w_pipe[i]);
        host_pid[i] = fork();
        if (!host_pid[i]){
            dup2(w_pipe[i][0], STDIN_FILENO);
            dup2(r_pipe[i][1], STDOUT_FILENO);
            close(w_pipe[i][0]);
            close(w_pipe[i][1]);
            close(r_pipe[i][0]);
            close(r_pipe[i][1]);
            char par[32];
            sprintf(par, "%d", i);
            execl("host", "host", par, NULL);  
        }
        close(w_pipe[i][0]);
        close(r_pipe[i][1]);
        FD_SET(r_pipe[i][0], &fd_read);
        if (r_pipe[i][0] > n_fd_read) n_fd_read = r_pipe[i][0];
    }
    ++n_fd_read;

//command host
    for (int i1 = 1;i1 <= player_num;++i1){
        for(int i2 = i1 + 1;i2 <= player_num;++i2){
            for(int i3 = i2 + 1;i3 <= player_num;++i3){
                for(int i4 = i3 + 1;i4 <= player_num;++i4){
                    //printf("%d %d %d %d\n", i1, i2, i3, i4);

                    int fl = 0;
                    for (int i = 1;i <= host_num;++i){
                        if (!done[i]){
                            char par[64];
                            sprintf(par, "%d %d %d %d\n", i1, i2, i3, i4);
                            write(w_pipe[i][1], par, strlen(par));
                            fl = 1;
                            done[i] = 1;
                            break;
                        }
                    }
                    if (fl) continue;

                    fd_set copy;
                    copy = fd_read;
                    int ret = select(n_fd_read, &copy, NULL, NULL, NULL);
                    for (int i = 1;i <= host_num;++i){
                        if (FD_ISSET(r_pipe[i][0], &copy)){
                            char tmp[128];
                            int index[4];
                            int rank[4];
                            read(r_pipe[i][0], tmp, 128);
                            sscanf(tmp, "%d%d%d%d%d%d%d%d", &index[0], &rank[0], &index[1], &rank[1], &index[2], &rank[2], &index[3], &rank[3]);
                            for (int j = 0;j < 4;++j) score[index[j]] += (4 - rank[j]);
                            done[i] = 0;
                        }
                    }

                    for (int i = 1;i <= host_num;++i){
                        if (!done[i]){
                            char par[64];
                            sprintf(par, "%d %d %d %d\n", i1, i2, i3, i4);
                            write(w_pipe[i][1], par, strlen(par));
                            done[i] = 1;
                            break;
                        }
                    }
                }
            }
        }
    }

//host done
    for (int i = 1;i <= host_num;++i){
        if (done[i]){
            char tmp[128];
            int index[4];
            int rank[4];
            read(r_pipe[i][0], tmp, 128);
            sscanf(tmp, "%d%d%d%d%d%d%d%d", &index[0], &rank[0], &index[1], &rank[1], &index[2], &rank[2], &index[3], &rank[3]);
            for (int j = 0;j < 4;++j) score[index[j]] += (4 - rank[j]);
            done[i] = 0;
        }
    }
    //for (int i = 1;i <= player_num;++i) printf("%d %d\n", i, score[i]);

//result out
    for (int i = 1;i <= player_num;++i){
        for (int j = i + 1;j <= player_num;++j){
            if (score[j] > score[i]) ++final_rank[i];
            if (score[j] < score[i]) ++final_rank[j];
        }
    }
    for (int i = 1;i <= player_num;++i) printf("%d %d\n", i, final_rank[i] + 1);
 
//wait host
    int status;
    for (int i = 1;i <= host_num;++i){ 
        char par[64] = "-1 -1 -1 -1\n";
        write(w_pipe[i][1], par, strlen(par));
        waitpid(host_pid[i], &status, 0);
    }
    return 0;
}
