#include "pool.h"

Task::Task() {
    name = "";
    status = 0;
    mutex = PTHREAD_MUTEX_INITIALIZER;
    cond = PTHREAD_COND_INITIALIZER;
}

Task::~Task() {
    delete name;
    delete status;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

ThreadPool::ThreadPool(int num_threads) {
    thread_count = num_threads;
    pool_mutex = PTHREAD_MUTEX_INITIALIZER;
    thread = malloc(sizeof(pthread_t)*num_threads);
    for (int i=0; i<num_threads; i++) {
        pthread_create(&thread[i], NULL, consumeTasks, NULL);
    }
}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    task->name = name;
    pthread_mutex_lock(&pool_mutex);
    taskList.push_back(task);
    pthread_mutex_unlock(&pool_mutex);
}

void ThreadPool::WaitForTask(const std::string &name) {
    Task* task;
    pthread_mutex_lock(&pool_mutex);
    for (int j=0; j<taskList.size(); j++) {
        if (taskList[j]->name == name) task = taskList[j];
    }
    pthread_mutex_unlock(&pool_mutex);

    //wait for task to finish running, unless the task is already done
    if (!task->status==2) {
        pthread_mutex_lock(&task->mutex);
        pthread_cond_wait(&task->cond);
        pthread_mutex_unlock(&task->mutex);
    }

    //erase task from queue, delete task
    pthread_mutex_lock(&pool_mutex);
    for (int j=0; j<taskList.size(); j++) {
        if (taskList[j]->name == name) TaskList.erase(TaskList.begin()+j);
    }
    pthread_mutex_unlock(&pool_mutex);
    delete task;
}

// terminate thread pool, deallocate resources
void ThreadPool::Stop() { //pthread_join or cancel??
    for (int i=0; i<thread; i++) {
        pthread_join(&thread[i], NULL, consumeTasks, NULL);
    }
    pthread_mutex_destroy(&pool_mutex);
}

// each thread is sent here upon creation (should this call a member func?)
void* consumeTasks(void* args){
    Task* task;
    for(;;) {

        pthread_mutex_lock(&pool_mutex);
        task = NULL;
        //find a waiting task
        if (!taskList.empty){
            for (int i=0; i<taskList.size(); i++) {
                if (taskList[i]->status == 0) {
                    taskList[i]->status = 1;
                    task = taskList[i];
                    break;
                }
            }
        }
        pthread_mutex_unlock(&pool_mutex);

        //run available task if found
        if (task != NULL) {
            task->Run();
            //set status to "done", signal task's condition variable
            pthread_mutex_lock(&pool_mutex);
            task->status = 2;
            pthread_mutex_unlock(&pool_mutex);
            pthread_cond_broadcast(&task->cond);
        }

    }
}