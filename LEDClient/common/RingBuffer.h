#pragma once

template <typename T, int W, int H, int D>
class RingBuffer {
 public:
  RingBuffer() : 
  r_(0),
  w_(0),
  level_(0) 
  {}
  size_t level() const { return level_; }
  constexpr size_t depth() { return D; }
  constexpr size_t size() { return W * H * sizeof(T); }
  const T *front() const { return bufs_[r_]; }
  T *push_back() {
    if (level_ && w_ == r_) {
      assert(0);
    }
    int w = w_;
    level_++;
    w_ = (w + 1) % D;
    return bufs_[w];
  }
  void pop() { 
    if (!level_) {
      assert(r_ == w_);
      throw std::runtime_error("buffer underrun!");
    }
    r_ = (r_ + 1) % D; 
    level_--;
  }

 private:
  T bufs_[D][W * H];
  int r_;
  int w_;
  size_t level_;
};
