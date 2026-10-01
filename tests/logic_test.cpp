// Unit tests for the schedule/decision logic and the presence detector.
#include <cstdio>
#include <vector>

#include "policy.h"
#include "presence.h"

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

static void test_schedule() {
    dp::Settings s;  // day 07:00-23:00
    CHECK(!dp::is_night(s, 12 * 60));
    CHECK(dp::is_night(s, 23 * 60));
    CHECK(dp::is_night(s, 3 * 60));
    CHECK(!dp::is_night(s, 7 * 60));

    s.day_start = 20 * 60;  // day wraps midnight
    s.night_start = 4 * 60;
    CHECK(!dp::is_night(s, 22 * 60));
    CHECK(!dp::is_night(s, 1 * 60));
    CHECK(dp::is_night(s, 12 * 60));
}

static void test_decide() {
    dp::Settings s;
    s.day_mode = dp::Mode::AlwaysOn;
    s.night_mode = dp::Mode::Camera;
    dp::Inputs in;
    in.minute_of_day = 12 * 60;
    CHECK(dp::decide(s, in).on);  // day: 24/7

    in.minute_of_day = 2 * 60;  // night: camera
    in.camera_ok = true;
    in.since_presence = 5;
    CHECK(dp::decide(s, in).on);
    in.since_presence = 1000;
    CHECK(!dp::decide(s, in).on);
    in.camera_ok = false;  // fail-safe
    CHECK(dp::decide(s, in).on);

    s.night_mode = dp::Mode::Off;  // "screen off by time"
    in.camera_ok = true;
    CHECK(!dp::decide(s, in).on);
    in.since_touch = 3;  // touch wakes it anyway
    CHECK(dp::decide(s, in).on);

    s.enabled = false;
    in.since_touch = 1e9;
    CHECK(dp::decide(s, in).on);
}

static void test_settings_roundtrip() {
    dp::Settings s;
    s.camera = "/dev/v4l/by-id/usb-cam";
    s.night_mode = dp::Mode::Off;
    s.interval_s = 5;
    dp::Settings r = dp::Settings::from_json(s.to_json());
    CHECK(r.camera == s.camera);
    CHECK(r.night_mode == dp::Mode::Off);
    CHECK(r.interval_s == 5);
    facet::Json bad = facet::Json::object();
    bad["interval_s"] = 100000;
    bad["day_mode"] = 9;
    dp::Settings c = dp::Settings::from_json(bad);
    CHECK(c.interval_s == 120);
    CHECK(c.day_mode == dp::Mode::AlwaysOn);
}

static void test_presence() {
    const int w = 320, h = 240;
    std::vector<uint8_t> a(w * h, 100), b = a;
    dp::PresenceDetector det;
    CHECK(det.feed(a.data(), w, h, w, 1).first);
    CHECK(!det.feed(a.data(), w, h, w, 1).present);  // static scene

    std::vector<uint8_t> brighter(w * h, 130);  // exposure change only
    CHECK(!det.feed(brighter.data(), w, h, w, 1).present);

    for (int y = 60; y < 200; ++y)  // a "person" appears
        for (int x = 100; x < 180; ++x) b[y * w + x] = 20;
    det.feed(a.data(), w, h, w, 1);
    auto r = det.feed(b.data(), w, h, w, 1);
    CHECK(r.present);
    CHECK(r.motion > 0.1f);

    std::vector<uint8_t> dark(w * h, 3);
    CHECK(det.feed(dark.data(), w, h, w, 1).dark);
}

int main() {
    test_schedule();
    test_decide();
    test_settings_roundtrip();
    test_presence();
    if (failures == 0) std::printf("all tests passed\n");
    return failures == 0 ? 0 : 1;
}
