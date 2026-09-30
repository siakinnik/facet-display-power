// display-power: turns the panel's screen on/off by schedule, touch and
// camera presence. Runs as a Facet plugin process (see facet-core docs).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

#include "camera.h"
#include "facet/plugin.h"
#include "i18n/i18n.h"
#include "policy.h"
#include "watcher.h"

using facet::Json;
using facet::sdk::Plugin;
using facet::sdk::Screen;

namespace {

#ifndef DP_VERSION
#define DP_VERSION "dev"  // set by CMake
#endif
constexpr const char* kVersion = DP_VERSION;
const char* kModes[] = {"Always on", "Camera", "Off"};
const char* kSensitivity[] = {"Low", "Medium", "High"};

int minute_of_day() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm.tm_hour * 60 + tm.tm_min;
}

class DisplayPower {
public:
    explicit DisplayPower(Plugin& plugin) : plugin_(plugin) {}

    void load() {
        Json j;
        if (facet::load_json_file(path(), j)) settings_ = dp::Settings::from_json(j);
        cameras_ = dp::list_cameras();
    }

    void save() {
        if (!facet::save_json_file(path(), settings_.to_json()))
            Plugin::log("cannot save settings to %s", path().c_str());
    }

    void on_event(const std::string& id, const Json& v) {
        dp::Settings& s = settings_;
        if (id == "enabled") s.enabled = v.as_bool(s.enabled);
        else if (id == "day_start") s.day_start = v.as_int(s.day_start);
        else if (id == "night_start") s.night_start = v.as_int(s.night_start);
        else if (id == "day_mode") s.day_mode = dp::Mode(v.as_int(0));
        else if (id == "night_mode") s.night_mode = dp::Mode(v.as_int(0));
        else if (id == "interval") s.interval_s = v.as_int(s.interval_s);
        else if (id == "sensitivity") s.sensitivity = v.as_int(s.sensitivity);
        else if (id == "presence_hold") s.presence_hold_s = v.as_int(s.presence_hold_s);
        else if (id == "touch_hold") s.touch_hold_s = v.as_int(s.touch_hold_s);
        else if (id == "camera") {
            int i = v.as_int(0);
            s.camera = i >= 1 && i <= int(cameras_.size()) ? cameras_[size_t(i - 1)].id : std::string();
        } else if (id == "rescan") {
            cameras_ = dp::list_cameras();
        }
        settings_ = dp::Settings::from_json(settings_.to_json());  // re-validate ranges
        save();
        update();
    }

    void on_activity() { last_touch_ = dp::now_s(); }

    // Runs every tick: decide, publish, refresh the UI.
    void update() {
        int minute = minute_of_day();
        dp::Mode mode = dp::current_mode(settings_, minute);
        bool camera_needed = settings_.enabled && mode == dp::Mode::Camera;
        watcher_.configure({camera_needed, settings_.camera, settings_.interval_s, settings_.sensitivity});

        dp::WatchStatus st = watcher_.status();
        double now = dp::now_s();
        dp::Inputs in;
        in.minute_of_day = minute;
        in.since_touch = now - last_touch_;
        in.since_presence = now - st.last_presence;
        in.camera_ok = st.camera_ok;
        dp::Decision d = dp::decide(settings_, in);
        plugin_.request_display(d.on);

        bool night = dp::is_night(settings_, minute);
        plugin_.set_tile(tr(d.on ? "Screen on" : "Screen off") + " · " + tr(d.reason));
        if (plugin_.visible()) plugin_.set_ui(build_ui(st, d, mode, night, now));
    }

private:
    std::string tr(std::string_view key) const { return plugin_.tr(key); }
    std::string tr(std::string_view key, const std::vector<std::string>& args) const { return plugin_.tr(key, args); }
    std::string path() const { return plugin_.data_dir() + "/settings.json"; }

    std::string ago(double s) const {
        if (s < 60) return tr("{} s ago", {std::to_string(int(s))});
        if (s < 3600) return tr("{} min ago", {std::to_string(int(s / 60))});
        return tr("{} h ago", {std::to_string(int(s / 3600))});
    }

    template <size_t N>
    std::vector<std::string> options(const char* const (&keys)[N]) const {
        std::vector<std::string> out;
        for (const char* k : keys) out.push_back(tr(k));
        return out;
    }

    Screen build_ui(const dp::WatchStatus& st, const dp::Decision& d, dp::Mode mode, bool night, double now) {
        const dp::Settings& s = settings_;
        const std::string sec = tr("s");
        Screen ui(tr("Screen & camera"));

        ui.section(tr("Now"));
        ui.info(tr("Screen"), tr(d.on ? "on — {}" : "off — {}", {tr(d.reason)}), d.on ? "good" : "dim");
        ui.info(tr("Period"), tr(night ? "night" : "day") + " · " + tr(kModes[size_t(mode)]));
        if (mode == dp::Mode::Camera && s.enabled) {
            if (st.camera_ok) {
                ui.info(tr("Camera"), st.camera_name, "good");
                ui.level(tr("Motion"), std::min(1.0, st.motion * 10.0), std::to_string(int(st.motion * 100)) + "%");
                double since = now - st.last_presence;
                bool here = since < s.presence_hold_s;
                ui.info(tr("Person"), here ? tr("here") : tr("gone · {}", {ago(since)}), here ? "good" : "dim");
                if (st.dark) ui.note(tr("Too dark in view: the camera may not see anyone. It needs IR illumination or light."));
            } else {
                ui.info(tr("Camera"), st.error.empty() ? tr("connecting…") : plugin_.render(st.error), "warn");
                ui.note(tr("While the camera is unavailable, the screen stays on."));
            }
        }

        ui.section(tr("Control"));
        ui.toggle("enabled", tr("Manage the screen"), s.enabled);

        ui.section(tr("Schedule"));
        ui.time("day_start", tr("Day starts"), s.day_start, 15);
        ui.select("day_mode", tr("Screen by day"), options(kModes), int(s.day_mode));
        ui.time("night_start", tr("Night starts"), s.night_start, 15);
        ui.select("night_mode", tr("Screen at night"), options(kModes), int(s.night_mode));

        ui.section(tr("Camera"));
        std::vector<std::string> cams = {tr("Auto (first found)")};
        int cam_index = 0;
        for (size_t i = 0; i < cameras_.size(); ++i) {
            cams.push_back(cameras_[i].name);
            if (cameras_[i].id == s.camera) cam_index = int(i) + 1;
        }
        if (!s.camera.empty() && cam_index == 0) {
            cams.push_back(tr("Not connected: {}", {s.camera}));
            cam_index = int(cams.size()) - 1;
        }
        ui.select("camera", tr("Camera"), cams, cam_index);
        ui.stepper("interval", tr("Check every"), s.interval_s, 1, 30, 1, sec);
        ui.select("sensitivity", tr("Sensitivity"), options(kSensitivity), s.sensitivity);
        ui.stepper("presence_hold", tr("Keep on after leaving"), s.presence_hold_s, 10, 900, 10, sec);
        ui.button("rescan", tr("Find cameras again"));

        ui.section(tr("Touch"));
        ui.stepper("touch_hold", tr("Screen after a touch"), s.touch_hold_s, 10, 900, 10, sec);
        ui.note(tr("The camera records and stores nothing: one frame per interval is analysed in memory for "
                   "motion only. The camera is switched on only during “Camera” periods."));
        return ui;
    }

    Plugin& plugin_;
    dp::Settings settings_;
    std::vector<dp::CameraInfo> cameras_;
    dp::Watcher watcher_;
    double last_touch_ = dp::now_s();
};

// `display-power --probe [seconds] [camera-id]`: lists cameras and prints live
// motion levels for the given (or first usable) one. For setting up a device.
int probe(int seconds, const std::string& only) {
    auto english = [](const facet::i18n::Text& t) { return facet::i18n::format(t.key, t.args); };
    auto cams = dp::list_cameras();
    std::printf("cameras: %zu\n", cams.size());
    for (const auto& c : cams) std::printf("  %s  (%s)\n", c.name.c_str(), c.id.c_str());
    for (const auto& c : cams) {
        if (!only.empty() && c.id != only) continue;
        dp::Camera cam;
        facet::i18n::Text err;
        if (!cam.open(c.id, err)) {
            std::printf("%s: %s\n", c.id.c_str(), english(err).c_str());
            continue;
        }
        std::printf("using %s (%s)\n", cam.name().c_str(), c.id.c_str());
        cam.warm_up(1.5);
        dp::PresenceDetector det;
        std::vector<uint8_t> frame;
        for (int i = 0; i < seconds; ++i) {
            int w = 0, h = 0;
            if (!cam.grab(frame, w, h, err)) {
                std::printf("grab: %s\n", english(err).c_str());
                return 1;
            }
            long sum = 0;
            for (uint8_t v : frame) sum += v;
            auto r = det.feed(frame.data(), w, h, w, 1);
            std::printf("%dx%d brightness=%3ld motion=%5.1f%% %s%s\n", w, h, sum / long(frame.size()),
                        r.motion * 100, r.present ? "PRESENT" : "-", r.dark ? " dark" : "");
            std::fflush(stdout);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return 0;
    }
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--probe")
        return probe(argc > 2 ? std::atoi(argv[2]) : 10, argc > 3 ? argv[3] : "");

    Plugin plugin("display-power", kVersion);
    dp::register_translations(plugin.catalog());
    DisplayPower app(plugin);

    plugin.on_hello = [&](const Json&) {
        app.load();
        app.update();
    };
    plugin.on_event = [&](const std::string& id, const Json& v) { app.on_event(id, v); };
    plugin.on_activity = [&] {
        app.on_activity();
        app.update();
    };
    plugin.on_visible = [&](bool) { app.update(); };
    plugin.on_locale = [&](const std::string&) { app.update(); };
    plugin.on_tick = [&] { app.update(); };
    return plugin.run(500);
}
