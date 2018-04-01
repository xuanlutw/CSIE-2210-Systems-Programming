/*b05202043 呂佳軒*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int argv, char* argc[]){
    char hostid[64];
    char path[5][64];
    int wfifo[5];
    int rfifo;
    mode_t mode = S_IRUSR | S_IWUSR;
    pid_t player_pid[4];
    int playerid[4];
    int random_key[4];
    srand(time(NULL));
    int money[4];
    int wins[4] = {0};
    int is_same[4];

//do phaser
    if (argv != 2){
        fprintf(stderr, "Err argv");
        exit(0);
    }
    strcpy(hostid, argc[1]);
    
//make FIFO
    for (int i = 0;i < 5;++i){
        char tmp[16] = {0};
        if (i == 4) strcpy(tmp, ".FIFO");
        else{ 
            strcpy(tmp, "_A.FIFO");
            tmp[1] += i;
        }
        strcpy(path[i],"host");
        strcat(path[i], argc[1]);
        strcat(path[i], tmp);
        mkfifo(path[i], mode);
    } 

//run game
    while (1){
        scanf("%d %d %d %d", &playerid[0], &playerid[1], &playerid[2], &playerid[3]);
        if (playerid[0] == -1 && playerid[1] == -1 && playerid[2] == -1 && playerid[3] == -1) break;
    
//fork player
        for (int i = 0;i < 4;++i){
            player_pid[i] = fork();
            if (!player_pid[i]){
                char par1[32], par2[32];
                sprintf(par1, "%c", i + 'A');
                random_key[i] = rand() % 65535;
                sprintf(par2, "%d", random_key[i]);
                int ret = execl("player", "player", argc[1], par1, par2, NULL);
                //printf("%d %d\n", ret, getppid());
            }
        }

//open FIFO
        rfifo = open(path[4], O_RDONLY | O_NONBLOCK);
        for (int i = 0;i < 4;){
            if ((wfifo[i] = open(path[i], O_WRONLY | O_NONBLOCK)) != -1) ++i;
        } 
        //printf("open fifo success\n");

//run 10 round
        for (int i = 0;i < 4;++i){ 
            money[i] = 0;
            wins[i] = 0;
        }
        for (int i = 0;i < 10;++i){
//write FIFO
            for (int j = 0;j < 4;++j) money[j] += 1000;
            char to_write[128];
            sprintf(to_write, "%d %d %d %d\n", money[0], money[1], money[2], money[3]);
            for (int j = 0;j < 4;++j){
                int ret = write(wfifo[j], to_write, strlen(to_write));
                //printf("%d\n", ret);
            }
//read FIFO
            char tmpp[2048] = {0}; 
            char index[4];
            int key[4];
            int pay[4];
            while (1){
                if(sscanf(tmpp, "%s%d%d%s%d%d%s%d%d%s%d%d", &index[0], &key[0], &pay[0], &index[1], &key[1], &pay[1]
                                                          , &index[2], &key[2], &pay[2], &index[3], &key[3], &pay[3]) == 12) break;
                /*fd_set rset;
                FD_ZERO(&rset);
                FD_SET(rfifo, &rset);
                select(rfifo + 1, &rset, NULL, NULL, NULL);*/
                read(rfifo, tmpp + strlen(tmpp), 1024);
            //fprintf(stderr, "%s\n", argc[1]); 
            }
            //printf("%s\n", tmpp);
//find winner
            is_same[0] = is_same[1] = is_same[2] = is_same[3] = 0;
            if (pay[0] == pay[1]) is_same[0] = is_same[1] = 1;
            if (pay[0] == pay[2]) is_same[0] = is_same[2] = 1;
            if (pay[0] == pay[3]) is_same[0] = is_same[3] = 1;
            if (pay[1] == pay[2]) is_same[1] = is_same[2] = 1;
            if (pay[1] == pay[3]) is_same[1] = is_same[3] = 1;
            if (pay[2] == pay[3]) is_same[2] = is_same[3] = 1;
            int max = -1;
            int winner = -1;
            for (int j = 0;j < 4;++j){
                if (pay[j] > max && !is_same[j]){
                    winner = index[j] - 'A';
                    max = pay[j];
                }
            }
            if (winner != -1) {
                ++wins[winner];
                money[winner] -= pay[winner];
            }
        }
        //printf("game done A %d B %d C %d D %d\n", wins[0], wins[1], wins[2], wins[3]);

//wait player
        int status;
        for (int i = 0;i < 4;++i) waitpid(player_pid[i], &status, 0); 
        
//close FIFO
        for (int i = 0;i < 5;++i) close(wfifo[i]);
        close(rfifo);

//return result
        int rank[4] = {0};
        if (wins[0] > wins[1]) ++rank[1];
        if (wins[0] > wins[2]) ++rank[2];
        if (wins[0] > wins[3]) ++rank[3];
        if (wins[1] > wins[2]) ++rank[2];
        if (wins[1] > wins[3]) ++rank[3];
        if (wins[2] > wins[3]) ++rank[3];
        if (wins[0] < wins[1]) ++rank[0];
        if (wins[0] < wins[2]) ++rank[0];
        if (wins[0] < wins[3]) ++rank[0];
        if (wins[1] < wins[2]) ++rank[1];
        if (wins[1] < wins[3]) ++rank[1];
        if (wins[2] < wins[3]) ++rank[2];
        for (int i = 0;i < 4;++i) printf("%d %d\n", playerid[i], rank[i] + 1);
        fflush(stdout);        
    }
//unlink FIFO
    for (int i = 0;i < 5;++i) unlink(path[i]);

    return 0;
}
