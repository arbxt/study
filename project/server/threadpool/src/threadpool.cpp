#include "threadpool.h"

#include <iostream>

/*
 * 构造线程池。
 *
 * 创建 thread_count 个工作线程。
 */
ThreadPool::ThreadPool(size_t thread_count)
    : stop_(false)
{
    
    // 如果用户传入 0，则至少创建 1 个线程。
    if (thread_count == 0) {
        thread_count = 1;
    }

    for (size_t i = 0; i < thread_count; ++i) {
        /*
         * 每个线程执行：
         *      this->worker_loop()
         *
         * [this] 是 lambda 捕获，
         * 表示 lambda 内部可以访问当前 ThreadPool 对象。
         */
        workers_.emplace_back([this]{
            this->worker_loop();
        });
    }
}

/*
 * 析构线程池。
 *
 * 这里做三件事：
 *
 * 1. 设置 stop_ = true
 * 2. 唤醒所有正在等待任务的工作线程
 * 3. join() 等待所有线程结束
 */
ThreadPool::~ThreadPool()
{
    {
        /*
         * 修改 stop_ 时需要加锁。
         *
         * 因为工作线程也会读取 stop_。
         */
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }

    /*
     * 唤醒所有工作线程。
     *
     * 如果某些线程正阻塞在 condition_.wait()，
     * notify_all() 会让它们醒来检查 stop_ 状态。
     */
    condition_.notify_all();

    /*
     * 等待所有工作线程退出。
     */
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

/*
 * 提交任务到线程池。
 */
void ThreadPool::submit(const std::function<void()>& task)
{
    {
        /*
         * 任务队列是共享资源，需要加锁保护。
         */
        std::lock_guard<std::mutex> lock(mutex_);

        /*
         * 如果线程池已经停止，则不再接受新任务。
         *
         * 当前示例中简单丢弃任务。
         * 也可以改成抛异常或返回 bool。
         */
        if (stop_) {
            return;
        }

        /*
         * 把任务加入队列。
         */
        tasks_.push(task);
    }

    /*
     * 唤醒一个等待中的工作线程。
     *
     * 注意：
     *      notify_one() 通常放在解锁之后调用。
     *      这样被唤醒的线程可以更快拿到锁。
     */
    condition_.notify_one();
}

/*
 * 工作线程主循环。
 */
void ThreadPool::worker_loop()
{
    while (true) {
        std::function<void()> task;

        {
            /*
             * condition_variable::wait 需要配合 unique_lock 使用。
             *
             * lock_guard 不行，因为 wait() 内部需要临时解锁和重新加锁。
             */
            std::unique_lock<std::mutex> lock(mutex_);

            /*
             * 当任务队列为空，并且线程池没有停止时，
             * 工作线程会阻塞在这里。
             *
             * wait 的第二个参数是谓词 predicate。
             *
             * 等价逻辑：
             *
             *      while (!stop_ && tasks_.empty()) {
             *          condition_.wait(lock);
             *      }
             *
             * 使用谓词可以防止虚假唤醒。
             */
            condition_.wait(lock, [this]() {
                return stop_ || !tasks_.empty();
            });

            /*
             * 如果线程池已经停止，并且任务队列为空，
             * 说明没有任务需要处理，当前工作线程可以退出。
             */
            if (stop_ && tasks_.empty()) {
                return;
            }

            /*
             * 取出队首任务。
             *
             * std::move 可以避免不必要的拷贝。
             */
            task = std::move(tasks_.front());
            tasks_.pop();
        }

        /*
         * 注意：
         *      任务执行一定要放在锁外。
         *
         * 如果在锁内执行 task()，
         * 那么其他工作线程就无法从队列取任务，
         * submit() 也无法加入新任务。
         *
         * 对于服务器来说，task() 里面会执行 read()，
         * 这是阻塞操作。
         *
         * 如果锁内执行 task()，整个线程池就会严重阻塞。
         */
        task();
    }
}