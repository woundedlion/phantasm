#pragma once

#include <mutex>
#include <condition_variable>

const int W = 288;
const int H = 144;
const int STRIP_H = 48;

template <typename T, int W, int H>
class DoubleBuffer {
public:
  
    DoubleBuffer() :
    front_(0),
    used_(0),
    canceled_(false)
  {}
  
  ~DoubleBuffer() {}
  
  constexpr size_t size() { return W * H * sizeof(T); }
  void swap() {
    front_ = !front_;
    dec_used();
  }
  
  void stamp() {
    std::copy(bufs_[front_], bufs_[front_] + (W * H), bufs_[!front_]);
  }
  
  RGB* front() {
    return bufs_[front_];
  }
  
  RGB* back() {
    return bufs_[!front_];
  }
  
  void inc_used() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_++;
    used_cv_.notify_all();
  }
  
  void dec_used() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_--;
    used_cv_.notify_all();
  }
  
  bool wait_not_empty() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_cv_.wait(_, [this]() { return canceled_ || used_ > 0; });
    return !canceled_;
  }
  
  bool wait_not_full() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_cv_.wait(_, [this]() { return canceled_ || used_ < 2; });
    return !canceled_;
  }
  
  void cancel_waits() {
    std::unique_lock<std::mutex> _(used_lock_);
    canceled_ = true;
    used_cv_.notify_all();
  }
  
private:
  
  int front_;
  T bufs_[2][W * H];
  std::mutex used_lock_;
  size_t used_;
  std::condition_variable used_cv_;
  bool canceled_;
};

