#include "pool.h"

void* runTasks(void* pool) {
    ThreadPool* threadPool = (ThreadPool*)pool;
    Task* task;
    while (!threadPool->stop) {
        sem_wait(&(threadPool->pool_semaphore));    // wait for a task to become available
        pthread_mutex_lock(&(threadPool->pool_mutex));
        for (uint i = 0; i < threadPool->pool_tasks.size(); i++) {
            task = threadPool->pool_tasks[i];
            if (task->status == TASK_AVAILABLE) {
                task->status = TASK_RUNNING;
                break;
            }
        }
        pthread_mutex_unlock(&(threadPool->pool_mutex));

        // run the task, then alert other threads of its completion
        task->Run();
        // ***
        task->status = TASK_DONE;
        pthread_cond_broadcast(&task->task_cond);
    }
    return;
}

Task::Task() {
    name = "";
    task_mutex = PTHREAD_MUTEX_INITIALIZER;
    task_cond = PTHREAD_COND_INITIALIZER;
    status = TASK_AVAILABLE;
}

Task::~Task() {
    pthread_mutex_destroy(&task_mutex);
    pthread_cond_destroy(&task_cond);
}

ThreadPool::ThreadPool(int num_threads) {
    pool_mutex = PTHREAD_MUTEX_INITIALIZER;
    sem_init(&pool_semaphore, 0, 0);
    pool_threads = std::vector<pthread_t>(num_threads);
    stop = false;
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool_threads[i], NULL, runTasks, void*(this));
    }

}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    task->name = name;
    pthread_mutex_lock(&pool_mutex);
    pool_tasks.push_back(task);
    sem_post(&pool_semaphore);  // alert threads of newly available task
    pthread_mutex_unlock(&pool_mutex);
}

void ThreadPool::WaitForTask(const std::string &name) {
    // identify the task and remove it from the queue
    Task* task;
    pthread_mutex_lock(&pool_mutex);
    for (uint i = 0; i < pool_tasks.size(); i++) {
        if (pool_tasks[i]->name == name) {
            task = pool_tasks[i];
            pool_tasks.erase(pool_tasks.begin() + i);
            break;
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    // if the task isn't already done, wait until it is
    pthread_mutex_lock(&(task->task_mutex));
    while (task->status != TASK_DONE) {
        pthread_cond_wait(&(task->task_cond), &(task->task_mutex));
    }
    pthread_mutex_unlock(&(task->task_mutex));

    delete task;
}

void ThreadPool::Stop() {
    // wait for threads to finish their current task and join
    stop = true;
    for (uint i = 0; i < pool_threads.size(); i++) {
        pthread_join(pool_threads[i], NULL);
    }

    // deallocate thread pool resources
    delete pool_tasks;
    delete pool_threads;
    pthread_mutex_destroy(&pool_mutex);
    sem_destroy(&pool_semaphore);
}