#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include "any_class.h"
#include "semaphore.h"

enum class PoolMode {
  MODE_FIXED,
  MODE_CACHED,
};

class Task;
class Result {
 public:
  Result(std::shared_ptr<Task> task, bool isValid = true);
  ~Result() = default;

  // 设置任务执行完的结果
  void setVal(Any any) {
    result_ = std::move(any);
    sem_.post();
  }
  // 获取任务的返回值
  Any get() {
    if (!isValid_) return "";
    sem_.wait();
    return std::move(result_);
  }

 private:
  Any result_;                  // 存储任务的返回值
  Semaphore sem_;               // 线程通信信号量
  std::shared_ptr<Task> task_;  // 指向对应获取返回值的任务对象
  std::atomic_bool isValid_;    // 返回值是否有效
};

class Task {
 public:
  Task() : result_(nullptr) {};
  ~Task() = default;
  void exec() {
    if (result_ != nullptr) result_->setVal(run());
  };
  void setResult(Result* res) { result_ = res; };

  virtual Any run() = 0;

 private:
  Result* result_;
};

class Thread {
 public:
  using ThreadFunc = std::function<void(int)>;
  Thread(ThreadFunc func) : func_(func), threadId_(generateId_++) {}
  ~Thread() = default;
  void start() { std::thread(func_, threadId_).detach(); }
  int getId() const { return threadId_; };

 private:
  int threadId_;           // 保存线程id
  ThreadFunc func_;        // 保存线程入口函数
  static int generateId_;  // 静态成员变量，线程id生成器
};

class ThreadPool {
 public:
  ThreadPool();
  ~ThreadPool();
  // 设置线程池的工作模式
  void setMode(PoolMode mode);
  // 设置task队列上限阈值
  void setTaskQueMaxThreshHold(int threshhold);
  // 设置线程池cached模式下线程阈值
  void setThreadSizeThreshHold(int threshhold);
  // 给线程池提交任务
  Result submitTask(std::shared_ptr<Task> sp);
  // 启动线程池
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
  int initThreadSize_;                                        // 初始的线程数量
  int threadSizeThreshHold_;        // 线程数量上限阈值
  std::atomic_int curThreadSize_;   // 当前线程池里面线程的个数
  std::atomic_int idleThreadSize_;  // 当前线程池里面空闲线程的个数

  std::queue<std::shared_ptr<Task>> taskQue_;  // 任务队列
  std::atomic_int taskSize_;                   // 任务的个数
  int taskQueMaxThreshHold_;                   // 任务队列数量上限阈值
  std::mutex taskQueMtx_;                      // 任务队列的锁
  std::condition_variable notFull_;            // 任务队列不满
  std::condition_variable notEmpty_;           // 任务队列不空
  std::condition_variable exitCond_;           // 退出条件

  PoolMode poolMode_;               // 线程池的工作模式
  std::atomic_bool isPoolRunning_;  // 线程池的启动状态
};