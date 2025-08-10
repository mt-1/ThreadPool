#include <iostream>
#include <functional>
#include <thread>
#include <future>
using namespace std;

#include "threadpool.h"

/*
submitTask：可变参模板

C++11 线程库 thread  package_task(function函数对象)  async
    std::thread t1(func, args...); // 启动线程
    t1.join(); // 等待线程结束
    t1.detach(); // 分离线程，允许线程在后台运行
*/


int sum1(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(2));
    return a + b;
}
int sum2(int a, int b, int c)
{
    this_thread::sleep_for(chrono::seconds(2));
    return a + b + c;
}

int main()
{

    ThreadPool pool;
    // pool.setPoolMode(PoolMode::MODE_CACHED);
    pool.start(2);

    future<int> r1 = pool.submitTask(sum1, 1, 2);
    future<int> r2 = pool.submitTask(sum2, 1, 2, 3);
    future<int> r3 = pool.submitTask([](int a, int b)->int{
        int res = 0;
        for(int i = a;i <= b;i++)
            res += i;
        return res;
    
    }, 1, 100);
    future<int> r4 = pool.submitTask(sum2, 1, 2, 3);

    future<int> r5 = pool.submitTask([](int a, int b)->int{
        int res = 0;
        for(int i = a;i <= b;i++)
            res += i;
        return res;
    }, 1, 100);

    cout << r1.get() << endl;
    cout << r2.get() << endl;
    cout << r3.get() << endl;
    cout << r4.get() << endl;
    cout << r5.get() << endl;

    // packaged_task<int(int, int, int)> task(sum2);
    // future<int> res = task.get_future();

    // thread t(std::move(task), 10, 20, 30);
    // t.join(); // 等待线程结束
    // // task(10, 20, 30); // 执行任务

    // cout << res.get() << endl;

    return 0;
}