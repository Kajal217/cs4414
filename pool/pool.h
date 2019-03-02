#ifndef POOL_H_
#include <string>
#include <pthread.h>
//#include <deque>
#include <vector>

class Task {
public:
    std::string name;
    int status; //0:waiting, 1:running, 2:done
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    Task();
    virtual ~Task();

    virtual void Run() = 0;  // implemented by subclass
};

class ThreadPool {
public:
    std::vector<Task*> taskList;
    std::vector<pthread_t> threadList;
    pthread_mutex_t pool_mutex;
    int thread_count;
    bool stop_call;

    ThreadPool(int num_threads);

    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();

    // send threads here to await and run tasks
    void ConsumeTasks();
};

extern void* runThread(void* args);
#endif
