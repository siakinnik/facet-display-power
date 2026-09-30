// Presence detector on grayscale frames taken every N seconds.
//
// Current method: frame differencing with exposure compensation on a 80x60
// thumbnail — "something moved in view since the last check". Combined with
// the hold time in policy.cpp it answers "is somebody here". The interface
// allows swapping in a real person detector later without touching callers.
#pragma once

#include <cstdint>
#include <vector>

namespace dp {

struct PresenceResult {
    float motion = 0;  // fraction of changed pixels, 0..1
    bool present = false;
    bool dark = false;  // too dark to see anything
    bool first = true;  // no previous frame to compare with yet
};

class PresenceDetector {
public:
    static constexpr int kW = 80, kH = 60;

    void reset() { prev_.clear(); }
    PresenceResult feed(const uint8_t* luma, int w, int h, int stride, int sensitivity);

private:
    std::vector<uint8_t> prev_;
};

}  // namespace dp
