#include "watcher.h"

#include <chrono>

#include "facet/plugin.h"

namespace dp {

namespace {
// From this check interval on, the sensor is switched off between checks.
constexpr int kPowerDownFrom = 5;
}  // namespace

double now_s() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

Watcher::Watcher() {
    // Assume someone is present at start so the screen never blanks right after boot.
    status_.last_presence = now_s();
    thread_ = std::thread([this] { run(); });
}

Watcher::~Watcher() {
    {
        std::lock_guard<std::mutex> l(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    thread_.join();
}

void Watcher::configure(const WatchConfig& cfg) {
    {
        std::lock_guard<std::mutex> l(mu_);
        if (cfg.active == cfg_.active && cfg.camera == cfg_.camera && cfg.interval_s == cfg_.interval_s &&
            cfg.sensitivity == cfg_.sensitivity)
            return;
        if (cfg.active && !cfg_.active) status_.last_presence = now_s();  // grace period on activation
        cfg_ = cfg;
        changed_ = true;
    }
    cv_.notify_all();
}

WatchStatus Watcher::status() const {
    std::lock_guard<std::mutex> l(mu_);
    return status_;
}

void Watcher::run() {
    Camera cam;
    PresenceDetector detector;
    std::vector<uint8_t> frame;
    double retry_at = 0;

    std::unique_lock<std::mutex> lock(mu_);
    while (!stop_) {
        WatchConfig cfg = cfg_;
        changed_ = false;

        if (!cfg.active) {
            if (cam.is_open()) {
                cam.close();
                facet::sdk::Plugin::log("camera closed (not needed now)");
            }
            status_.camera_ok = false;
            status_.error = {};
            cv_.wait(lock, [this] { return stop_ || changed_; });
            continue;
        }

        lock.unlock();
        // (Re)open the camera when needed; retry every 10 s after a failure.
        facet::i18n::Text error;
        bool want_reopen = !cam.is_open() || (!cfg.camera.empty() && cam.id() != cfg.camera);
        if (want_reopen && now_s() >= retry_at) {
            std::string id = cfg.camera;
            if (id.empty()) {
                auto cams = list_cameras();
                if (!cams.empty()) id = cams.front().id;
            }
            if (id.empty()) {
                error = "no camera found";
            } else if (cam.open(id, error)) {
                facet::sdk::Plugin::log("camera reserved: %s (%s)", cam.name().c_str(), id.c_str());
                detector.reset();
            }
            if (!cam.is_open()) retry_at = now_s() + 10;
        }

        // The device stays reserved while checks are needed, but with longer
        // intervals the sensor is only powered for the check itself (about a
        // second for auto-exposure plus one frame), which keeps it cool and
        // frees USB bandwidth for the camera's other sensor.
        PresenceResult res;
        bool grabbed = false;
        if (cam.is_open()) {
            bool keep_streaming = cfg.interval_s < kPowerDownFrom;
            bool started = cam.streaming();
            if (!started && cam.start(error)) {
                started = true;
                cam.warm_up(keep_streaming ? 1.5 : 1.0);
            }
            int w = 0, h = 0;
            if (started && cam.grab(frame, w, h, error)) {
                res = detector.feed(frame.data(), w, h, w, cfg.sensitivity);
                grabbed = true;
                if (!keep_streaming) cam.stop();
            } else {
                facet::sdk::Plugin::log("camera error: %s", facet::i18n::format(error.key, error.args).c_str());
                cam.close();
                retry_at = now_s() + 5;
            }
        }
        lock.lock();

        double now = now_s();
        status_.camera_ok = cam.is_open() && grabbed;
        status_.camera_name = cam.is_open() ? cam.name() : std::string();
        if (!error.empty() || grabbed) status_.error = error;  // keep the reason while waiting to retry
        status_.last_check = now;
        if (grabbed) {
            status_.motion = res.motion;
            status_.dark = res.dark;
            if (res.present) {
                if (now - status_.last_presence > 10)
                    facet::sdk::Plugin::log("presence: motion %.1f%% after %.0fs quiet", res.motion * 100,
                                            now - status_.last_presence);
                status_.last_presence = now;
            }
        }
        cv_.wait_for(lock, std::chrono::seconds(std::max(1, cfg.interval_s)), [this] { return stop_ || changed_; });
    }
    cam.close();
}

}  // namespace dp
