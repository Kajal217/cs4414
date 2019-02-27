#include "life.h"
#include <pthread.h>
#include <cmath>
#include <sys/wait.h>

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    LifeBoard next_state{state.width(), state.height()};
    pthread_barrier_t* barrier;
    pthread_t life_threads[threads];
    int cellsPerThread = ceil((state.height()-2)*(state.width()-2)/threads);

    //args: [board pointer, next pointer, numSteps, numThreads, startIndex, endIndex]
    void* arguments[8];
    arguments[0] = state;
    arguments[1] = next_state;
    arguments[2] = steps;
    arguments[3] = threads;
    arguments[4] = 0;
    arguments[5] = cellsPerThread;
    arguments[6] = barrier;

    for (int i=0; i<threads; i++) {
        if (i==threads-1) arguments[5] = (state.height()-2)*(state.width()-2);
        pthread_create(&life_threads[i], NULL, thread_simulate, arguments);
        arguments[4] += cellsPerThread;
        arguments[5] += cellsPerThread;
    }

    for (int step = 0; step < steps; ++step) {
        pthread_barrier_init(barrier, NULL, threads);
        swap(state, next_state);
        pthread_barrier_destroy(barrier);
    }

}

void* thread_simulate(void* arguments) {

    LifeBoard state = arguments[0];
    LifeBoard next_state = arguments[1];
    int steps = arguments[2];
    int threads = arguments[3];
    int startIndex = arguments[4];
    int endIndex = arguments[5];
    pthread_barrier_t* barrier = arguments[6];
    int* wait_status;

    // each iteration = 1 generation
    for (int step = 0; step < steps; ++step) {
        // operate on cells from start to end index
        int index = 0;

        for (int y = 1; y < state.height() - 1; ++y) {
            for (int x = 1; x < state.width() - 1; ++x) {
                if (index < startIndex) {
                    index++;
                    continue;
                }
                if (index == endIndex) break;

                int live_in_window = 0;
                /* For each cell, examine a 3x3 "window" of cells around it,
                 * and count the number of live (true) cells in the window. */
                for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (state.at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }
                /* Cells with 3 live neighbors remain or become live.
                   Live cells with 2 live neighbors remain live. */
                next_state.at(x, y) = (
                    live_in_window == 3 /* dead cell with 3 neighbors or live cell with 2 */ ||
                    (live_in_window == 4 && state.at(x, y)) /* live cell with 3 neighbors */
                );

                index++;
            }
            if (index == endIndex) break;
        }

        // this generation's work is done. wait for all threads before moving on...
        pthread_barrier_wait(barrier);
    }

}