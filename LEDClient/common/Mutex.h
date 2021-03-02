#pragma once

class Mutex {
 public:
  Mutex() : lock_(portMUX_INITIALIZER_UNLOCKED) {}
  void lock() { portENTER_CRITICAL(&lock_); }
  void unlock() { portEXIT_CRITICAL(&lock_); }

 private:
  portMUX_TYPE lock_;
};