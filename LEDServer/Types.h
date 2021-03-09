#pragma once

#include "ColorSpace.h"

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


