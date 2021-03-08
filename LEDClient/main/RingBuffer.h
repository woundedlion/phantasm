#pragma once

#include <mutex>
#include "Mutex.h"

template <typename T, int D>
class RingBuffer {
 public:
  RingBuffer() : r_(0), w_(0), level_(0) {}

  constexpr size_t datum_size() const { return sizeof(T); }
  constexpr size_t depth() { return D; }

  size_t level() const {
    std::lock_guard<Mutex> _(lock_);
    return level_;
  }

  const T& front() const {
    std::lock_guard<Mutex> _(lock_);
    return bufs_[r_];
  }

  T* next() {
    std::lock_guard<Mutex> _(lock_);
    return &bufs_[(w_ + 1) % D];
  }

  void push() {
    std::lock_guard<Mutex> _(lock_);
    if (level_ && w_ == r_) {
      assert(0);
    }
    level_++;
    w_ = (w_ + 1) % D;
  }

  int pop(uint32_t n) {
    std::lock_guard<Mutex> _(lock_);
    if (!level_) {
      assert(r_ == w_);
      throw std::runtime_error("buffer underrun!");
    }
    int popped = std::min(n, level_);
    r_ = (r_ + popped) % D;
    level_ -= popped;
    return popped;
  }

 private:
  T bufs_[D];
  int r_;
  int w_;
  size_t level_;
  mutable Mutex lock_;
};
