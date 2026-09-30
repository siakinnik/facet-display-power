// Settings and the pure decision logic: "should the screen be on now?".
// No I/O here, so it is unit-tested directly.
#pragma once

#include <string>

#include "facet/json.h"

namespace dp {

enum class Mode { AlwaysOn = 0, Camera = 1, Off = 2 };

struct Settings {
    bool enabled = true;         // manage the screen at all
    std::string camera;          // camera id; empty = first found
    int interval_s = 2;          // presence check period
    int sensitivity = 1;         // 0 low, 1 medium, 2 high
    int presence_hold_s = 60;    // keep on after the last detected presence
    int touch_hold_s = 60;       // keep on after a touch
    int day_start = 7 * 60;      // minutes since midnight
    int night_start = 23 * 60;
    Mode day_mode = Mode::AlwaysOn;
    Mode night_mode = Mode::Camera;

    facet::Json to_json() const;
    static Settings from_json(const facet::Json& j);
};

bool is_night(const Settings& s, int minute_of_day);
Mode current_mode(const Settings& s, int minute_of_day);

struct Inputs {
    int minute_of_day = 0;
    double since_touch = 1e9;     // seconds
    double since_presence = 1e9;  // seconds
    bool camera_ok = false;
};

struct Decision {
    bool on = true;
    const char* reason = "";  // English key; translate before showing
};

Decision decide(const Settings& s, const Inputs& in);

}  // namespace dp
