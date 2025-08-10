# ThreadPool

实现一个固定线程数量和可变线程数量的线程池

制作动态库：

g++ -fPIC -shared threadpool.cpp -o libtdpool.so -std=c++17

sudo mv libtdpool.so /usr/local/lib

g++ main.cpp -ltdpool -std=c++17 -lpthread
