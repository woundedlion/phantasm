#pragma once
#include "Types.h"
#include "DoubleBuffer.h"
#include "boost/asio.hpp"

template <int W, int H>
class Canvas {
 public:
  Canvas(DoubleBuffer<RGB, W, H>& buf) : buf_(buf) {
    buf_.wait_not_full();
    buf_.stamp();
  }

  ~Canvas() { buf_.inc_used(); }

  RGB* operator[](int x) { return &buf_.back()[x * H]; }

 private:
  DoubleBuffer<RGB, W, H>& buf_;
};

class Effect {
 public:
  Effect() : frame_count_(0) {}
  virtual ~Effect(){};
  virtual bool show_bg() { return true; }
  virtual void draw_frame() = 0;

  boost::asio::const_buffer buf(int i) {
    return boost::asio::buffer(bufs_.front() + i * STRIP_H,
                               W * STRIP_H * sizeof(RGB));
  }

  void advance_frame() {
    frame_count_++;
    bufs_.swap();
  }

  bool wait_frame_available() { return bufs_.wait_not_empty(); }
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
    for (int x = 0; x < W; ++x) {
      for (int y = 0; y < H; ++y) {
        if (x % 8 == 0 || y % 8 == 6) {
          c[x][y] = RGB(0x00, 0xff, 0x00);
        } else {
          c[x][y] = RGB(0x00, 0x00, 0x00);
        }
      }
    }
  }
};

#include "ColorSpace.h"

class RainbowHSV : public Effect {
 public:
  void draw_frame() {
    Canvas<W, H> _(this->bufs_);
    for (int x = 0; x < W; ++x) {
      for (int y = 0; y < H; ++y) {
        ColorSpace::Rgb rgb;
        ColorSpace::Hsv((y % STRIP_H) * 360 / STRIP_H, 1, 1).ToRgb(&rgb);
        _[x][y] = rgb;
      }
    }
  }
};

class RainbowTwistHSV : public Effect {
 public:
  void draw_frame() {
    Canvas<W, H> _(this->bufs_);
    for (int x = 0; x < W; ++x) {
      for (int y = 0; y < H; ++y) {
        ColorSpace::Rgb rgb;
        ColorSpace::Hsv(((x + y) % STRIP_H) * 360 / STRIP_H, 1, 1).ToRgb(&rgb);
        _[x][y] = rgb;
      }
    }
  }
};

class RainbowHSL : public Effect {
 public:
  void draw_frame() {
    Canvas<W, H> _(this->bufs_);
    for (int x = 0; x < W; ++x) {
      for (int y = 0; y < H; ++y) {
        ColorSpace::Rgb rgb;
        ColorSpace::Hsl((y % STRIP_H) * 360 / STRIP_H, 100, 50).ToRgb(&rgb);
        _[x][y] = rgb;
      }
    }
  }
};
