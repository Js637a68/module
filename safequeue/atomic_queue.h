#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>

// T 必须是 trivially copyable 且 lock-free 的
template <typename T, size_t Capacity>
class LockFreeQueue {
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of 2");
  static_assert(std::is_trivially_copyable<T>::value,
                "T must be trivially copyable");

 private:
  struct Slot {
    std::atomic<size_t> sequence;
    T data;
  };

  alignas(64) Slot buffer_[Capacity];
  alignas(64) std::atomic<size_t> head_{0};  // 消费者递增
  alignas(64) std::atomic<size_t> tail_{0};  // 生产者递增

 public:
  LockFreeQueue() {
    for (size_t i = 0; i < Capacity; ++i)
      buffer_[i].sequence.store(i, std::memory_order_relaxed);
  }

  bool push(const T& value) {
    size_t pos = tail_.load(std::memory_order_relaxed);

    while (true) {
      Slot& slot = buffer_[pos % Capacity];
      size_t seq = slot.sequence.load(std::memory_order_acquire);

      intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
      if (diff == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1,
                                        std::memory_order_relaxed)) {
          slot.data = value;
          slot.sequence.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;  // 队列已满
      } else {
        pos =
            tail_.load(std::memory_order_relaxed);  // 被其他线程占用，重新尝试
      }
    }
  }

  bool pop(T& result) {
    size_t pos = head_.load(std::memory_order_relaxed);

    while (true) {
      Slot& slot = buffer_[pos % Capacity];
      size_t seq = slot.sequence.load(std::memory_order_acquire);

      intptr_t diff =
          static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
      if (diff == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1,
                                        std::memory_order_relaxed)) {
          result = slot.data;
          slot.sequence.store(pos + Capacity, std::memory_order_release);
          return true;
        }
      } else if (diff < 0) {
        return false;  // 队列为空
      } else {
        pos = head_.load(std::memory_order_relaxed);  // 重新尝试
      }
    }
  }
};
