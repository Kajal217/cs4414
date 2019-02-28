#include "life.h"
#include <pthread.h>
#include <sys/wait.h>
#include <stdio.h>
#include <iostream>

pthread_barrier_t barrier;

struct thread_arguments {
  LifeBoard* state;
  LifeBoard* next_state;
  int steps;
  int startIndex;
  int endIndex;
  //pthread_barrier_t* barrier;
};

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    LifeBoard next_state{state.width(), state.height()};
    //pthread_barrier_t barrier;
    pthread_t life_threads[9];
    int gameCells = (state.height()-2)*(state.width()-2);
    int cellsPerThread = (gameCells+threads-1)/threads; // ceiling of (cells/threads)

    pthread_barrier_init(&barrier, NULL, threads+1);
    
    //args: [board pointer, next pointer, numSteps, numThreads, startIndex, endIndex]
    struct thread_arguments args[9];
    args[0].startIndex = 0;
    args[0].endIndex = cellsPerThread;

    for (int i=0; i<threads; i++) {
        args[i].state = &state;
        args[i].next_state = &next_state;
        args[i].steps = steps;
	//args[i].barrier = &barrier;
        if (i==threads-1) args[i].endIndex = (state.height()-2)*(state.width()-2);
        pthread_create(&life_threads[i], NULL, thread_simulate, (void*)&args[i]);
        if (i<threads-1) {
	  args[i+1].startIndex = args[i].endIndex;
          args[i+1].endIndex = args[i+1].startIndex + cellsPerThread;
	}
    }

    for (int step = 0; step < steps; ++step) {
        pthread_barrier_wait(&barrier);
        swap(state, next_state);
	pthread_barrier_wait(&barrier);
    }

    for (int j=0; j<threads; j++) {
      pthread_join(life_threads[j], NULL);
    }
    pthread_barrier_destroy(&barrier);

}

void* thread_simulate(void* a) {

  struct thread_arguments *arguments = (struct thread_arguments*) a;

    LifeBoard* state = arguments->state;
    LifeBoard* next_state = arguments->next_state;
    int steps = arguments->steps;
    int startIndex = arguments->startIndex;
    int endIndex = arguments->endIndex;
    //pthread_barrier_t* barrier = arguments->barrier;
    int index;
    
    // each iteration = 1 generation
    for (int step = 0; step < steps; ++step) {
        // operate on cells from start to end index
        index = 0;

        for (int y = 1; y < state->height() - 1; ++y) {
            for (int x = 1; x < state->width() - 1; ++x) {
                if (index < startIndex) {
                    index++;
                    continue;
                }
                if (index == endIndex) break;

                int live_in_window = 0;
                // For each cell, examine a 3x3 "window" of cells around it,
                // and count the number of live (true) cells in the window.
                for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (state->at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }
                // Cells with 3 live neighbors remain or become live.
                // Live cells with 2 live neighbors remain live.
                next_state->at(x, y) = (
                    live_in_window == 3  ||
                    (live_in_window == 4 && state->at(x, y))
                );

                index++;
            }
            if (index == endIndex) break;
        }

        // this generation's work is done. wait for all threads before moving on...
        pthread_barrier_wait(&barrier);
        pthread_barrier_wait(&barrier);	
    }
    return NULL;
}


/*
struct arguments {
  LifeBoard eState;
  LifeBoard oState;
  int steps;
  int startIndex;
  int endIndex;
  pthread_barrier_t* barrier;
};

void simulate_life_parallel(int threads, LifeBoard &eState, int steps) {
    LifeBoard oState{eState.width(), eState.height()};
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads+1);
    pthread_t life_threads[8];
    int cellsPerThread = ceil((eState.height()-2)*(eState.width()-2)/threads);

    //args: [board pointer, next pointer, numSteps, numThreads, startIndex, endIndex]
    struct arguments args;
    args.eState = eState;
    args.oState = oState;
    args.steps = steps;
    args.startIndex = 0;
    args.endIndex = cellsPerThread;
    args.barrier = &barrier;

    for (int i=0; i<threads; i++) {
        if (i==threads-1) args.endIndex = (eState.height()-2)*(eState.width()-2);
        pthread_create(&life_threads[i], NULL, thread_simulate, (void*)&args);
        args.startIndex += cellsPerThread;
        args.endIndex += cellsPerThread;
    }

    for (int step = 0; step < steps; ++step) {
        pthread_barrier_wait(&barrier);
        //swap(state, next_state);
    }

    for (int j=0; j<threads; j++) {
      pthread_join(life_threads[j], NULL);
    }

    if (steps%2==1){
        swap(eState, oState);
    }

    pthread_barrier_destroy(&barrier);

}

void* thread_simulate(void* a) {

  struct arguments *arguments = (struct arguments*) a;

    LifeBoard eState = arguments->eState;
    LifeBoard oState = arguments->oState;
    int steps = arguments->steps;
    int startIndex = arguments->startIndex;
    int endIndex = arguments->endIndex;
    pthread_barrier_t* barrier = arguments->barrier;

    // each iteration = 1 generation
    for (int step = 0; step < steps; ++step) {
        // operate on cells from start to end index
        int index = 0;

        for (int y = 1; y < eState.height() - 1; ++y) {
            for (int x = 1; x < eState.width() - 1; ++x) {
                if (index < startIndex) {
                    index++;
                    continue;
                }
                if (index == endIndex) break;

                int live_in_window = 0;
                // For each cell, examine a 3x3 "window" of cells around it,
                // and count the number of live (true) cells in the window.
                for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (step%2==0 && eState.at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                        else if (step%2==1 && oState.at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }
                // Cells with 3 live neighbors remain or become live.
                // Live cells with 2 live neighbors remain live.
                if (step%2==0){
                    oState.at(x, y) = (
                        live_in_window == 3  ||
                        (live_in_window == 4 && eState.at(x, y))
                    );
                }
                else if (step%2==1){
                    eState.at(x, y) = (
                        live_in_window == 3 ||
                        (live_in_window == 4 && oState.at(x, y))
                    );
                }

                index++;
            }
            if (index == endIndex) break;
        }

        // this generation's work is done. wait for all threads before moving on...
        pthread_barrier_wait(barrier);
    }
    return NULL;
}
*/
