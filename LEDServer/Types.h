#pragma once

struct RGB {
  RGB() {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r_(r), g_(g), b_(b){}
  uint8_t r_;
  uint8_t g_;
  uint8_t b_;
};
