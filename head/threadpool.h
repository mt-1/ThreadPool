#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>

/*
example:
ThreadPool pool;
pool.start(4); // 启动线程池，初始线程数量为4

class MyTask : public Task
{
public:
    void run() { // 线程代码... }
};

pool.submitTask(std::make_shared<MyTask>()); // 提交任务

*/


// Any类型：可以接收任意数据的类型
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    // 这个构造函数可以让Any类型接收任意其它的数据
    template<typename T>  // T:int  Derive<int>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {}

    // 这个方法能吧Any对象里面存储的数据转换成指定类型
    template<typename T>
    T cast()
    {
        // 怎么从base_找到它所指向的派生类对象，并且从它里面取出data成员变量
        // 基类指针 =》 派生类指针  RTTI
        Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr)
        {
            throw std::bad_cast();
        }
        return pd->data_;
    }
private:
    // 基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    // 派生类类型
    template<typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data) {}
        T data_;
    };

private:
    // 定义一个基类的指针
    std::unique_ptr<Base> base_;
};


// 实现一个信号量类
class Semaphore
{
public:
    Semaphore(int limit = 0) 
        : resLimit_(limit)
        , isExit_(false)
    {}
    ~Semaphore()
    {
        // 线程池析构时，销毁信号量
        // 避免析构时死锁
        isExit_ = true;
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    // 获取一个信号量资源
    void wait()
    {
        if(isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有资源的话，会阻塞当前线程
        cond_.wait(lock, [&]()->bool { return resLimit_ > 0; });
        resLimit_--;
    }

    // 增加一个信号量资源
    void post()
    {
        if(isExit_)
            return;
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        // linux下condition_variable的析构函数什么也没做
        // 导致这里状态已经失效，无故阻塞
        cond_.notify_all();  // 等待状态，释放mutex锁，通知条件变量起来
    }
private:
    std::atomic_bool isExit_;
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

// Task类型的前置声明
class Task;

// 异步任务返回值类型 类似future
// 实现接收提交到线程池的task任务执行完成后的返回值类型Result
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    // Result(const Result&) = delete;
    // Result& operator=(const Result&) = delete;

    // setVal方法，获取任务执行完成的返回值
    void setVal(Any any);

    // get方法，用户调用这个方法获取task的返回值 阻塞直到得到结果
    Any get();
private:
    Any any_;  // 存储任务的返回值
    Semaphore sem_;  // 线程通信信号量
    std::shared_ptr<Task> task_; // 指向对应获取返回值的任务对象
    std::atomic_bool isValid_;  // 返回值是否有效
};

// 任务抽象基类
class Task
{
public:
    Task();
    ~Task() = default;
    void exec();
    void setResult(Result* res);
    // 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务逻辑
    virtual Any run() = 0;
private:
    Result* result_;  // Result生命周期 > Task生命周期
};

// 线程支持的模式
enum PoolMode
{
    MODE_FIXED, // 固定数量的线程
    MODE_CACHED, // 线程数量可动态增长
};

// 线程类型
class Thread
{
public:
    // 线程函数对象类型
    using ThreadFunc = std::function<void(int)>;

    // 线程构造
    Thread(ThreadFunc func);

    // 线程析构
    ~Thread();

    // 启动线程
    void start();

    // 获取线程id
    int getId() const;
private:
    ThreadFunc func_;
    static int generateId_;
    int threadId_;  // 保存线程id
};

// 线程池类型
class ThreadPool
{
public:
    // 线程池构造
    ThreadPool();

    // 线程池析构
    ~ThreadPool();

    // 设置线程池模式
    void setPoolMode(PoolMode mode);

    // 设置task任务队列上限阈值
    void setTaskQueMaxThreshHold(int threshHold);

    // 设置线程池cached模式下线程阈值
    void setThreadSizeThreshHold(int threshHold);

    // 给线程池添加任务
    Result submitTask(std::shared_ptr<Task> sp);

    // 开启线程池  返回系统核心数量
    void start(int initThreadSize = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void threadFunc(int threadid);

    // 检查pool的运行状态
    bool checkRunningState() const;

private:
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // 线程列表
    // std::vector<std::unique_ptr<Thread>> threads_; // 线程列表

    // 线程管理
    size_t initThreadsSize_; // 初始线程数量
    std::atomic_int curThreadsSize_; // 记录当前线程池里面的线程数量
    std::atomic_int idleThreadSize_; // 记录空闲线程数量
    int threadSizeThreshHOld_;  // 线程数量上限阈值

    // 任务管理
    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskSize_; // 任务数量
    int taskQueMaxThreshHold_; // 任务队列数量上限阈值

    // 同步机制
    std::mutex taskQueMtx_; // 保证任务队列的线程安全
    std::condition_variable notFull_; // 任务列队不满
    std::condition_variable notEmpty_; // 任务列队不空
    std::condition_variable exitCond_; // 等待线程资源全部回收 

    PoolMode poolMode_; // 当前线程池工作模式
    std::atomic_bool isPoolRunning_;  // 表示当前线程池的启动状态

};



#endif