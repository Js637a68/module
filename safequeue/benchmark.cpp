#include <iostream>
#include <thread>
#include <vector>

#include "atomic_queue.h"
#include "lock_queue.h"

constexpr size_t QueueSize = 1024;
constexpr int NumThreads = 4;   // 生产者数量 == 消费者数量
constexpr int DurationSec = 5;  // 测试时长（秒）

std::atomic<size_t> total_pushes{0};
std::atomic<size_t> total_pops{0};

template <typename Queue>
void producer(Queue& q, std::atomic<bool>& running) {
  while (running.load(std::memory_order_relaxed)) {
    if (q.push(42)) {
      total_pushes.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

template <typename Queue>
void consumer(Queue& q, std::atomic<bool>& running) {
  int value;
  while (running.load(std::memory_order_relaxed)) {
    if (q.pop(value)) {
      total_pops.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

template <typename Queue>
void run_benchmark(const std::string& name, Queue& q) {
  total_pushes = 0;
  total_pops = 0;

  std::atomic<bool> running{true};
  std::vector<std::thread> threads;

  for (int i = 0; i < NumThreads; ++i)
    threads.emplace_back(producer<Queue>, std::ref(q), std::ref(running));
  for (int i = 0; i < NumThreads; ++i)
    threads.emplace_back(consumer<Queue>, std::ref(q), std::ref(running));

  auto start = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::seconds(DurationSec));
  running = false;
  for (auto& t : threads) t.join();
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> elapsed = end - start;

  std::cout << "\n=== Benchmark: " << name << " ===\n";
  std::cout << "Time elapsed:  " << elapsed.count() << " sec\n";
  std::cout << "Total pushes:  " << total_pushes.load() << "\n";
  std::cout << "Total pops:    " << total_pops.load() << "\n";
}

int main() {
  // LockFreeQueue<int, QueueSize> lockfree_q;
  // run_benchmark("LockFreeQueue (MPMC)", lockfree_q);

  ThreadSafeQueue<int> locked_q;
  run_benchmark("LockedQueue (std::mutex)", locked_q);
  return 0;

  /* 2cpu 2core
=== Benchmark: LockFreeQueue (MPMC) ===
Time elapsed:  5.00146 sec
Total pushes:  37143230
Total pops:    37143226
=== Benchmark: LockedQueue (std::mutex) ===
Time elapsed:  5.00039 sec
Total pushes:  34355550
Total pops:    34114948
  */
}