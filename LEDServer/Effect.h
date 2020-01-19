#pragma once
#include "boost/asio.hpp"

struct RGB {
  RGB() {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r_(r), g_(g), b_(b){}
  uint8_t r_;
  uint8_t g_;
  uint8_t b_;
};

const int H = 144;

template <int W, int H>
class DoubleBuffer {
public:
  
  DoubleBuffer() : front_(0) {}
  ~DoubleBuffer() {}

  void swap() {
    front_ = !front_;
    std::copy(bufs_[front_], bufs_[front_] + sizeof(bufs_[front_]), bufs_[!front_]);
  }

  boost::asio::const_buffer front() {
    return boost::asio::buffer(bufs_[front_], sizeof(bufs_[front_]));
  }
  
  RGB* back() {
    return bufs_[!front_];
  }
  
private:

  int front_;
  RGB bufs_[2][W * H];    
};

template<int W, int H>
class Canvas {
public:
  
  Canvas(DoubleBuffer<W, H> & buf) :
    buf_(buf)
  {
    
  }

  ~Canvas() {
    buf_.swap();
  }

  RGB* operator[](int x) {
    return &buf_.back()[x * H];
  }
  
private:
  
  DoubleBuffer<W, H>& buf_;
};

template <int W, int H>
class Effect {
public:

  Effect() {}
  virtual ~Effect() {};
  virtual void draw_frame() = 0;
  boost::asio::const_buffer buf() { return bufs_.front(); }
  
protected:
  
    DoubleBuffer<W, H> bufs_;  
};

template <int W>
class Test : public Effect<W, H> {
public:
  Test() {}
  void draw_frame() {
    Canvas<W, H> c(this->bufs_);
    c[0][0] = RGB(255, 0, 0);
  }
};
