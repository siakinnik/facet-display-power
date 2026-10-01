// V4L2 camera: discovery and grabbing the luma plane of a fresh frame.
// Frames are only held in memory for analysis; nothing is stored.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "facet/i18n.h"

namespace dp {

struct CameraInfo {
    std::string id;    // stable path (/dev/v4l/by-path/...) when available
    std::string name;  // human-readable card name
};

std::vector<CameraInfo> list_cameras();

class Camera {
public:
    ~Camera() { close(); }
    // Errors are untranslated messages; render them with the plugin catalog.
    // open() reserves the device (no other program can stream from it) but
    // leaves the sensor off; start()/stop() switch the sensor on and off.
    bool open(const std::string& id, facet::i18n::Text& error);
    void close();
    bool start(facet::i18n::Text& error);
    void stop();
    bool is_open() const { return fd_ >= 0; }
    bool streaming() const { return streaming_; }
    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }

    // Discards frames for `seconds` so auto-exposure settles (while streaming).
    void warm_up(double seconds);
    // Waits for a new frame (up to 2 s) and copies its luma into `out`.
    bool grab(std::vector<uint8_t>& out, int& w, int& h, facet::i18n::Text& error);

private:
    struct Buffer {
        void* ptr = nullptr;
        size_t len = 0;
    };
    bool extract(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

    int fd_ = -1;
    std::string id_, name_;
    uint32_t fourcc_ = 0;
    int width_ = 0, height_ = 0, stride_ = 0;
    std::vector<Buffer> buffers_;
    bool streaming_ = false;
};

}  // namespace dp
