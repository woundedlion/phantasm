#pragma once
#include "Types.h"
#include "FrameBuffer.h"
#include "boost/asio.hpp"

class Effect {
 public:
  Effect() : frame_count_(0) {}
  virtual ~Effect(){};
  virtual bool show_bg() { return true; }
  virtual void draw_frame(RGBFrameBuffer::Frame& frame) = 0;

 protected:
  uint64_t frame_count_;
};

class Test : public Effect {
 public:
  Test() {}

  void draw_frame(RGBFrameBuffer::Frame& frame) {
    for (int x = 0; x < Config::W; ++x) {
      for (int y = 0; y < Config::H; ++y) {
        if (x % 8 == 0 || y % 8 == 6) {
          frame.pixel(x,y) = RGB(0x00, 0xff, 0x00);
        } else {
          frame.pixel(x,y) = RGB(0x00, 0x00, 0x00);
        }
      }
    }
  }
};

/*
#include "ColorSpace.h"

class RainbowHSV : public Effect {
 public:
  void draw_frame() {
    Canvas<W, H> _(this->bufs_);
    for (int x = 0; x < W; ++x) {
      for (int y = 0; y < H; ++y) {
        ColorSpace::Rgb rgb;
        ColorSpace::Hsv((y % Config::STRIP_H) * 360 / Config::STRIP_H, 1, 1).ToRgb(&rgb);
        _[x][y] = rgb;
      }
    }
  }
};

class RainbowTwistHSV : public Effect {
 public:
  void draw_frame() {
    Canvas<W, H> _(this->bufs_);
    for (int x = 0; x < Config::W; ++x) {
      for (int y = 0; y < Config::H; ++y) {
        ColorSpace::Rgb rgb;
        ColorSpace::Hsv(((x + y) % Config::STRIP_H) * 360 / Config::STRIP_H, 1, 1).ToRgb(&rgb);
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
*/