#include <unistd.h>

#include <iostream>

#include "threadpool.h"

using namespace std;

int add() {
  static atomic_int32_t x = 0;
  return ++x;
}

class MyTask : public Task {
 public:
  Any run() override { return Any(add()); }
};

int main() {
  ThreadPool pool;
  pool.setMode(PoolMode::MODE_CACHED);
  pool.start(4);

  for (int i = 0; i < 100; ++i) {
    thread([&]() {
      Result res = pool.submitTask(std::make_shared<MyTask>());
      int val = res.get().cast_<int>();
      cout << val << endl;
    }).detach();
  }
  cout << add() << endl;
}