#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"

using ULong = unsigned long long;

class MyTask : public Task
{
public:
    MyTask() = default;
    MyTask(int begin, int end)
        : begin_(begin), end_(end)
    {
    }

    Any run()
    {
        std::cout << "tid:" << std::this_thread::get_id() << " begin" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ULong sum = 0;
        for (ULong i = begin_; i <= end_; i++)
        {
            sum += i;
        }
        std::cout << "tid:" << std::this_thread::get_id() << " end" << std::endl;
        return sum;
    }

private:
    int begin_;
    int end_;
};

int main()
{
    ThreadPool pool;
    pool.setPoolMode(PoolMode::MODE_CACHED);
    pool.start(4);
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 10));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(11, 20));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(21, 30));
    pool.submitTask(std::make_shared<MyTask>(21, 30));
    pool.submitTask(std::make_shared<MyTask>(21, 30));
    // ULong sum1 = res1.get().cast<ULong>();
    // std::cout << sum1 << std::endl;
    // std::cout << "main over " << std::endl;

#if 0
    {
    ThreadPool pool;
    pool.setPoolMode(PoolMode::MODE_CACHED);
    // 启动线程池
    pool.start(4);

    Result res1 = pool.submitTask(std::make_shared<MyTask>(1,10));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(11,20));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(21,30));
    pool.submitTask(std::make_shared<MyTask>(21,30));
    pool.submitTask(std::make_shared<MyTask>(21,30));
    
    ULong sum1 = res1.get().cast<ULong>();
    ULong sum2 = res2.get().cast<ULong>();
    ULong sum3 = res3.get().cast<ULong>();



    std::cout << "-----1-----: " << sum1 + sum2 + sum3 << std::endl;

    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());
    // pool.submitTask(std::make_shared<MyTask>());

    ULong sum = 0;
    for(ULong i  = 1;i <= 30; i++)
    {
        sum += i;
    }
    std::cout << "-----2-----: " << sum << std::endl;
}

    // std::this_thread::sleep_for(std::chrono::seconds(2));
    getchar();

    
    return 0;
#endif
}