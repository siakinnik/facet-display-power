#include "policy.h"

#include <algorithm>

namespace dp {

using facet::Json;

Json Settings::to_json() const {
    Json j = Json::object();
    j["enabled"] = enabled;
    j["camera"] = camera;
    j["interval_s"] = interval_s;
    j["sensitivity"] = sensitivity;
    j["presence_hold_s"] = presence_hold_s;
    j["touch_hold_s"] = touch_hold_s;
    j["day_start"] = day_start;
    j["night_start"] = night_start;
    j["day_mode"] = int(day_mode);
    j["night_mode"] = int(night_mode);
    return j;
}

Settings Settings::from_json(const Json& j) {
    Settings d, s;
    auto mode = [](const Json& v, Mode def) {
        int m = v.as_int(int(def));
        return m >= 0 && m <= 2 ? Mode(m) : def;
    };
    s.enabled = j["enabled"].as_bool(d.enabled);
    s.camera = j["camera"].as_string(d.camera);
    s.interval_s = std::clamp(j["interval_s"].as_int(d.interval_s), 1, 120);
    s.sensitivity = std::clamp(j["sensitivity"].as_int(d.sensitivity), 0, 2);
    s.presence_hold_s = std::clamp(j["presence_hold_s"].as_int(d.presence_hold_s), 5, 3600);
    s.touch_hold_s = std::clamp(j["touch_hold_s"].as_int(d.touch_hold_s), 5, 3600);
    s.day_start = std::clamp(j["day_start"].as_int(d.day_start), 0, 1439);
    s.night_start = std::clamp(j["night_start"].as_int(d.night_start), 0, 1439);
    s.day_mode = mode(j["day_mode"], d.day_mode);
    s.night_mode = mode(j["night_mode"], d.night_mode);
    return s;
}

bool is_night(const Settings& s, int m) {
    if (s.day_start == s.night_start) return false;  // no night configured
    if (s.day_start < s.night_start) return m < s.day_start || m >= s.night_start;
    // Day wraps midnight (e.g. day 20:00 → 04:00).
    return m >= s.night_start && m < s.day_start;
}

Mode current_mode(const Settings& s, int m) { return is_night(s, m) ? s.night_mode : s.day_mode; }

Decision decide(const Settings& s, const Inputs& in) {
    if (!s.enabled) return {true, "control disabled"};
    if (in.since_touch < s.touch_hold_s) return {true, "recent touch"};
    switch (current_mode(s, in.minute_of_day)) {
        case Mode::AlwaysOn: return {true, "always on"};
        case Mode::Off: return {false, "off by schedule"};
        case Mode::Camera:
            // Fail-safe: without a working camera the screen stays on.
            if (!in.camera_ok) return {true, "camera unavailable"};
            if (in.since_presence < s.presence_hold_s) return {true, "someone is here"};
            return {false, "nobody around"};
    }
    return {true, ""};
}

}  // namespace dp
