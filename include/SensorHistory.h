#pragma once

#include <stddef.h>

// One-minute window at one sample per second.
// Index 0 is 59 seconds ago, index kCapacity - 1 is the current second.
// Seconds that have not been recorded yet, and failed reads, are missing.
class SensorHistory {
 public:
  static constexpr size_t kCapacity = 60;

  void push(float temperature, float humidity, bool valid) {
    samples_[head_].temperature = temperature;
    samples_[head_].humidity = humidity;
    samples_[head_].valid = valid;
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) {
      ++count_;
    }
  }

  bool at(size_t index, float& temperature, float& humidity) const {
    if (index >= kCapacity || count_ == 0 || index < kCapacity - count_) {
      return false;
    }

    const size_t ageFromNewest = kCapacity - 1 - index;
    const size_t physical = (head_ + kCapacity - 1 - ageFromNewest) % kCapacity;
    const Sample& sample = samples_[physical];
    if (!sample.valid) {
      return false;
    }

    temperature = sample.temperature;
    humidity = sample.humidity;
    return true;
  }

 private:
  struct Sample {
    float temperature;
    float humidity;
    bool valid;
  };

  Sample samples_[kCapacity]{};
  size_t head_ = 0;
  size_t count_ = 0;
};
