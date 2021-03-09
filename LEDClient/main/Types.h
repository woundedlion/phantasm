#pragma once

struct __attribute__((__packed__)) RGB {
  RGB() {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : b_(b), g_(g), r_(r) {}
  uint8_t b_;
  uint8_t g_;
  uint8_t r_;
};


