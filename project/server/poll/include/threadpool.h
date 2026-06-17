#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>              // std::vector
#include <queue>               // std::queue
#include <thread>              // std::thread
#include <mutex>               // std::mutex, std::lock_guard, std::unique_lock
#include <condition_variable>  // std::condition_variable
#include <functional>          // std::function

/*
 * ThreadPool 类：
 *
 * 作用：
 *      管理固定数量的工作线程。
 *      外部可以向线程池提交任务。
 *      工作线程从任务队列中取出任务并执行。
 *
 * 本线程池中的任务类型为：
 *
 *      std::function<void()>
 *
 * 也就是说，每个任务都是一个：
 *
 *      无参数、无返回值的可调用对象
 *
 */
class ThreadPool {
public:
    //禁止隐式构造
    explicit ThreadPool(size_t thread_count);

    ~ThreadPool();

    //任务提交函数
    void submit(const std::function<void()>& task);

    //禁止c++默认的拷贝构造
    ThreadPool(const ThreadPool&) = delete;

    //禁止拷贝赋值。
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    /*
     * 工作线程执行的函数。
     * 每个工作线程都会运行 worker_loop()。
     */
    void worker_loop();

private:
    /*
     * 保存所有工作线程对象。
     */
    std::vector<std::thread> workers_;

    /*
     * 任务队列。
     * 主线程 submit() 时向队列中添加任务。
     * 工作线程从队列中取任务执行。
     */
    std::queue<std::function<void()>> tasks_;

    /*
     * 互斥锁。
     * 用于保护任务队列 tasks_。
     */
    std::mutex mutex_;

    /*
     * 条件变量。
     * 当任务队列为空时，工作线程阻塞等待。
     * 当 submit() 添加新任务后，唤醒一个工作线程。
     */
    std::condition_variable condition_;

    /*
     * 停止标志。
     * stop_ == false：
     *      线程池正常运行。
     * stop_ == true：
     *      线程池准备停止。
     */
    bool stop_;
};

#endif