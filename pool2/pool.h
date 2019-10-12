#ifndef POOL_H_
#include <string>
#include <deque>
#include <vector>
#include <pthread.h>
#include <semaphore.h>

#define TASK_AVAILABLE 0
#define TASK_RUNNING 1
#define TASK_DONE 2

class Task {
public:
    std::string name;
    pthread_mutex_t task_mutex;
    pthread_cond_t task_cond;   // broadcasts when task completes
    int status;

    Task();
    virtual ~Task();

    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:
    std::deque<Task*> pool_tasks;
    std::vector<pthread_t> pool_threads;
    pthread_mutex_t pool_mutex; // protects the task queue
    sem_t pool_semaphore;   // tracks number of available tasks
    bool stop;

    ThreadPool(int num_threads);

    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();
};
#endif
