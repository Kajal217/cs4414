#ifndef POOL_H_
#include <string>
#include <pthread.h>
#include <dequeue>

class Task {
public:
    std::string name
    int status; //0:waiting, 1:running, 2:done
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    Task();
    virtual ~Task();

    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:
    std::dequeue<Task*> taskList;
    pthread_t thread;
    pthread_mutex_t pool_mutex;

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
