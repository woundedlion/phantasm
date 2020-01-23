#pragma once
#include "boost/asio.hpp"
#include "Types.h"
#include "DoubleBuffer.h"

const int W = 288;
const int H = 144;

template<int W, int H>
class Canvas {
public:

  Canvas(DoubleBuffer<RGB, W, H> & buf) :
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

  DoubleBuffer<RGB, W, H>& buf_;
};

class Effect {
public:

  Effect() : frame_count_(0){}
  virtual ~Effect() {};
  virtual void draw_frame() = 0;
  boost::asio::const_buffer buf() { return boost::asio::buffer(bufs_.front(),
							       bufs_.size()); }
  void advance_frame() {
    frame_count_++;
    bufs_.swap();
  }
  void wait_frame_available() { bufs_.wait_not_empty(); }
  uint64_t frame_count() { return frame_count_; }
  void cancel() { bufs_.cancel_waits(); }
  
protected:

  DoubleBuffer<RGB, W, H> bufs_;
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

