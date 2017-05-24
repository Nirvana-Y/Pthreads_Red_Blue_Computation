#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define RED 1
#define BLUE 2
#define BOTH 3
#define PARALLEL 1
#define SEQUENTIAL 2

// declare the functions
void *red_blue_computation(void *threadarg);
void allocate_memory(int l, int w, int **grid_flat, int ***grid);
void init_grid(int n, int ***grid);
void print_grid(int n, int ***grid);
int* distribute_task_for_processes(int num_threads, int num_tasks);
void do_red(int l, int begin, int end, int ***grid);

struct thread_data{
    int row_column_begin;
    int row_column_end;
    int tile_begin;
    int tile_end;
};

// public variables
int *grid_flat;	// one-dimension version of grid
int **grid;	// two-dimension grid
int num_threads, n, t, c, max_iters; // the number of threads,  n-cell grid size, t-tile grid size, c-terminating threshold, max_iters- maximum number of iterations
double threshold; // the threshold shown in percentage
int tile_number; // the number of tile in the grid
int finished_flag = 0; // the flag showing whether the iterations finish or not

int main(int argc, char **argv) {
    int *grid_flat_copy; // the copy of one-dimension version of grid
    int **grid_copy; // the copy of the initial two-dimension grid
    int i, j, k;

    if (argc != 6) {
        printf("Wrong number of arguments.\n");
        printf("Please enter the command in the following format:\n");
        printf("pthreads_red_blue_computation.exe [the number of threads] [cell grid size] [tile grid size] [terminating threshold] [maximum number of iterations]\n");
        printf("Note: [cell grid size] % [tile grid size] = 0; [the number of threads >= 1]\n");
        printf("\n");
        return 0;
    }

    // parse the command line arguments
    num_threads = atoi(argv[1]);
    n = atoi(argv[2]);
    t = atoi(argv[3]);
    c = atoi(argv[4]);
    max_iters = atoi(argv[5]);
    threshold = c / 100.0;

    if ((n % t != 0) || (num_threads < 1)) {
        printf("Illegal arguments.\n");
        printf("Please enter the command in the following format:\n");
        printf("pthreads_red_blue_computation.exe [the number of threads] [cell grid size] [tile grid size] [terminating threshold] [maximum number of iterations]\n");
        printf("Note: [cell grid size] % [tile grid size] = 0; [the number of threads >= 1]\n");
        printf("\n");
        return 0;
    }

    tile_number = t * t;
    pthread_t threads[num_threads];
    struct thread_data thread_data_array[num_threads];

    int *row_column_task = distribute_task_for_processes(num_threads, n);
    int *tile_task  = distribute_task_for_processes(num_threads, tile_number);

    // initialize the grid and print it out
    allocate_memory(n, n, &grid_flat, &grid);
    allocate_memory(n, n, &grid_flat_copy, &grid_copy);

    init_grid(n, &grid);
    memcpy(grid_flat_copy, grid_flat, sizeof(int) * n * n);

    printf("The initial grid: \n");
    print_grid(n, &grid);

    // distribute the workload among the threads
    for (i = 0; i < num_threads; i++){
        // initialize the struct argument
        if (i == 0){
            thread_data_array[i].row_column_begin = 0;
            thread_data_array[i].tile_begin = 0;
        }
        else{
            thread_data_array[i].row_column_begin = thread_data_array[i - 1].row_column_end;
            thread_data_array[i].tile_begin = thread_data_array[i - 1].tile_end;
        }
        thread_data_array[i].row_column_end = thread_data_array[i].row_column_begin + row_column_task[i];
        thread_data_array[i].tile_end = thread_data_array[i].tile_begin + tile_task[i];

        pthread_create(&threads[i], NULL, red_blue_computation, (void *)&thread_data_array[i]);
    }

    pthread_exit(NULL);
}

//
void *red_blue_computation(void *thread_arg){
    int n_itrs; // the iteration times
    struct thread_data *my_data = (struct thread_data *) thread_arg;

    while (!finished_flag && n_itrs < max_iters){
        do_red(n, my_data->row_column_begin, my_data->row_column_end, &grid);

    }


    pthread_exit(NULL);
}


// memory allocation for grid
void allocate_memory(int l, int w, int **grid_flat, int ***grid) {
    int count = l * w;
    *grid_flat = (int *)malloc(sizeof(int) * count);
    *grid = (int **)malloc(sizeof(int *) * w);
    int i;

    for (i = 0; i < w; i++) {
        (*grid)[i] = &((*grid_flat)[i * l]);
    }
}

// initialize grid
void init_grid(int n, int ***grid) {
    time_t s;
    srand((unsigned)time(&s));
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            (*grid)[i][j] = rand() % 3;
        }
    }
}

// print grid
void print_grid(int n, int ***grid) {
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            printf("%d", (*grid)[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// calculate the tasks distributed to each thread
int* distribute_task_for_processes(int num_threads, int num_tasks) {
    int *task = (int *)malloc(sizeof(int) * (num_threads)); // array 'row' store the number of row distributed to each process
    int base = num_tasks / num_threads;
    int remain = num_tasks % num_threads;
    int i;

    // calculate the tile row distributed to each process
    for (i = 0; i < num_threads; i++) {
        task[i] = base;
    }
    for (i = 0; i < remain; i++) {
        task[i] = task[i] + 1;
    }

    return task;
}

// red color movement
void do_red(int l, int begin, int end, int ***grid) {
    int i, j;
    for (i = begin; i < end; i++) {
        //the first column.
        if ((*grid)[i][0] == 1 && (*grid)[i][1] == 0) {
            (*grid)[i][0] = 4;
            (*grid)[i][1] = 3;
        }
        //the rest column.
        for (j = 1; j < l; j++) {
            if ((*grid)[i][j] == 1 && ((*grid)[i][(j + 1) % l] == 0)) {
                (*grid)[i][j] = 0;
                (*grid)[i][(j + 1) % l] = 3;
            }
            else if ((*grid)[i][j] == 3)
                (*grid)[i][j] = 1;
        }
        //cast back to the changed colours.
        if ((*grid)[i][0] == 3)
            (*grid)[i][0] = 1;
        else if ((*grid)[i][0] == 4)
            (*grid)[i][0] = 0;
    }
}