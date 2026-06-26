# ThreadPool 实现笔记

## 1. ThreadPool 属于哪个知识层次

```text
C++ 并发
任务队列
线程同步
服务器工程基础设施
```

## 2. 当前项目中的线程池成员

```cpp
std::vector<std::thread> workers_;
std::queue<std::function<void()>> tasks_;
std::mutex mutex_;
std::condition_variable condition_;
bool stop_;
```

含义：

| 成员 | 作用 |
|---|---|
| `workers_` | 保存工作线程对象 |
| `tasks_` | 保存待执行任务 |
| `mutex_` | 保护任务队列和 stop_ |
| `condition_` | 队列为空时让 worker 等待 |
| `stop_` | 通知 worker 退出 |

## 3. worker_loop 核心结构

```cpp
while (true) {
    std::function<void()> task;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return stop_ || !tasks_.empty();
        });

        if (stop_ && tasks_.empty()) {
            return;
        }

        task = std::move(tasks_.front());
        tasks_.pop();
    }

    task();
}
```

## 4. 为什么 task 必须在锁外执行

锁保护的是：

```text
任务队列结构
stop_ 状态
```

不是保护任务本身。

如果在锁内执行：

```cpp
std::lock_guard<std::mutex> lock(mutex_);
task();
```

问题：

```text
其他 worker 无法取任务。
submit 无法加入任务。
如果 task 内部阻塞，整个线程池调度都被卡住。
```

服务器中 task 很可能包含：

```text
read/write
业务计算
数据库访问
日志写入
```

这些都不应该持有任务队列锁。

## 5. condition_variable 为什么用 unique_lock

`condition_variable::wait` 需要：

```text
等待时释放锁
被唤醒后重新加锁
```

所以必须使用：

```cpp
std::unique_lock<std::mutex>
```

而不是：

```cpp
std::lock_guard<std::mutex>
```

## 6. 谓词防止虚假唤醒

```cpp
condition_.wait(lock, [this] {
    return stop_ || !tasks_.empty();
});
```

等价于：

```cpp
while (!stop_ && tasks_.empty()) {
    condition_.wait(lock);
}
```

意义：

```text
即使线程被虚假唤醒，也会重新检查条件。
```

## 7. 当前线程池的工程改进点

### 7.1 submit 返回 bool

当前：

```cpp
void submit(const std::function<void()>& task);
```

可改为：

```cpp
bool submit(std::function<void()> task);
```

好处：

```text
线程池停止后，调用方知道任务没有提交成功。
```

### 7.2 支持移动任务

当前按 const 引用接收，push 时拷贝。

可改为：

```cpp
void submit(std::function<void()> task) {
    tasks_.push(std::move(task));
}
```

### 7.3 限制任务队列长度

真实服务器中不能无限接受任务：

```text
任务队列无限增长会导致内存压力。
```

后续可以加入：

```text
最大队列长度
提交失败返回 false
过载保护
```
