#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_OF_FEATURE 33
#define MAX_DATASET 26000

typedef struct{
    double feature[NUM_OF_FEATURE];
    int result;
} Dataset;

typedef struct{
    double feature;
    int result;
} Pair;

typedef struct node{
    int feature;
    double threshold;
    struct node* left;
    struct node* right;
} Node;

/* Job define
   0: thread terminal
   1: construct tree 
        Node** root
        int index[]
   2: make decision
        Node* root
        int* index
        int* result
        Dataset* test_data;
*/

typedef struct job{
    int type;
    int* index;
    int* result;
    Dataset* test_data;
    void* root;
    struct job* next;
} Job;

Dataset* dataset;
int num_data;
pthread_mutex_t lock_of_malloc = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_of_job = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_of_job = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_of_vote = PTHREAD_MUTEX_INITIALIZER;
Job* job_list;

void* thread_entry(void* arg);
Node* construct_tree(int* index, int num);
int make_decision(Node* root, Dataset* test_data);

int main(int argv, char** argc){
    char training_path[PATH_MAX];
    char testing_path[PATH_MAX];
    char output_file[NAME_MAX];
    Node** root;
    int num_tree;
    int num_thread;
    Dataset* test_dataset;
    int num_test_data;


    // Pharse arg
    if (argv = 9 || strcmp(argc[1], "-data") || strcmp(argc[3], "-output"), strcmp(argc[5], "-tree"), strcmp(argc[7], "-thread")){
        fprintf(stderr, "Wrong format!\n");
        exit(1);
    }
    strcpy(training_path, argc[2]);
    strcat(training_path, "/training_data");
    strcpy(testing_path, argc[2]);
    strcat(testing_path, "/testing_data");
    strcpy(output_file, argc[4]);
    num_tree = atoi(argc[6]);
    num_thread = atoi(argc[8]);

    // Initialize
    num_data = 0;
    dataset = (Dataset*)malloc(sizeof(Dataset) * MAX_DATASET);
    root = (Node**)malloc(sizeof(Node*) * num_tree);
    for (int i = 0 ;i < num_tree;++i) root[i] = NULL;
    
    // Read data
    FILE* training_data;
    training_data = fopen(training_path, "rw");
    if (!training_data){
        fprintf(stderr, "open %s err\n", training_path);
        exit(1);
    }
    while (fscanf(training_data, "%*d") != EOF){
        for (int i = 0;i < NUM_OF_FEATURE;++i) fscanf(training_data, "%lf", dataset[num_data].feature + i);
        fscanf(training_data, "%d", &dataset[num_data].result);
        ++num_data;
    }
    fclose(training_data);

    // Test construct_tree
    /*
    int a[30] = {1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    construct_tree(a, 30);
    return 0;
    */

    // Initialize thread
    job_list = NULL;
    for (int i = 0;i < num_thread - 1;++i){
        pthread_t tid;
        int err = pthread_create(&tid, NULL, thread_entry, NULL);
        if (err){
            fprintf(stderr, "create thread err\n");
            exit(1);
        }
    }

    // Given job of construct tree 
    srand(time(NULL));
    for (int i = 0;i < num_tree;++i){
        pthread_mutex_lock(&lock_of_malloc);
        Job* new_job = (Job*)malloc(sizeof(Job));
        int* index = (int*)malloc(sizeof(int) * num_data);
        pthread_mutex_unlock(&lock_of_malloc);
        
        for (int i = 0;i < num_data;++i) index[i] = rand() % num_data;
        new_job->type = 1;
        new_job->index = index;
        new_job->root = root + i;
        pthread_mutex_lock(&lock_of_job);
        new_job->next = job_list;
        job_list = new_job;
        pthread_mutex_unlock(&lock_of_job);
        pthread_cond_broadcast(&cond_of_job);
    }

    // Wait construct tree done
    for (int i = 0;i < num_tree;){
        if (root[i] == NULL) sleep(2);
        else ++i;
    }
    //fprintf(stderr, "Construct tree done\n");

    // Test make decision
    /*
    Dataset test = {{71.0, 65.0, 6.0, 36.0, 50.0, 47.0, 3.0, 33.0, 31.0, 2.0, 27.0, 27.0, 5.0, 14.0, 3.0, 3.0, 0.0, 1.0, 14.0, 
                    1.0, 5.0, 2.62962962963, 2.40740740741, 1.2, 0.408450704225, 0.446153846154, 0.0, 64.5476190476, 66.0277777778,
                    55.6666666667, 3779.53255932, 3556.74555172, 16701.179}, 0};
    int ret = make_decision(root[0], &test);
    printf("%d\n", ret);
    */

    // Read test data
    test_dataset = (Dataset*)malloc(sizeof(Dataset) * MAX_DATASET);
    num_test_data = 0;
    FILE* testing_data;
    testing_data = fopen(testing_path, "rw");
    if (!testing_data){
        fprintf(stderr, "open %s err\n", testing_path);
        exit(1);
    }
    while (fscanf(testing_data, "%*d") != EOF){
        for (int i = 0;i < NUM_OF_FEATURE;++i) fscanf(testing_data, "%lf", test_dataset[num_test_data].feature + i);
        ++num_test_data;
    }
    fclose(testing_data);

    // Given job of decision data
    int vote_result[num_test_data];
    int vote_num[num_test_data];
    for (int i = 0;i < num_test_data;++i){
        vote_result[i] = 0;
        vote_num[i] = 0;
        for (int j = 0;j < num_tree;++j){
            pthread_mutex_lock(&lock_of_malloc);
            Job* new_job = (Job*)malloc(sizeof(Job));
            pthread_mutex_unlock(&lock_of_malloc);

            new_job->type = 2;
            new_job->index = vote_num + i;
            new_job->result = vote_result + i;
            new_job->test_data = test_dataset + i;
            new_job->root = root[j];
            pthread_mutex_lock(&lock_of_job);
            new_job->next = job_list;
            job_list = new_job;
            pthread_mutex_unlock(&lock_of_job);
            pthread_cond_broadcast(&cond_of_job);
        }
    }

    // Wait decision done
    for (int i = 0;i < num_test_data;){
        if (vote_num[i] != num_tree) sleep(2);
        else ++i;
    }
    //fprintf(stderr, "Decision done\n");

    // Terminal all thread
    for (int i = 0;i < num_thread;++i){
        pthread_mutex_lock(&lock_of_malloc);
        Job* new_job = (Job*)malloc(sizeof(Job));
        pthread_mutex_unlock(&lock_of_malloc);

        new_job->type = 0;
        pthread_mutex_lock(&lock_of_job);
        new_job->next = job_list;
        job_list = new_job;
        pthread_mutex_unlock(&lock_of_job);
        pthread_cond_broadcast(&cond_of_job);
    }

    // Write result
    FILE* output;
    output = fopen(output_file, "w");
    if (!output){
        fprintf(stderr, "open %s err\n", output_file);
        exit(1);
    }
    fprintf(output, "id,label\n");
    for (int i = 0;i < num_test_data;++i) fprintf(output, "%d,%d\n", i, vote_result[i] > 0);
    fclose(output);

    // Calculate accuracy
    /*
    int sum = 0, right = 0, tmp;
    FILE* ans;
    ans = fopen("ans.csv", "r");
    if (!ans){
        fprintf(stderr, "open ans.csv err\n");
        exit(1);
    }
    fscanf(ans, "%*s");
    for (int i = 0;i < num_test_data;++i){
        fscanf(ans, "%*d%*c%d\n", &tmp);
        ++sum;
        if (tmp == (vote_result[i] > 0)) ++right;
    }
    printf("accuracy = %lf\n", (double)right / sum);
    fclose(ans);
    */

    pthread_exit(NULL);
} 

void* thread_entry(void* arg){
    //fprintf(stderr, "thread creat tid = %d\n", pthread_self());
    pthread_detach(pthread_self());

    // Main loop
    while (1){
        // Wait job
        Job* job_now; 
        pthread_mutex_lock(&lock_of_job);
        while (job_list == NULL) pthread_cond_wait(&cond_of_job, &lock_of_job);
        job_now = job_list;
        job_list = job_list->next;
        pthread_mutex_unlock(&lock_of_job);

        // Type 0 thread terminal
        if (job_now->type == 0){
            pthread_mutex_lock(&lock_of_malloc);
            free(job_now);
            pthread_mutex_unlock(&lock_of_malloc);
            //fprintf(stderr, "thread exit tid = %d\n", pthread_self());
            pthread_exit(NULL);
        }

        // Type 1 construct tree
        else if (job_now->type == 1){
            *(Node**)(job_now->root) = construct_tree(job_now->index, 2000);
            pthread_mutex_lock(&lock_of_malloc);
            free(job_now->index);
            free(job_now);
            pthread_mutex_unlock(&lock_of_malloc);
            //fprintf(stderr, "construct tree done tid = %d\n", pthread_self());
        }

        // case 2 make decision
        else if (job_now->type == 2){
            int ret = make_decision(job_now->root, job_now->test_data);
            pthread_mutex_lock(&lock_of_vote);
            if (ret > 0) ++(*(job_now->result));
            else --(*(job_now->result));
            ++(*(job_now->index));
            pthread_mutex_unlock(&lock_of_vote);
            pthread_mutex_lock(&lock_of_malloc);
            free(job_now);
            pthread_mutex_unlock(&lock_of_malloc);
        }
    }
}

int cmp(const void* a, const void* b){
    return (((Pair*)a)->feature > ((Pair*)b)->feature);
    //if (((Pair*)a)->feature < ((Pair*)b)->feature) return -1;
    return 0;
}

Node* construct_tree(int* index, int num){
    // Leaf
    int counter = 0;
    for (int i = 0;i < num;++i) counter += dataset[index[i]].result;
    if (counter == 0 || counter == num){
        pthread_mutex_lock(&lock_of_malloc);
        Node* ans = (Node*)malloc(sizeof(Node));
        pthread_mutex_unlock(&lock_of_malloc);
        ans->feature = dataset[index[0]].result == 1 ? -1 : -2;
        ans->right = NULL;
        ans->left = NULL;
        return ans;
    }
    
    // Find least gini
    int feature;
    double threshold, gini = DBL_MAX;
    for (int i = 0;i < NUM_OF_FEATURE;++i){
        Pair tmp[num];
        double counter1 = 0, counter2 = 0, sum1 = 0, sum2 = num;
        for (int j = 0;j < num;++j){
            counter2 += dataset[index[j]].result;
            tmp[j].feature = dataset[index[j]].feature[i];
            tmp[j].result = dataset[index[j]].result;
        }
        qsort(tmp, num, sizeof(Pair), cmp);
        for (int j = 0;j < num - 1;++j){
            counter1 += tmp[j].result;
            counter2 -= tmp[j].result;
            sum1 += 1;
            sum2 -= 1;
            if (tmp[j].feature == tmp[j + 1].feature) continue;
            double tmp_gini = counter1 / sum1 * (1.0 - counter1 / sum1) + counter2 / sum2 * (1.0 - counter2 / sum2);
            if (tmp_gini < gini){
                gini = tmp_gini;
                feature = i;
                threshold = ((double)tmp[j].feature + (double)tmp[j + 1].feature) / 2;
            }
        }
    }
    //printf ("%d %lf\n", feature, threshold);
    int front = 0, back = num - 1;

    // Classify data by feature
    while (front - back != 1){
        if (dataset[index[front]].feature[feature] > threshold){
            int tmp = index[front];
            index[front] = index[back];
            index[back] = tmp;
            --back;
        }
        else ++front;
    }
    //printf(" %d %d\n", num, front);

    // Cycle break
    if (front == 0 || front == num){
        pthread_mutex_lock(&lock_of_malloc);
        Node* ans = (Node*)malloc(sizeof(Node));
        pthread_mutex_unlock(&lock_of_malloc);
        ans->feature = dataset[index[0]].result == 1 ? -1 : -2;
        ans->right = NULL;
        ans->left = NULL;
        return ans;
    }

    // Recursion
    pthread_mutex_lock(&lock_of_malloc);
    Node* ans = (Node*)malloc(sizeof(Node));
    pthread_mutex_unlock(&lock_of_malloc);
    ans->feature = feature;
    ans->threshold = threshold;
    ans->left = construct_tree(index + front, num - front);
    ans->right = construct_tree(index, front);
    return ans;
}

int make_decision(Node* root, Dataset* test_data){
    if (root->feature == -1) return 1;
    if (root->feature == -2) return 0;
    if (test_data->feature[root->feature] > root->threshold) return make_decision(root->left, test_data);
    return make_decision(root->right, test_data);
}
