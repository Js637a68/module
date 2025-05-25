#pragma once
#include <memory>
#include <mutex>
#include <condition_variable>

class Semaphore
{
public:
  Semaphore(int limit = 0) : resLimit_(limit) {}
  ~Semaphore() = default;
  // 获取一个信号量资源
  void wait()
  {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [&]() -> bool
             { return resLimit_ > 0; });
    resLimit_--;
  }

  // 增加一个信号量资源
  void post()
  {
    std::unique_lock<std::mutex> lock(mtx_);
    resLimit_++;
    // linux下condition_variable的析构函数什么也没做
    // 导致这里状态已经失效，无故阻塞
    cv_.notify_all(); // 通知并释放mutex锁
  }

private:
  int resLimit_;
  std::mutex mtx_;
  std::condition_variable cv_;
};