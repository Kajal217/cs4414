#include "life.h"
#include <pthread.h>
#include <cmath>
#include <sys/wait.h>

struct arguments {
  LifeBoard state;
  LifeBoard next_state;
  int steps;
  int startIndex;
  int endIndex;
  pthread_barrier_t* barrier;
};

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    LifeBoard next_state{state.width(), state.height()};
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads+1);
    pthread_t life_threads[8];
    int cellsPerThread = ceil((state.height()-2)*(state.width()-2)/threads);

    //args: [board pointer, next pointer, numSteps, numThreads, startIndex, endIndex]
    struct arguments args;
    args.state = state;
    args.next_state = next_state;
    args.steps = steps;
    args.startIndex = 0;
    args.endIndex = cellsPerThread;
    args.barrier = &barrier;

    for (int i=0; i<threads; i++) {
        if (i==threads-1) args.endIndex = (state.height()-2)*(state.width()-2);
        pthread_create(&life_threads[i], NULL, thread_simulate, (void*)&args);
        args.startIndex += cellsPerThread;
        args.endIndex += cellsPerThread;
    }

    for (int step = 0; step < steps; ++step) {
        pthread_barrier_wait(&barrier);
        swap(state, next_state);
    }

    for (int j=0; j<threads; j++) {
      pthread_join(life_threads[j], NULL);
    }
    pthread_barrier_destroy(&barrier);

}

void* thread_simulate(void* a) {

  struct arguments *arguments = (struct arguments*) a;

    LifeBoard state = arguments->state;
    LifeBoard next_state = arguments->next_state;
    int steps = arguments->steps;
    int startIndex = arguments->startIndex;
    int endIndex = arguments->endIndex;
    pthread_barrier_t* barrier = arguments->barrier;

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
    return NULL;
}
