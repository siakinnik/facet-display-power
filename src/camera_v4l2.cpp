#include <dirent.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>

#include "camera.h"

namespace dp {

namespace {

int xioctl(int fd, unsigned long req, void* arg) {
    int r;
    do r = ioctl(fd, req, arg);
    while (r < 0 && errno == EINTR);
    return r;
}

std::string real(const std::string& p) {
    char buf[PATH_MAX];
    return realpath(p.c_str(), buf) ? std::string(buf) : p;
}

// /dev/videoN -> /dev/v4l/by-path/<port+interface> (stable across reboots).
// by-id is not usable: a composite webcam (RGB + IR) exposes both nodes
// under the same by-id name, and udev keeps only one of them.
std::map<std::string, std::string> stable_ids() {
    std::map<std::string, std::string> out;
    const char* dir = "/dev/v4l/by-path";
    if (DIR* d = opendir(dir)) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            std::string link = std::string(dir) + "/" + e->d_name;
            std::string target = real(link);
            bool legacy_alias = std::strstr(e->d_name, "-usbv2-") != nullptr;  // duplicate naming scheme
            auto it = out.find(target);
            if (it == out.end() || (!legacy_alias && it->second.find("-usbv2-") != std::string::npos))
                out[target] = link;
        }
        closedir(d);
    }
    return out;
}

bool is_capture(int fd, v4l2_capability& cap) {
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) return false;
    uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    return (caps & V4L2_CAP_VIDEO_CAPTURE) && (caps & V4L2_CAP_STREAMING);
}

}  // namespace

std::vector<CameraInfo> list_cameras() {
    std::vector<CameraInfo> out;
    auto ids = stable_ids();
    for (int i = 0; i < 64; ++i) {
        std::string path = "/dev/video" + std::to_string(i);
        int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        v4l2_capability cap{};
        if (is_capture(fd, cap)) {
            auto it = ids.find(path);
            out.push_back({it != ids.end() ? it->second : path, reinterpret_cast<const char*>(cap.card)});
        }
        ::close(fd);
    }
    return out;
}

bool Camera::open(const std::string& id, facet::i18n::Text& error) {
    close();
    fd_ = ::open(id.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        error = {"cannot open: {}", {std::strerror(errno)}};
        return false;
    }
    v4l2_capability cap{};
    if (!is_capture(fd_, cap)) {
        error = "device cannot capture video";
        close();
        return false;
    }
    id_ = id;
    name_ = reinterpret_cast<const char*>(cap.card);

    // Pick a format we can read luma from without a decoder.
    static const uint32_t kPreferred[] = {V4L2_PIX_FMT_GREY, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY,
                                          V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_YUV420};
    std::vector<uint32_t> available;
    v4l2_fmtdesc desc{};
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (xioctl(fd_, VIDIOC_ENUM_FMT, &desc) == 0) {
        available.push_back(desc.pixelformat);
        ++desc.index;
    }
    uint32_t chosen = 0;
    for (uint32_t f : kPreferred)
        if (std::find(available.begin(), available.end(), f) != available.end()) {
            chosen = f;
            break;
        }
    if (!chosen) {
        error = "no uncompressed format (needs YUYV/GREY/NV12)";
        close();
        return false;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 320;
    fmt.fmt.pix.height = 240;
    fmt.fmt.pix.pixelformat = chosen;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        error = {"cannot set format: {}", {std::strerror(errno)}};
        close();
        return false;
    }
    fourcc_ = fmt.fmt.pix.pixelformat;
    width_ = int(fmt.fmt.pix.width);
    height_ = int(fmt.fmt.pix.height);
    stride_ = int(fmt.fmt.pix.bytesperline);
    if (stride_ == 0) stride_ = (fourcc_ == V4L2_PIX_FMT_YUYV || fourcc_ == V4L2_PIX_FMT_UYVY) ? width_ * 2 : width_;

    // Ask for a low frame rate: we look at one frame every few seconds.
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe = {1, 5};
    xioctl(fd_, VIDIOC_S_PARM, &parm);

    v4l2_requestbuffers req{};
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
        error = {"cannot allocate buffers: {}", {std::strerror(errno)}};
        close();
        return false;
    }
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer b{};
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &b) < 0) {
            error = {"cannot allocate buffers: {}", {"QUERYBUF"}};
            close();
            return false;
        }
        void* p = mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, b.m.offset);
        if (p == MAP_FAILED) {
            error = {"cannot allocate buffers: {}", {"mmap"}};
            close();
            return false;
        }
        buffers_.push_back({p, b.length});
        xioctl(fd_, VIDIOC_QBUF, &b);
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        error = {"cannot start streaming: {}", {std::strerror(errno)}};
        close();
        return false;
    }
    streaming_ = true;
    return true;
}

void Camera::close() {
    if (fd_ < 0) return;
    if (streaming_) {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd_, VIDIOC_STREAMOFF, &type);
        streaming_ = false;
    }
    for (auto& b : buffers_) munmap(b.ptr, b.len);
    buffers_.clear();
    ::close(fd_);
    fd_ = -1;
}

void Camera::warm_up(double seconds) {
    // Auto-exposure needs a moment; early frames are dark and would look like motion.
    std::vector<uint8_t> tmp;
    int w, h;
    facet::i18n::Text err;
    int frames = std::max(1, int(seconds * 5));
    for (int i = 0; i < frames && is_open(); ++i)
        if (!grab(tmp, w, h, err)) break;
}

bool Camera::grab(std::vector<uint8_t>& out, int& w, int& h, facet::i18n::Text& error) {
    if (fd_ < 0) {
        error = "camera is not open";
        return false;
    }
    // 1) Return every stale frame to the driver so the next one is fresh.
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    while (xioctl(fd_, VIDIOC_DQBUF, &b) == 0) xioctl(fd_, VIDIOC_QBUF, &b);
    if (errno != EAGAIN) {
        error = {"read error: {}", {std::strerror(errno)}};
        return false;
    }
    // 2) Wait for a new frame.
    for (int attempt = 0; attempt < 2; ++attempt) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval tv{2, 0};
        int r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) {
            error = "camera delivers no frames";
            return false;
        }
        std::memset(&b, 0, sizeof b);
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd_, VIDIOC_DQBUF, &b) < 0) {
            if (errno == EAGAIN) continue;
            error = {"read error: {}", {std::strerror(errno)}};
            return false;
        }
        bool ok = b.index < buffers_.size() &&
                  extract(static_cast<const uint8_t*>(buffers_[b.index].ptr), b.bytesused, out);
        xioctl(fd_, VIDIOC_QBUF, &b);
        if (!ok) {
            error = "corrupt frame";
            return false;
        }
        w = width_;
        h = height_;
        return true;
    }
    error = "camera delivers no frames";
    return false;
}

bool Camera::extract(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    out.resize(size_t(width_) * size_t(height_));
    bool packed = fourcc_ == V4L2_PIX_FMT_YUYV || fourcc_ == V4L2_PIX_FMT_UYVY;
    size_t need = size_t(stride_) * size_t(height_ - 1) + size_t(packed ? width_ * 2 : width_);
    if (len < need) return false;
    int off = fourcc_ == V4L2_PIX_FMT_UYVY ? 1 : 0;
    for (int y = 0; y < height_; ++y) {
        const uint8_t* row = data + size_t(y) * size_t(stride_);
        uint8_t* dst = &out[size_t(y) * size_t(width_)];
        if (packed) {
            for (int x = 0; x < width_; ++x) dst[x] = row[x * 2 + off];
        } else {
            std::memcpy(dst, row, size_t(width_));  // GREY or the Y plane of NV12/YUV420
        }
    }
    return true;
}

}  // namespace dp
