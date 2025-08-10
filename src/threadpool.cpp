#include "threadpool.h"

#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 10; // 单位：秒

// 线程池构造
ThreadPool::ThreadPool()
    : initThreadsSize_(4)
    , taskSize_(0)
    , idleThreadSize_(0)
    , curThreadsSize_(0)
    , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    , threadSizeThreshHOld_(THREAD_MAX_THRESHHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    , isPoolRunning_(false)
{}

// 线程池析构
ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    // 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock,
        [&]()->bool { return threads_.size() == 0;});
    

}



// 设置线程池模式
void ThreadPool::setPoolMode(PoolMode mode)
{
    if (checkRunningState())
        return ;
    poolMode_ = mode;
}


// 设置task任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshHold)
{
    if (checkRunningState())
        return ;
    taskQueMaxThreshHold_ = threshHold;
}

// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshHold)
{
    if (checkRunningState())
        return ;
    if( poolMode_ == PoolMode::MODE_CACHED)
    {
        threadSizeThreshHOld_ = threshHold;
    }

}

// 给线程池添加任务  用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 线程的通信  等待任务队列有空余  wait  wait_for  wait_until
    // 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
    if(!notFull_.wait_for(lock, std::chrono::seconds(1), 
                [&]()->bool { return taskQue_.size() < taskQueMaxThreshHold_;}))
    {
        std::cout << taskQue_.size() << " " << taskQueMaxThreshHold_ << std::endl;
        // 表示notFull_通知等待1s，没有被唤醒，说明任务队列满了
        std::cerr << "task queue is full, submit task fail." << std::endl;
        return Result(sp, false);  
    }

    // 如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    // 因为新放了任务，任务队列肯定不空了，在notEmpty_上通知,分配线程消费任务
    notEmpty_.notify_all();

    // cached模式，任务处理比较紧急 场景：小而快的任务 根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
    if(poolMode_ == PoolMode::MODE_CACHED 
        && taskSize_ > idleThreadSize_
        && curThreadsSize_ < threadSizeThreshHOld_)
    {
        std::cout << ">>> create new thread..." << std::endl;
        // 创建新的线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        // 启动线程
        threads_[threadId]->start(); 
        // 修改线程个数相关变量
        curThreadsSize_++;
        idleThreadSize_++; // 新创建的线程是空闲的
    }
    return Result(sp, true);
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    // 设置线程池的启动状态
    isPoolRunning_ = true;

    // 记录初始线程个数
    initThreadsSize_ = initThreadSize;
    curThreadsSize_ = initThreadSize;

    // 创建线程对象
    for(int i = 0;i < initThreadsSize_; i++)
    {
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        // threads_.emplace_back(std::move(ptr));
    }

    // 启动所有线程     std::vector<Thread*> threads_; 
    for(auto& pair : threads_)
    {
        pair.second->start();
        idleThreadSize_++;
    }

}

// 定义线程函数  线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadid)
{
    // std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
    // std::cout << "end threadFunc tid:"  << std::this_thread::get_id() <<std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();
    for(;;)
    {
        std::shared_ptr<Task> task;
        {
            // 获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);
            std::cout << "tid:" << std::this_thread::get_id() << 
                " 尝试获取任务..." << std::endl;

            // cache模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程结束回收（超过initThreadSize_数量的线程要进行回收）
            // 当前时间 - 上一次线程执行的时间 > 60s
            // 每一秒钟返回一次  怎么区分 超时返回 还是有任务待执行返回
            // 锁 + 双重判断
            while(taskQue_.size() == 0)
            {
                // 线程池要结束回收线程资源
                if(!isPoolRunning_)
                {
                    threads_.erase(threadid);
                    std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
                    exitCond_.notify_all(); // 通知线程池析构函数可以结束了
                    return ;  // 线程函数结束，线程结束
                }
                if(poolMode_ == PoolMode::MODE_CACHED)
                {
                    if(std::cv_status::timeout == 
                        notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if(dur.count() >= THREAD_MAX_IDLE_TIME
                            && curThreadsSize_ > initThreadsSize_)
                        {
                            // 开始回收当前线程
                            // 记录线程相关变量的值修改
                            // 把线程数量从线程列表容器中删除  
                            threads_.erase(threadid);
                            curThreadsSize_--;
                            idleThreadSize_--;

                            std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
                            return ;
                        }
                    }
                        
                }
                else
                {
                    // 等待notEmpty条件
                    notEmpty_.wait(lock);
                }
                
            }
            
            idleThreadSize_--; // 线程开始工作，空闲线程数-1

            std::cout << "tid:" << std::this_thread::get_id() << 
                " 获取任务成功..." << std::endl;
            // 从任务队列中取一个任务
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;
            
            if(taskQue_.size() > 0)
            {
                // 如果依然有剩余任务，继续通知其他的线程执行任务
                notEmpty_.notify_all();
            }
            // 任务队列不满了，通知notFull_条件变量
            notFull_.notify_all();
        } // 释放锁
        
        // 当前线程负责执行这个任务
        if(task != nullptr)
        {    
            // task->run();  // 执行任务，把任务的返回值存储到Result对象中
            task->exec();
        }
        idleThreadSize_++; // 线程执行完任务，空闲线程数+1
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间

    }

}

bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

//////////////////// Task方法实现//////////////////
Task::Task()
    : result_(nullptr)
{}

void Task::exec()
{
    if(result_ != nullptr)
    {
        result_->setVal(run()); // 发生多态调用，执行用户自定义的任务逻辑
    }
}

void Task::setResult(Result* res)
{
    result_ = res;
}


/////////////////////线程方法实现////////////////
int Thread::generateId_ = 0;

// 线程构造
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{

}

// 线程析构
Thread::~Thread()
{

}

// 启动线程
void Thread::start()
{
    // 创建一个线程来执行线程函数  fcun_绑定器 threadId作为参数传入
    std::thread t(func_, threadId_);
    t.detach(); // 分离线程，允许线程在后台运行  pthread_detach() pthread_t设置成分离线程
}

int Thread::getId() const
{
    return threadId_;
}

//////////// Result方法实现////////////
Result::Result(std::shared_ptr<Task> task, bool isValid)
    : isValid_(isValid)
    , task_(task)
{
    task_->setResult(this);
}

Any Result::get()
{
    if(!isValid_)
    {
        return "";
    }
    sem_.wait();  // task任务如果没有执行完成，当前线程会阻塞在这里
    return std::move(any_);
}

void Result::setVal(Any any)
{
    // 存储task的返回值
    this->any_ = std::move(any);
    sem_.post();  // 已经获取了任务的返回值，增加信号量资源
}
