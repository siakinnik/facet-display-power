#include "presence.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace dp {

PresenceResult PresenceDetector::feed(const uint8_t* luma, int w, int h, int stride, int sensitivity) {
    PresenceResult r;
    if (!luma || w < kW || h < kH) return r;

    // Box-average downscale: cheap and removes most sensor noise.
    std::vector<uint8_t> cur(size_t(kW) * kH);
    long sum_all = 0;
    for (int y = 0; y < kH; ++y) {
        int y0 = y * h / kH, y1 = (y + 1) * h / kH;
        for (int x = 0; x < kW; ++x) {
            int x0 = x * w / kW, x1 = (x + 1) * w / kW;
            long s = 0;
            for (int yy = y0; yy < y1; ++yy) {
                const uint8_t* row = luma + size_t(yy) * size_t(stride);
                for (int xx = x0; xx < x1; ++xx) s += row[xx];
            }
            int v = int(s / std::max(1, (y1 - y0) * (x1 - x0)));
            cur[size_t(y) * kW + x] = uint8_t(v);
            sum_all += v;
        }
    }
    float mean = float(sum_all) / float(kW * kH);
    r.dark = mean < 12.f;

    if (prev_.size() != cur.size()) {
        prev_ = std::move(cur);
        return r;
    }
    r.first = false;

    long prev_sum = 0;
    for (uint8_t v : prev_) prev_sum += v;
    float shift = mean - float(prev_sum) / float(kW * kH);  // auto-exposure drift

    static const int kThreshold[] = {28, 20, 14};
    static const float kMinRatio[] = {0.03f, 0.015f, 0.006f};
    int sens = std::clamp(sensitivity, 0, 2);
    int changed = 0;
    for (size_t i = 0; i < cur.size(); ++i) {
        float d = std::fabs(float(cur[i]) - float(prev_[i]) - shift);
        if (d > float(kThreshold[sens])) ++changed;
    }
    r.motion = float(changed) / float(cur.size());
    r.present = !r.dark && r.motion >= kMinRatio[sens];
    prev_ = std::move(cur);
    return r;
}

}  // namespace dp
