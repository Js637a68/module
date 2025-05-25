#pragma once
#include "message.h"

namespace messaging {
class close_queue {};

template <typename PreviousDispatcher, typename Msg, typename Func>
class TemplateDispatcher;

class dispatcher {
  queue* q;
  bool chained;

  dispatcher(dispatcher const&) = delete;  // dispatcher实例不能被拷贝
  dispatcher& operator=(dispatcher const&) = delete;

  template <typename Dispatcher, typename Msg, typename Func>
  friend class TemplateDispatcher;

  void wait_and_dispatch() {
    for (;;) {
      auto msg = q->wait_and_pop();
      dispatch(msg);
    }
  }

  bool dispatch(std::shared_ptr<message_base> const& msg) {
    if (dynamic_cast<wrapped_message<close_queue>*>(msg.get())) {
      throw close_queue();
    }
    return false;
  }

 public:
  dispatcher(dispatcher&& other) : q(other.q), chained(other.chained) {
    other.chained = true;
  }

  explicit dispatcher(queue* q_) : q(q_), chained(false) {}
  template <typename Message, typename Func>
  TemplateDispatcher<dispatcher, Message, Func> handle(Func&& f) {
    return TemplateDispatcher<dispatcher, Message, Func>(q, this,
                                                         std::forward<Func>(f));
  }

  ~dispatcher() noexcept(false)  // 4 析构函数可能会抛出异常
  {
    if (!chained) {
      wait_and_dispatch();
    }
  }
};

class sender {
  queue* q;  // sender是一个队列指针的包装类
 public:
  sender() : q(nullptr) {}
  explicit sender(queue* q_) : q(q_) {}
  template <typename Message>
  void send(Message const& msg) {
    if (q) {
      q->push(msg);  // 将发送信息推送给队列
    }
  }
};

class receiver {
  queue q;  // 接受者拥有对应队列
 public:
  operator sender()  // 允许将类中队列隐式转化为一个sender队列
  {
    return sender(&q);
  }
  dispatcher wait()  // 等待对队列进行调度
  {
    return dispatcher(&q);
  }
};

template <typename PreviousDispatcher, typename Msg, typename Func>
class TemplateDispatcher {
  std::string msg;
  queue* q;
  PreviousDispatcher* prev;
  Func f;
  bool chained;

  TemplateDispatcher(TemplateDispatcher const&) = delete;
  TemplateDispatcher& operator=(TemplateDispatcher const&) = delete;
  template <typename Dispatcher, typename OtherMsg, typename OtherFunc>
  friend class TemplateDispatcher;

  void wait_and_dispatch() {
    for (;;) {
      auto msg = q->wait_and_pop();
      if (dispatch(msg))  // 1 如果消息处理过后，会跳出循环
        break;
    }
  }
  bool dispatch(std::shared_ptr<message_base> const& msg) {
    if (wrapped_message<Msg>* wrapper = dynamic_cast<wrapped_message<Msg>*>(
            msg.get()))  // 2 检查消息类型，并且调用函数
    {
      f(wrapper->contents);
      return true;
    } else {
      return prev->dispatch(msg);  // 3 链接到之前的调度器上
    }
  }

 public:
  TemplateDispatcher(TemplateDispatcher&& other)
      : q(other.q),
        prev(other.prev),
        f(std::move(other.f)),
        chained(other.chained) {
    other.chained = true;
  }
  TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_)
      : q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false) {
    prev_->chained = true;
  }

  template <typename OtherMsg, typename OtherFunc>
  TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc> handle(
      OtherFunc&& of)  // 4 可以链接其他处理器
  {
    return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>(
        q, this, std::forward<OtherFunc>(of));
  }

  ~TemplateDispatcher() noexcept(false) {
    if (!chained) {
      wait_and_dispatch();
    }
  }
};

}  // namespace messaging