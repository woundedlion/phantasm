#pragma once

#include <mutex>
#include <cassert>
#include "Mutex.h"

template <typename T, int D>
class RingBuffer {
 public:
  RingBuffer() : r_(0), w_(0), level_(0) { 
    memset(bufs_, 0, sizeof(bufs_));
  }

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

  void pop() {
    std::lock_guard<Mutex> _(lock_);
    if (!level_) {
      assert(r_ == w_);
      throw std::runtime_error("buffer underrun!");
    }
    r_ = (r_ + 1) % D;
    level_--;
  }

  uint32_t ffwd(uint32_t n) {
    std::lock_guard<Mutex> _n(lock_);
    if (!level_) {
      return 0;
    }
    int popped = std::min(n, level_ - 1);
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
