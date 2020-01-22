#pragma once
#include "boost/asio.hpp"

struct RGB {
  RGB() {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r_(r), g_(g), b_(b){}
  uint8_t r_;
  uint8_t g_;
  uint8_t b_;
};

const int W = 288;
const int H = 144;

template <int W, int H>
class DoubleBuffer {
public:

  DoubleBuffer() :
    front_(0),
    used_(2),
    canceled_(false)
  {}

  ~DoubleBuffer() {}

  void swap() {
    front_ = !front_;
    dec_used();
  }

  void stamp() {
    std::copy(bufs_[front_], bufs_[front_] + sizeof(bufs_[front_]), bufs_[!front_]);
  }

  boost::asio::const_buffer front() {
    return boost::asio::buffer(bufs_[front_], sizeof(bufs_[front_]));
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

  void wait_not_empty() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_cv_.wait(_, [this]() { return canceled_ || used_ > 0; });
    if (canceled_) {
      throw std::runtime_error("wait_not_empty() canceled");
    }
  }

  void wait_not_full() {
    std::unique_lock<std::mutex> _(used_lock_);
    used_cv_.wait(_, [this]() { return canceled_ || used_ < 2; });
    if (canceled_) {
      throw std::runtime_error("wait_not_full() canceled");
    }
  }

  void cancel_waits() {
    std::unique_lock<std::mutex> _(used_lock_);
    canceled_ = true;
    used_cv_.notify_all();
  }

private:

  int front_;
  RGB bufs_[2][W * H];
  std::mutex used_lock_;
  size_t used_;
  std::condition_variable used_cv_;
  bool canceled_;
};

template<int W, int H>
class Canvas {
public:

  Canvas(DoubleBuffer<W, H> & buf) :
    buf_(buf)
  {
    buf_.wait_not_full();
    buf_.stamp();
  }

  ~Canvas() {
    buf_.inc_used();
  }

  RGB* operator[](int x) {
    return &buf_.back()[x * H];
  }

private:

  DoubleBuffer<W, H>& buf_;
};

class Effect {
public:

  Effect() : frame_count_(0){}
  virtual ~Effect() {};
  virtual void draw_frame() = 0;
  boost::asio::const_buffer buf() { return bufs_.front(); }
  void advance_frame() {
    frame_count_++;
    bufs_.swap();
  }
  void wait_frame_available() { bufs_.wait_not_empty(); }
  uint64_t frame_count() { return frame_count_; }
  void cancel() { bufs_.cancel_waits(); }
  
protected:

  DoubleBuffer<W, H> bufs_;
  uint64_t frame_count_;
};

class Test : public Effect {
public:
  Test() {}
  void draw_frame() {
    Canvas<W, H> c(this->bufs_);
    c[0][0] = RGB(255, 0, 0);
  }
};

