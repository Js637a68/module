#include <atomic>
#include <iostream>
#include <thread>

class spin_lock {
 public:
  void lock() {
    while (flag.test_and_set(std::memory_order_acquire)) {
      // spin
    }
  }

  void unlock() { flag.clear(std::memory_order_release); }

 private:
  std::atomic_flag flag = ATOMIC_FLAG_INIT;
};
