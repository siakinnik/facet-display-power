// Background thread that owns the camera: opens it only while presence
// checks are needed, grabs one frame per interval and runs the detector.
// The plugin's main thread never blocks on camera I/O.
#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "camera.h"
#include "facet/i18n.h"
#include "presence.h"

namespace dp {

struct WatchConfig {
    bool active = false;
    std::string camera;  // empty = first available
    int interval_s = 2;
    int sensitivity = 1;
};

struct WatchStatus {
    bool camera_ok = false;
    std::string camera_name;
    facet::i18n::Text error;  // untranslated; empty when fine
    float motion = 0;
    bool dark = false;
    double last_presence = 0;  // steady-clock seconds
    double last_check = 0;
};

class Watcher {
public:
    Watcher();
    ~Watcher();
    void configure(const WatchConfig& cfg);
    WatchStatus status() const;

private:
    void run();

    mutable std::mutex mu_;
    std::condition_variable cv_;
    WatchConfig cfg_;
    WatchStatus status_;
    bool stop_ = false, changed_ = false;
    std::thread thread_;
};

double now_s();

}  // namespace dp
