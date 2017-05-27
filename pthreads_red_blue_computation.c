#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define RED 1
#define BLUE 2
#define BOTH 3

// public variables
// struct for thread data
struct thread_data {
    int id;
    int row_column_begin;
    int row_column_end;
    int tile_begin;
    int tile_end;
};

// struct for barrier function
typedef struct {
    pthread_mutex_t count_lock;
    pthread_cond_t ok_to_proceed;
    int count;
} mylib_barrier_t;

int *grid_flat;    // one-dimension version of grid
int **grid;    // two-dimension grid
int num_threads, n, t, c, max_iters; // the number of threads,  n-cell grid size, t-tile grid size, c-terminating threshold, max_iters- maximum number of iterations
double threshold; // the threshold shown in percentage
int tile_number; // the number of tile in the grid
int finished_flag = 0; // the flag showing whether the iterations finish or not
mylib_barrier_t barrier; // barrier struct
double *red_blue_array; // the array storing the check tile result
int n_itrs; // the iteration times

// declare the functions
void mylib_barrier_init(mylib_barrier_t *b);

void mylib_barrier(mylib_barrier_t *b, int num_threads);

void mylib_barrier_destroy(mylib_barrier_t *b);

void *red_blue_computation(void *threadarg);

void allocate_memory(int l, int w, int **grid_flat, int ***grid);

void init_grid(int n, int ***grid);

void print_grid(int n, int ***grid);

int *distribute_task_for_processes(int num_threads, int num_tasks);

void do_red(int l, int begin, int end, int ***grid);

void do_blue(int w, int begin, int end, int ***grid);

int check_tile(double **red_blue_array, int ***grid, int begin, int end, int n, int t, double threshold);

void print_computation_result(double **red_blue_array, int tile_number);

void sequential_computation(int ***grid,  int n, int t, double threshold, int max_iters);

void self_check(int ***grid, int ***grid_copy, int n);

int main(int argc, char **argv) {
    int *grid_flat_copy; // the copy of one-dimension version of grid
    int **grid_copy; // the copy of the initial two-dimension grid
    int i, j, k;

    if (argc != 6) {
        printf("Wrong number of arguments.\n");
        printf("Please enter the command in the following format:\n");
        printf("./pthreads_red_blue_computation [the number of threads] [cell grid size] [tile grid size] [terminating threshold] [maximum number of iterations]\n");
        printf("Note: [cell grid size] % [tile grid size] = 0; [the number of threads >= 0]\n");
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

    if ((n % t != 0) || (num_threads < 0)) {
        printf("Illegal arguments.\n");
        printf("Please enter the command in the following format:\n");
        printf("./pthreads_red_blue_computation [the number of threads] [cell grid size] [tile grid size] [terminating threshold] [maximum number of iterations]\n");
        printf("Note: [cell grid size] % [tile grid size] = 0; [the number of threads >= 0]\n");
        printf("\n");
        return 0;
    }
    
    // initialize the grid and print it out
    allocate_memory(n, n, &grid_flat, &grid);
    allocate_memory(n, n, &grid_flat_copy, &grid_copy);

    init_grid(n, &grid);
    memcpy(grid_flat_copy, grid_flat, sizeof(int) * n * n);

    printf("The initial grid: \n");
    print_grid(n, &grid);

    if (num_threads == 0){
        sequential_computation(&grid_copy, n, t, threshold, max_iters);
        return 0;
    }

    tile_number = t * t;
    pthread_t threads[num_threads];
    struct thread_data thread_data_array[num_threads];
    red_blue_array = (double *)malloc(sizeof(double) * tile_number * 3);

    int *row_column_task = distribute_task_for_processes(num_threads, n);
    int *tile_task = distribute_task_for_processes(num_threads, tile_number);

    // initialize the barrier
    mylib_barrier_init(&barrier);

    // distribute the workload among the threads
    for (i = 0; i < num_threads; i++) {
        // initialize the struct argument
        thread_data_array[i].id = i;
        if (i == 0) {
            thread_data_array[i].row_column_begin = 0;
            thread_data_array[i].tile_begin = 0;
        } else {
            thread_data_array[i].row_column_begin = thread_data_array[i - 1].row_column_end;
            thread_data_array[i].tile_begin = thread_data_array[i - 1].tile_end;
        }
        thread_data_array[i].row_column_end = thread_data_array[i].row_column_begin + row_column_task[i];
        thread_data_array[i].tile_end = thread_data_array[i].tile_begin + tile_task[i];

        pthread_create(&threads[i], NULL, red_blue_computation, (void *) &thread_data_array[i]);
    }

    // wait all the threads finish
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("The parallel computation result: \n");
    printf("After %d interations, the final grid: \n", n_itrs);
    print_grid(n, &grid);
    print_computation_result(&red_blue_array, tile_number);

    sequential_computation(&grid_copy, n, t, threshold, max_iters);
    self_check(&grid, &grid_copy, n);

    // clean up and exit
    mylib_barrier_destroy(&barrier);
    pthread_exit(NULL);
}

//
void *red_blue_computation(void *thread_arg) {
    int finished_flag_p = 0;
    int n_itrs_p = 0; // the iteration times
    struct thread_data *my_data = (struct thread_data *) thread_arg;

    while (!finished_flag && n_itrs_p < max_iters) {
        n_itrs_p = n_itrs_p + 1; // renew the iteration number

        do_red(n, my_data->row_column_begin, my_data->row_column_end, &grid);
        mylib_barrier(&barrier, num_threads);
        do_blue(n, my_data->row_column_begin, my_data->row_column_end, &grid);
        mylib_barrier(&barrier, num_threads);
        finished_flag_p = check_tile(&red_blue_array, &grid, my_data ->tile_begin, my_data ->tile_end, n, t, threshold);
        if (finished_flag_p == 1){
            finished_flag = finished_flag_p;
        }
        mylib_barrier(&barrier, num_threads);
    }

    if (my_data -> id == 0){
        n_itrs = n_itrs_p;
    }

    pthread_exit(NULL);
}

// barrier initialization
void mylib_barrier_init(mylib_barrier_t *b) {
    b->count = 0;
    pthread_mutex_init(&(b->count_lock), NULL);
    pthread_cond_init(&(b->ok_to_proceed), NULL);
}

// barrier function
void mylib_barrier(mylib_barrier_t *b, int num_threads) {
    pthread_mutex_lock(&(b->count_lock));
    b->count = b->count + 1;
    if (b->count == num_threads) {
        b->count = 0;
        pthread_cond_broadcast(&(b->ok_to_proceed));
    } else {
        while (pthread_cond_wait(&(b->ok_to_proceed), &(b->count_lock)) != 0);
    }
    pthread_mutex_unlock(&(b->count_lock));
}

// barrier destroy
void mylib_barrier_destroy(mylib_barrier_t *b) {
    pthread_mutex_destroy(&(b->count_lock));
    pthread_cond_destroy(&(b->ok_to_proceed));
}

// memory allocation for grid
void allocate_memory(int l, int w, int **grid_flat, int ***grid) {
    int count = l * w;
    *grid_flat = (int *) malloc(sizeof(int) * count);
    *grid = (int **) malloc(sizeof(int *) * w);
    int i;

    for (i = 0; i < w; i++) {
        (*grid)[i] = &((*grid_flat)[i * l]);
    }
}

// initialize grid
void init_grid(int n, int ***grid) {
    time_t s;
    srand((unsigned) time(&s));
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
int *distribute_task_for_processes(int num_threads, int num_tasks) {
    int *task = (int *) malloc(
            sizeof(int) * (num_threads)); // array 'row' store the number of row distributed to each process
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
            } else if ((*grid)[i][j] == 3)
                (*grid)[i][j] = 1;
        }
        //cast back to the changed colours.
        if ((*grid)[i][0] == 3)
            (*grid)[i][0] = 1;
        else if ((*grid)[i][0] == 4)
            (*grid)[i][0] = 0;
    }
}

// blue color movement
void do_blue(int w, int begin, int end, int ***grid) {
    int i, j;
    for (j = begin; j < end; j++) {
        if ((*grid)[0][j] == 2 && (*grid)[1][j] == 0) {
            (*grid)[0][j] = 4;
            (*grid)[1][j] = 3;
        }
        for (i = 1; i < w; i++) {
            if ((*grid)[i][j] == 2 && (*grid)[(i + 1) % w][j] == 0) {
                (*grid)[i][j] = 0;
                (*grid)[(i + 1) % w][j] = 3;
            } else if ((*grid)[i][j] == 3)
                (*grid)[i][j] = 2;
        }
        if ((*grid)[0][j] == 3)
            (*grid)[0][j] = 2;
        else if ((*grid)[0][j] == 4)
            (*grid)[0][j] = 0;
    }
}

// check whether there is any tile exceeding the threshold
int check_tile(double **red_blue_array, int ***grid, int begin, int end, int n, int t, double threshold) {
    double tile_count = (n * n) / (t * t); // the number of cells in each tile
    int tile_size = n / t; // the size of the tile
    int tile_row, tile_column; // the index of tile in tile grid
    int redcount = 0, bluecount = 0;
    double red_ratio, blue_ratio;
    int finished_flag = 0;
    int i, j, k;

    for (i = begin; i < end; i++) {
        tile_row = i / t;
        tile_column = i % t;
        for (j = tile_size * tile_row; j < tile_size * tile_row + tile_size; j++) {
            for (k = tile_size * tile_column; k < tile_size * tile_column + tile_size; k++) {
                if ((*grid)[j][k] == 1) {
                    redcount = redcount + 1;
                }
                if ((*grid)[j][k] == 2) {
                    bluecount = bluecount + 1;
                }
            }
        }

        red_ratio = redcount / tile_count;
        blue_ratio = bluecount / tile_count;
        red_ratio = (int) (100.0 * red_ratio + 0.5) / 100.0;
        blue_ratio = (int) (100.0 * blue_ratio + 0.5) / 100.0;
        (*red_blue_array)[3 * i + 1] = red_ratio;
        (*red_blue_array)[3 * i + 2] = blue_ratio;

        if (red_ratio > threshold) {
            (*red_blue_array)[3 * i] = RED;
            finished_flag = 1;
        }

        if (blue_ratio > threshold) {
            (*red_blue_array)[3 * i] = BLUE;
            finished_flag = 1;
        }

        if (blue_ratio > threshold && red_ratio > threshold) {
            (*red_blue_array)[3 * i] = BOTH;
            finished_flag = 1;
        }

        redcount = 0;
        bluecount = 0;
    }
    return finished_flag;
}

// print which tile exceed the threshold
void print_computation_result(double **red_blue_array, int tile_number) {
    int exceed_tile = 0;
    int i;

    for (i = 0; i < tile_number; i++) {
        if ((*red_blue_array)[3 * i] == RED) {
            printf("In tile %d, the red color exceeds the threshold with the ratio %.2lf.\n", i, (*red_blue_array)[3 * i + 1]);
            exceed_tile = exceed_tile + 1;
        }

        if ((*red_blue_array)[3 * i] == BLUE) {
            printf("In tile %d, the blue color exceeds the threshold with the ratio %.2lf.\n", i, (*red_blue_array)[3 * i + 2]);
            exceed_tile = exceed_tile + 1;
        }

        if ((*red_blue_array)[3 * i] == BOTH) {
            printf("In tile %d, the red color exceeds the threshold with the ratio %.2lf and the blue color exceeds the threshold with the ratio %.2lf.\n", i, (*red_blue_array)[3 * i + 1], (*red_blue_array)[3 * i + 2]);
            exceed_tile = exceed_tile + 1;
        }
    }

    if (exceed_tile == 0) {
        printf("There is no tile containning color exceeding threshold.\n");
        printf("The computation terminated because the maximum iteration number has been reached.\n");
    }
    printf("\n");
}

// sequential program for red/blue computation
void sequential_computation(int ***grid,  int n, int t, double threshold, int max_iters) {
    int finished_flag = 0;
    int n_itrs = 0;
    int tile_number = t * t;
    double *red_blue_array = (double *)malloc(sizeof(double) * tile_number * 3);	// store the red and blue ratio in grid

    while (!finished_flag && n_itrs < max_iters) {
        n_itrs = n_itrs + 1; // renew the iteration number

        do_red(n, 0, n, grid);
        do_blue(n, 0, n, grid);

        finished_flag = check_tile(&red_blue_array, grid, 0, tile_number, n, t, threshold);
    }

    printf("The sequential computation result: \n");
    printf("After %d interations, the final grid: \n", n_itrs);
    print_grid(n, grid);
    print_computation_result(&red_blue_array, tile_number);
}

// self-checking program
void self_check(int ***grid, int ***grid_copy, int n) {
    int flag = 0;
    int i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if ((*grid)[i][j] != (*grid_copy)[i][j]) {
                flag = 1;
                break;
            }
            if (flag == 1) {
                break;
            }
        }
    }

    if (flag == 0) {
        printf("The results of parallel program and sequential program are identical.\n");
        printf("The program is correct.\n");
    }
    else {
        printf("The results of parallel program and sequential program are not identical.\n");
        printf("The program is wrong.\n");
    }
    printf("\n");
}
