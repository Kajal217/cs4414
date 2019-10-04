// Austin Baney // ab5ep
// Based on my own solution from S19
#include "life.h"
#include <pthread.h>
#include <sys/wait.h>

struct thread_args {
  LifeBoard* state;
  LifeBoard* next_state;
  pthread_barrier_t* barrier_ptr;
  int steps;
  int start;    // index of first cell
  int end;      // index after last cell
};

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    LifeBoard next_state{state.width(), state.height()};
    int totalCells = (state.height()-2)*(state.width()-2);
    int cellsPerThread = (totalCells+threads-1)/threads;

    pthread_t lifeThreads[100];
    struct thread_args args[100];

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads+1);

    // fill arguments struct for each thread
    args[0].start = 0;
    args[0].end = cellsPerThread;
    for (int i = 0; i < threads; i++) {
        args[i].state = &state;
        args[i].next_state = &next_state;
        args[i].barrier_ptr = &barrier;
        args[i].steps = steps;
        if (i > 0) {
            int cur_start = args[i-1].end;
            args[i].start = cur_start;
            if (i < threads - 1) {
                args[i].end = cur_start + cellsPerThread;
            } else {    // final thread may work on fewer cells
                args[i].end = totalCells;
            }
        }
        // run this thread
        pthread_create(&lifeThreads[i], NULL, run_thread, (void*)&args[i]);
    }

    for (int i = 0; i < steps; i++) {
        // wait for all threads to complete this step
        pthread_barrier_wait(&barrier);
        swap(state, next_state);
        // threads must wait until swap is complete
	    pthread_barrier_wait(&barrier);
    }

    for (int i = 0; i < threads; i++) {
      pthread_join(lifeThreads[i], NULL);
    }
    pthread_barrier_destroy(&barrier);
}

void* run_thread(void* arguments) {
    struct thread_args* args = (struct thread_args*)arguments;
    LifeBoard* state = args->state;
    LifeBoard* next_state = args->next_state;
    pthread_barrier_t* barrier_ptr = args->barrier_ptr;
    int steps = args->steps;
    int start = args->start;
    int end = args->end;

    for (int step = 0; step < steps; ++step) {
        /* We use the range [1, width - 1) here instead of
         * [0, width) because we fix the edges to be all 0s.
         */
        int index = 0;
        for (int y = 1; y < state->height() - 1; ++y) {
            for (int x = 1; x < state->width() - 1; ++x) {
                // only operate on the specified cells for this thread
                if (index < start) {
                    index++;
                    continue;
                }
                if (index == end) break;

                int live_in_window = 0;
                /* For each cell, examine a 3x3 "window" of cells around it,
                 * and count the number of live (true) cells in the window. */
                for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (state->at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }
                /* Cells with 3 live neighbors remain or become live.
                   Live cells with 2 live neighbors remain live. */
                next_state->at(x, y) = (
                    live_in_window == 3 /* dead cell with 3 neighbors or live cell with 2 */ ||
                    (live_in_window == 4 && state->at(x, y)) /* live cell with 3 neighbors */
                );

                index++;
            }
            if (index == end) break;
        }
        
        // wait for other threads to finish this generation
        pthread_barrier_wait(barrier_ptr);
        // wait for swap before starting next generation
        pthread_barrier_wait(barrier_ptr);
    }
    return NULL;
}
