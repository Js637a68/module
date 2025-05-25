#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace messaging {
struct message_base {
  virtual ~message_base() {}
};

template <typename Msg>
struct wrapped_message : message_base {
  Msg contents;
  explicit wrapped_message(Msg const& contents_) : contents(contents_) {}
};

class queue {
  std::mutex m;
  std::condition_variable c;
  std::queue<std::shared_ptr<message_base>> q;

 public:
  template <class T>
  void push(T const& msg) {
    std::lock_guard<std::mutex> lock(m);
    q.push(std::make_shared<wrapped_message<T>>(msg));
    c.notify_all();
  }

  std::shared_ptr<message_base> wait_and_pop() {
    std::unique_lock<std::mutex> lock(m);
    while (q.empty()) c.wait(lock, [&]() { return !q.empty(); });
    auto res = q.front();
    q.pop();
    return res;
  }
};

}  // namespace messaging