#include "pool.h"
#include <iostream>

Task::Task() {
    name = "";
    status = 0;
    mutex = PTHREAD_MUTEX_INITIALIZER;
    cond = PTHREAD_COND_INITIALIZER;
}

Task::~Task() {
  //delete &name;
  //delete &status;
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

ThreadPool::ThreadPool(int num_threads) {
    thread_count = num_threads;
    stop_call = false;
    threadList = std::vector<pthread_t>(num_threads);
    pool_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_mutex_lock(&pool_mutex);
    for (int i=0; i<num_threads; i++) {
      pthread_create(&(threadList[i]), NULL, runThread, (void*)this);
    }
    pthread_mutex_unlock(&pool_mutex);
}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    task->name = name;
    task->status = 0;
    pthread_mutex_lock(&pool_mutex);
    taskList.push_back(task);
    pthread_mutex_unlock(&pool_mutex);
}

void ThreadPool::WaitForTask(const std::string &name) {
    Task* task = NULL;
    bool done = false;
    pthread_mutex_lock(&pool_mutex);
    for (uint j=0; j<taskList.size(); j++) {
        if (taskList[j]->name == name) task = taskList[j];
    }
    if (task->status==2) done = true;
    pthread_mutex_unlock(&pool_mutex);

    //wait for task to finish running, unless the task is already done
    if (!done) {
      pthread_mutex_lock(&(task->mutex));
      pthread_cond_wait(&(task->cond), &(task->mutex));
      pthread_mutex_unlock(&(task->mutex));
    }

    //erase task from queue, delete task
    pthread_mutex_lock(&pool_mutex);
    for (uint j=0; j<taskList.size(); j++) {
        if (taskList[j]->name == name) taskList.erase(taskList.begin()+j);
    }
    pthread_mutex_unlock(&pool_mutex);
    if (task!=NULL) delete task;
}

// terminate thread pool, deallocate resources
void ThreadPool::Stop() { //pthread_join or cancel??
  pthread_mutex_lock(&pool_mutex);
  stop_call = true;
  pthread_mutex_unlock(&pool_mutex);
  
    for (int i=0; i<thread_count; i++) {
      //pthread_cancel(threadList[i]);
      pthread_join(threadList[i], NULL);
    }
    
    pthread_mutex_lock(&pool_mutex);
    taskList.clear();
    threadList.clear();
    pthread_mutex_unlock(&pool_mutex);
    pthread_mutex_destroy(&pool_mutex);
}

// thread searches for tasks to run
void ThreadPool::ConsumeTasks(){
    Task* task;
    for(;;) {

        if (stop_call) pthread_exit(NULL);
	
        pthread_mutex_lock(&pool_mutex);
        task = NULL;
        //find a waiting task
        if (!taskList.empty()){
            for (uint i=0; i<taskList.size(); i++) {
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

// each thread is sent here (can't be member function), then to ConsumeTasks()
void* runThread(void* threadPool) {
  ((ThreadPool*)threadPool)->ConsumeTasks();
  return NULL;
}
