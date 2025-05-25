#include "threadpool.h"

#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;  // 单位：秒
int Thread::generateId_ = 0;

ThreadPool::ThreadPool()
    : initThreadSize_(0),
      taskSize_(0),
      idleThreadSize_(0),
      curThreadSize_(0),
      taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
      threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
      poolMode_(PoolMode::MODE_FIXED),
      isPoolRunning_(false) {}
ThreadPool::~ThreadPool() {
  isPoolRunning_ = false;

  // 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
  std::unique_lock<std::mutex> lock(taskQueMtx_);
  notEmpty_.notify_all();
  exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp) {
  // 获取锁
  std::unique_lock<std::mutex> lock(taskQueMtx_);

  if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool {
        return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
      })) {
    std::cerr << "task queue is full, submit task fail." << std::endl;

    return Result(sp, false);
    // return task->getResult();  // Task  Result
    // 线程执行完task，task对象就被析构掉了
  }

  taskQue_.emplace(sp);
  taskSize_++;
  notEmpty_.notify_one();

  // cached模式 任务处理比较紧急 场景：小而快的任务
  // 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
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
  return Result(sp);
}

// 开启线程池
void ThreadPool::start(int initThreadSize) {
  isPoolRunning_ = true;
  initThreadSize_ = initThreadSize;
  curThreadSize_ = initThreadSize;

  for (int i = 0; i < initThreadSize_; i++) {
    auto ptr = std::make_unique<Thread>(
        std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
    int threadId = ptr->getId();
    threads_.emplace(threadId, std::move(ptr));
    // threads_.emplace_back(std::move(ptr));
  }

  for (int i = 0; i < initThreadSize_; i++) {
    threads_[i]->start();
    idleThreadSize_++;
  }
}

bool ThreadPool::checkRunningState() const { return isPoolRunning_; }
void ThreadPool::setMode(PoolMode mode) {
  if (checkRunningState()) return;
  poolMode_ = mode;
}
void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
  if (checkRunningState()) return;
  taskQueMaxThreshHold_ = threshhold;
}
void ThreadPool::setThreadSizeThreshHold(int threshhold) {
  if (checkRunningState()) return;
  if (poolMode_ == PoolMode::MODE_CACHED) {
    threadSizeThreshHold_ = threshhold;
  }
}

void ThreadPool::threadFunc(int threadid)  // 线程函数返回，相应的线程也就结束了
{
  auto lastTime = std::chrono::high_resolution_clock().now();

  // 所有任务必须执行完成，线程池才可以回收所有线程资源
  for (;;) {
    std::shared_ptr<Task> task;
    {
      // 先获取锁
      std::unique_lock<std::mutex> lock(taskQueMtx_);

      // std::cout << "tid:" << std::this_thread::get_id() <<
      // "尝试获取任务..." << std::endl;

      // cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程
      // 结束回收掉（超过initThreadSize_数量的线程要进行回收）
      // 当前时间 - 上一次线程执行的时间 > 60s

      // 每一秒中返回一次   怎么区分：超时返回？还是有任务待执行返回
      // 锁 + 双重判断
      while (taskQue_.size() == 0) {
        // 线程池要结束，回收线程资源
        if (!isPoolRunning_) {
          threads_.erase(threadid);  // std::this_thread::getid()
          std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                    << std::endl;
          exitCond_.notify_all();
          return;  // 线程函数结束，线程结束
        }

        if (poolMode_ == PoolMode::MODE_CACHED) {
          // 条件变量，超时返回了
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

              std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
                        << std::endl;
              return;
            }
          }
        } else {
          // 等待notEmpty条件
          notEmpty_.wait(lock);
        }
      }

      idleThreadSize_--;

      // std::cout << "tid:" << std::this_thread::get_id() <<
      // "获取任务成功..." << std::endl;

      task = taskQue_.front();
      taskQue_.pop();
      taskSize_--;

      // 如果依然有剩余任务，继续通知其它得线程执行任务
      if (taskQue_.size() > 0) {
        notEmpty_.notify_all();
      }

      notFull_.notify_all();
    }

    if (task != nullptr) {
      task->exec();
    }

    idleThreadSize_++;
    lastTime =
        std::chrono::high_resolution_clock().now();  // 更新线程执行完任务的时间
  }
}

Result::Result(std::shared_ptr<Task> task, bool isValid)
    : task_(task), isValid_(isValid) {
  task_->setResult(this);
}