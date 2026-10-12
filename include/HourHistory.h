#pragma once

#include <Preferences.h>

#include <stdint.h>
#include <time.h>

// 48 local hours. Slot i on the page is currentHour - (47 - i) hours.
// A slot is kept only while its hourStart matches that hour.
class HourHistory {
 public:
  static constexpr size_t kCapacity = 48;

  struct Point {
    bool valid;
    float temperature;
    float humidity;
  };

  HourHistory() {
    store_.magic = kMagic;
    store_.version = kVersion;
  }

  bool load(const char* nvsNamespace) {
    Preferences prefs;
    if (!prefs.begin(nvsNamespace, true)) {
      return false;
    }

    Store loaded{};
    const size_t stored = prefs.getBytesLength("hours");
    size_t got = 0;
    if (stored == sizeof(loaded)) {
      got = prefs.getBytes("hours", &loaded, sizeof(loaded));
    }
    prefs.end();

    if (got != sizeof(loaded) || loaded.magic != kMagic || loaded.version != kVersion) {
      return false;
    }

    store_ = loaded;
    dirty_ = false;
    return true;
  }

  bool save(const char* nvsNamespace) {
    if (!dirty_) {
      return true;
    }

    store_.magic = kMagic;
    store_.version = kVersion;
    store_.pad = 0;

    Preferences prefs;
    if (!prefs.begin(nvsNamespace, false)) {
      return false;
    }
    const size_t stored = prefs.putBytes("hours", &store_, sizeof(store_));
    prefs.end();
    if (stored != sizeof(store_)) {
      return false;
    }

    dirty_ = false;
    return true;
  }

  bool dirty() const { return dirty_; }

  // True when this sample opened a new hour slot.
  bool add(float temperature, float humidity, time_t hourStart) {
    if (hourStart <= 0) {
      return false;
    }

    const size_t index =
        static_cast<size_t>((static_cast<uint32_t>(hourStart) / 3600u) % kCapacity);
    Slot& slot = store_.slots[index];
    const bool opened = slot.hourStart != static_cast<int32_t>(hourStart);
    if (opened) {
      slot.hourStart = static_cast<int32_t>(hourStart);
      slot.tempSumCenti = 0;
      slot.humSumDeci = 0;
      slot.count = 0;
      slot.pad = 0;
    }
    if (slot.count == 0xFFFF) {
      return opened;
    }

    slot.tempSumCenti += scale(temperature, 100.0f);
    slot.humSumDeci += scale(humidity, 10.0f);
    ++slot.count;
    dirty_ = true;
    return opened;
  }

  Point at(size_t index, time_t currentHour) const {
    Point point{};
    if (index >= kCapacity || currentHour <= 0) {
      return point;
    }

    const time_t age = static_cast<time_t>(kCapacity - 1 - index);
    const time_t want = currentHour - age * 3600;
    if (want <= 0) {
      return point;
    }

    const size_t slotIndex =
        static_cast<size_t>((static_cast<uint32_t>(want) / 3600u) % kCapacity);
    const Slot& slot = store_.slots[slotIndex];
    if (slot.hourStart != static_cast<int32_t>(want) || slot.count == 0) {
      return point;
    }

    point.valid = true;
    point.temperature =
        static_cast<float>(slot.tempSumCenti) / (100.0f * static_cast<float>(slot.count));
    point.humidity =
        static_cast<float>(slot.humSumDeci) / (10.0f * static_cast<float>(slot.count));
    return point;
  }

 private:
  static constexpr uint32_t kMagic = 0x48343831;
  static constexpr uint16_t kVersion = 1;

  struct Slot {
    int32_t hourStart;
    int32_t tempSumCenti;
    int32_t humSumDeci;
    uint16_t count;
    uint16_t pad;
  };

  struct Store {
    uint32_t magic;
    uint16_t version;
    uint16_t pad;
    Slot slots[kCapacity];
  };

  static_assert(sizeof(Slot) == 16, "hour slot layout");
  static_assert(sizeof(Store) == 8 + sizeof(Slot) * kCapacity, "hour store layout");

  static int32_t scale(float value, float factor) {
    const float scaled = value * factor;
    return static_cast<int32_t>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
  }

  Store store_{};
  bool dirty_ = false;
};
