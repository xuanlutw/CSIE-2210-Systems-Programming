/*b05202043 呂佳軒*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

int main(int argv, char* argc[]){
    char hostid[16];
    int player_index;   /*A-0 B-1 C-2 D-3*/
    char random_key[16];
    FILE* read_FIFO;
    FILE* write_FIFO;
    
    if (argv != 4) {
        fprintf(stderr, "argerr");
        exit(1);
    }
    strcpy(hostid, argc[1]);
    player_index = argc[2][0] - 'A';
    strcpy(random_key, argc[3]);
    //printf("%s %d %d\n", hostid, player_index, random_key);
    
    char path[32] = "host";
    char path1[32] = "_A.FIFO";
    path1[1] += player_index;
    strcat(path, hostid);
    strcat(path, path1);
    //printf("%s\n", path);
    read_FIFO = fopen(path, "r");
    strcpy(path, "host");
    strcat(path, hostid);
    strcat(path, ".FIFO");
    write_FIFO = fopen(path, "w");
    //printf("Open FIFO done%d %d\n", read_FIFO, write_FIFO);

    srand((((int)time(NULL)) * player_index) % 31);
    
    for (int i = 0;i < 10;++i){
        int money[4];
        int k = 0;
        money[0] = 0;
        fscanf(read_FIFO, "%d%d%d%d", &money[0], &money[1], &money[2], &money[3]);
        //if (!player_index) printf("%d %d %d %d\n", money[0], money[1], money[2], money[3]);
        int fll = 1, max = -1;
        for (int j = 0;j < 4;++j){
            if (j == player_index) continue;
            if (money[player_index] <= money[j]) fll = 0;
            if (money[j] > max) max = money[j];
        }
        if (fll){
            fprintf(write_FIFO, "%c %s %d\n", player_index + 'A', random_key, max + 1);
        }
        else{
            fprintf(write_FIFO, "%c %s %d\n", player_index + 'A', random_key, money[player_index] - rand() % 27);
        } 
        fflush(write_FIFO);
    }
    return 0;
}
