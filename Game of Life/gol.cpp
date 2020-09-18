#include "gol.h"
#include <pthread.h>
#include <semaphore.h>

int count = 0;
int num_iter;
int NUM_OF_CELLS;
int stride;
pthread_mutex_t mutex;
sem_t barrier1, barrier2;

/* true  : alive
   false : dead
   9 = number of neighbors
   0 - topLeft
   1 - top
   2 - topRight
   3 - left
   4 - center
   5 - right
   6 - botLeft
   7 - bottom
   8 - botRight */
struct neighbors {
    bool cell[9];
    int cell_index;
};
std::vector<neighbors> nbs;

void *gol_algorithm(void *p)
{
    neighbors neighbor = *reinterpret_cast<neighbors*>(p);

    for (int i = 0; i < num_iter; ++i)
    {
        /* read data from neighbors */
        neighbor = nbs[neighbor.cell_index];
        bool old_state = neighbor.cell[4];
        bool new_state = old_state;

        int num_of_live_neighbors = 0;

        for (int j = 0; j < 9; ++j)
        {
            if (neighbor.cell[j] == true && j != 4)
                ++num_of_live_neighbors;
        }

        /* calculate the next state */
        if (old_state == false)
        {
            if (num_of_live_neighbors == 3)
                new_state = true;
        }
        else
        {
            if (num_of_live_neighbors < 2
             || num_of_live_neighbors > 3)
                new_state = false;
        }
        
        /* wait all threads to finish their calculations */
        pthread_mutex_lock(&mutex);
        count = count + 1;
        if (count == NUM_OF_CELLS)
        {
            sem_wait(&barrier2);
            sem_post(&barrier1);
        }
        pthread_mutex_unlock(&mutex);

        sem_wait(&barrier1);
        sem_post(&barrier1);

        /* write the state back to array */
        neighbor.cell[4] = new_state;

        if (old_state != new_state)
        {
            for (int k = -1; k <= 1; ++k)
            {
                int cell_i_x = neighbor.cell_index + stride * k;

                if (cell_i_x < 0 || cell_i_x >= NUM_OF_CELLS)
                    continue;

                for (int j = -1; j <= 1; ++j)
                {
                    int cell_i_y = neighbor.cell_index % stride;

                    if ((j == -1 && cell_i_y == 0)
                     || (j ==  1 && cell_i_y == stride -1))
                        continue;

                    int cell_i = cell_i_x + j;
                    int cell_dir = 4 + (-1)*k + (-3)*j;
                    nbs[cell_i].cell[cell_dir] = new_state;
                }
            }
        }

        /* wait all threads to finish their writes */
        pthread_mutex_lock(&mutex);
        count = count - 1;
        if (count == 0)
        {
            sem_wait(&barrier1);
            sem_post(&barrier2);
        }

        pthread_mutex_unlock(&mutex);

        sem_wait(&barrier2);
        sem_post(&barrier2);
    }
    return 0;
}

std::vector< std::tuple<int,int> >
run( std::vector< std::tuple<int,int> > initial_population, int num_iter, int max_x, int max_y )
{
    /* initialize data */
    stride = max_y;
    NUM_OF_CELLS = max_x * max_y;
    ::num_iter = num_iter;
    nbs.resize(NUM_OF_CELLS);

    /* store the index of the cell */
    for (int i = 0; i < NUM_OF_CELLS; ++i)
        nbs[i].cell_index = i;

    /* initialize the neighbor state with initial population info */
    for (unsigned k = 0; k < initial_population.size(); ++k)
    {
        int alive_cell_x, alive_cell_y;
        std::tie(alive_cell_x, alive_cell_y) = initial_population[k];

        for (int i = -1; i <= 1; ++i)
        {
            int cell_i_x = alive_cell_x + i;

            if (cell_i_x < 0 || cell_i_x >= max_x)
                continue;

            for (int j = -1; j <= 1; ++j)
            {
                int cell_i_y = alive_cell_y + j;

                if (cell_i_y < 0 || cell_i_y >= max_y)
                    continue;

                int cell_i = stride * cell_i_x + cell_i_y;
                int cell_dir = 4 + (-1)*i + (-3)*j;
                nbs[cell_i].cell[cell_dir] = true;
            }
        }
    }

    /* create threads one per cell */
    std::vector<pthread_t> threads_id(NUM_OF_CELLS);

    pthread_mutex_init(&mutex, 0);
    sem_init(&barrier1, 0, 0);  
    sem_init(&barrier2, 0, 1);

    for (int i = 0; i < NUM_OF_CELLS; ++i)
        pthread_create(&threads_id[i], 0, gol_algorithm, &nbs[i]);

    for (int i = 0; i < NUM_OF_CELLS; ++i)
        pthread_join(threads_id[i], 0);

    /* delete threads */
    pthread_mutex_destroy(&mutex);
    sem_destroy(&barrier1);
    sem_destroy(&barrier2);

    /* output the result */
    std::vector<std::tuple<int, int>> result;
    for (int i = 0; i < NUM_OF_CELLS; ++i)
    {
        if (nbs[i].cell[4] == true)
        {
            int x = i / stride;
            int y = i % stride;

            result.push_back(std::make_tuple(x, y));
        }
    }

    return result;
}