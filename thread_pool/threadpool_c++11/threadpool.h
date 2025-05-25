#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;  // 单位：秒

enum class PoolMode {
  MODE_FIXED,   // 固定数量的线程
  MODE_CACHED,  // 线程数量可动态增长
};

class Thread {
 public:
  using ThreadFunc = std::function<void(int)>;
  Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}
  ~Thread() = default;
  void start() {
    std::thread t(func_, threadId_);
    t.detach();
  }
  int getId() const { return threadId_; }

 private:
  static int generateId_;
  ThreadFunc func_;
  int threadId_;
};
int Thread::generateId_ = 0;

class ThreadPool {
 public:
  ThreadPool()
      : initThreadSize_(0),
        curThreadSize_(0),
        idleThreadSize_(0),
        taskSize_(0),
        threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
        taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
        poolMode_(PoolMode::MODE_FIXED),
        isPoolRunning_(false) {}

  ~ThreadPool() {
    isPoolRunning_ = false;
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
  }

  void setMode(PoolMode mode) {
    if (checkRunningState()) return;
    poolMode_ = mode;
  }

  void setTaskQueMaxThreshHold(int threshhold) {
    if (checkRunningState()) return;
    taskQueMaxThreshHold_ = threshhold;
  }

  void setThreadSizeThreshHold(int threshhold) {
    if (checkRunningState()) return;
    if (poolMode_ == PoolMode::MODE_FIXED) return;
    threadSizeThreshHold_ = threshhold;
  }

  template <typename Func, typename... Args>
  auto submitTask(Func&& func, Args&&... args)
      -> std::future<decltype(func(args...))> {
    // 返回函数执行结果的类型
    using RType = decltype(func(args...));
    auto task = std::make_shared<std::packaged_task<RType()>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    std::future<RType> result = task->get_future();

    std::unique_lock<std::mutex> lock(taskQueMtx_);

    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
          return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
        })) {
      std::cerr << "task queue is full, submit task fail." << std::endl;
      auto task = std::make_shared<std::packaged_task<RType()>>(
          []() -> RType { return RType(); });
      (*task)();
      return task->get_future();
    }

    //  值捕获，注意task生命周期
    taskQue_.emplace([task]() { (*task)(); });
    taskSize_++;

    notEmpty_.notify_all();

    // Cache Mod
    if (poolMode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ &&
        curThreadSize_ < threadSizeThreshHold_) {
      std::cout << ">>> create new thread..." << std::endl;

      auto ptr = std::make_unique<Thread>(
          std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
      int threadId = ptr->getId();
      threads_.emplace(threadId, std::move(ptr));
      threads_[threadId]->start();
      curThreadSize_++;
      idleThreadSize_++;
    }

    return result;
  }

  void start(int initThreadSize = std::thread::hardware_concurrency()) {
    isPoolRunning_ = true;
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    for (int i = 0; i < initThreadSize_; i++) {
      auto ptr = std::make_unique<Thread>(
          std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
      int threadId = ptr->getId();
      threads_.emplace(threadId, std::move(ptr));
    }

    for (int i = 0; i < initThreadSize_; i++) {
      threads_[i]->start();
      idleThreadSize_++;
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

 private:
  void threadFunc(int threadid) {
    auto lastTime = std::chrono::high_resolution_clock().now();

    for (;;) {
      Task task;
      {
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..."
                  << std::endl;
        while (taskQue_.size() == 0) {
          if (!isPoolRunning_) {
            threads_.erase(threadid);
            std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                      << std::endl;
            exitCond_.notify_all();
            return;
          }

          if (poolMode_ == PoolMode::MODE_CACHED) {
            if (std::cv_status::timeout ==
                notEmpty_.wait_for(lock, std::chrono::seconds(1))) {
              auto now = std::chrono::high_resolution_clock().now();
              auto dur = std::chrono::duration_cast<std::chrono::seconds>(
                  now - lastTime);
              if (dur.count() >= THREAD_MAX_IDLE_TIME &&
                  curThreadSize_ > initThreadSize_) {
                threads_.erase(threadid);
                curThreadSize_--;
                idleThreadSize_--;
                std::cout << "threadid:" << std::this_thread::get_id()
                          << " exit!" << std::endl;
                return;
              }
            }
          } else
            notEmpty_.wait(lock);
        }

        idleThreadSize_--;

        std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功..."
                  << std::endl;

        // 从任务队列种取一个任务出来
        task = taskQue_.front();
        taskQue_.pop();
        taskSize_--;

        if (taskQue_.size() > 0) notEmpty_.notify_all();
        notFull_.notify_all();
      }

      if (task != nullptr) task();
      idleThreadSize_++;
      lastTime = std::chrono::high_resolution_clock().now();
    }
  }

  bool checkRunningState() const { return isPoolRunning_; }

 private:
  std::unordered_map<int, std::unique_ptr<Thread>> threads_;  // 线程列表

  int initThreadSize_;              // 初始的线程数量
  int threadSizeThreshHold_;        // 线程数量上限阈值
  std::atomic_int curThreadSize_;   // 记录当前线程池里面线程的总数量
  std::atomic_int idleThreadSize_;  // 记录空闲线程的数量

  // Task任务 =》 函数对象
  using Task = std::function<void()>;
  std::queue<Task> taskQue_;  // 任务队列
  std::atomic_int taskSize_;  // 任务的数量
  int taskQueMaxThreshHold_;  // 任务队列数量上限阈值

  std::mutex taskQueMtx_;             // 保证任务队列的线程安全
  std::condition_variable notFull_;   // 表示任务队列不满
  std::condition_variable notEmpty_;  // 表示任务队列不空
  std::condition_variable exitCond_;  // 等到线程资源全部回收

  PoolMode poolMode_;               // 当前线程池的工作模式
  std::atomic_bool isPoolRunning_;  // 表示当前线程池的启动状态
};