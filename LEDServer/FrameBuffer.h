#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <map>

#include "Types.h"

template <typename T, size_t MAX_FRAMES, size_t MAX_CLIENTS>
class FrameBuffer {
 public:
  typedef T Frame;
  typedef std::shared_ptr<T> FramePtr;
  constexpr size_t max_frames() { return MAX_FRAMES; }
  constexpr size_t max_clients() { return MAX_CLIENTS; }

  FrameBuffer() : canceled_(false) {}
  ~FrameBuffer() {}

  void clear() {
    std::scoped_lock _(lock_);
    frames_.clear();
  }

  void push(uint64_t num, std::shared_ptr<T> frame) {
    std::unique_lock lock(lock_);
    while (!canceled_ && frames_.size() == MAX_FRAMES) {
      size_cond_.wait(lock);
    }
    frames_[num] = FrameRef(num, frame);
    size_cond_.notify_all();
  }

  FramePtr pop(uint64_t frame_num) {
    std::unique_lock lock(lock_);
    while (!canceled_ &&
            (frames_.empty() || frames_.rbegin()->first < frame_num)) {
        size_cond_.wait(lock);
    }
    auto& frameref = frames_[frame_num];
    auto frame = frameref.frame_;
    if (--frameref.refs_ == 0) {
        frames_.erase(frame_num);
        size_cond_.notify_all();
    }
    return frame;
  }

  void cancel() {
    {
      std::scoped_lock _(lock_);
      canceled_ = true;
    }
    size_cond_.notify_all();
  }

 private:
  struct FrameRef {
    FrameRef() : refs_(0), num_(-1) {}
    FrameRef(uint64_t num, FramePtr frame)
        : refs_(MAX_CLIENTS), num_(num), frame_(frame) {}
    FrameRef(const FrameRef& fr)
        : refs_(fr.refs_), num_(fr.num_), frame_(fr.frame_) {}

    int refs_;
    uint64_t num_;
    FramePtr frame_;
  };

  std::mutex lock_;
  std::condition_variable size_cond_;
  std::map<uint64_t, FrameRef> frames_;
  bool canceled_;
};

typedef FrameBuffer<RGBFrame, 16, Config::SLICE_COUNT> RGBFrameBuffer;
