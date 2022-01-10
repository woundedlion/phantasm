#pragma once

#include <unordered_map>
#include "ColorSpace.h"

struct Config {
  static const int W = 288;
  static const int H = 144;
  static const int STRIP_H = 48;
  static const int SLICE_COUNT = 1;

  static const char* _slices[SLICE_COUNT];
};

struct __attribute__((__packed__)) RGB {
  RGB() {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : b_(b), g_(g), r_(r) {}
  RGB(const ColorSpace::Rgb& c) : r_(c.r), g_(c.g), b_(c.b) {}
  RGB& operator=(const ColorSpace::Rgb& c) {
    r_ = c.r;
    g_ = c.g;
    b_ = c.b;
    return *this;
  }
  operator ColorSpace::Rgb() { return ColorSpace::Rgb(r_, g_, b_); }
  uint8_t b_;
  uint8_t g_;
  uint8_t r_;
};

class RGBFrame {
 public:
  RGB& pixel(int x, int y) { return buf_[x + y * Config::W]; }
  boost::asio::const_buffer slice_data(int slice_idx) {
    return boost::asio::buffer(buf_ + slice_idx * Config::STRIP_H,
                               Config::W * Config::STRIP_H * sizeof(RGB));
  }

 private:
  RGB buf_[Config::W * Config::H];
};

