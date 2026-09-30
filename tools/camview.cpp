// Dev tool: live grayscale camera view on the framebuffer, full screen.
//   camview [camera-id] [/dev/fb0]
// Auto-levels each frame so dim IR images stay visible. Ctrl+C / SIGTERM exits.
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "camera.h"

static volatile sig_atomic_t g_quit = 0;
static void on_signal(int) { g_quit = 1; }

int main(int argc, char** argv) {
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    std::string id;
    if (argc > 1) {
        id = argv[1];
    } else {
        auto cams = dp::list_cameras();
        if (cams.empty()) {
            std::fprintf(stderr, "no cameras\n");
            return 1;
        }
        id = cams.back().id;  // IR is usually the last node of a composite webcam
    }
    const char* fb_path = argc > 2 ? argv[2] : "/dev/fb0";

    int fb = open(fb_path, O_RDWR);
    fb_var_screeninfo var{};
    fb_fix_screeninfo fix{};
    if (fb < 0 || ioctl(fb, FBIOGET_VSCREENINFO, &var) || ioctl(fb, FBIOGET_FSCREENINFO, &fix) ||
        var.bits_per_pixel != 32) {
        std::fprintf(stderr, "framebuffer %s unusable (need 32bpp)\n", fb_path);
        return 1;
    }
    size_t size = size_t(fix.line_length) * var.yres;
    auto* map = static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0));
    if (map == MAP_FAILED) return 1;
    int W = int(var.xres), H = int(var.yres);

    dp::Camera cam;
    facet::i18n::Text err;  // printed untranslated (English)
    if (!cam.open(id, err)) {
        std::fprintf(stderr, "%s: %s\n", id.c_str(), facet::i18n::format(err.key, err.args).c_str());
        return 1;
    }
    std::printf("showing %s (%s) on %dx%d\n", cam.name().c_str(), id.c_str(), W, H);
    std::fflush(stdout);

    std::memset(map, 0, size);
    std::vector<uint8_t> frame;
    std::vector<int> xmap;
    while (!g_quit) {
        int w = 0, h = 0;
        if (!cam.grab(frame, w, h, err)) {
            std::fprintf(stderr, "grab: %s\n", facet::i18n::format(err.key, err.args).c_str());
            break;
        }
        // Auto-levels: stretch the 1st..99th percentile to full range.
        int hist[256] = {};
        for (uint8_t v : frame) ++hist[v];
        size_t n = frame.size(), acc = 0;
        int lo = 0, hi = 255;
        for (int i = 0; i < 256; ++i) {
            acc += size_t(hist[i]);
            if (acc < n / 100) lo = i;
            if (acc < n - n / 100) hi = i;
        }
        hi = std::max(hi, lo + 16);
        uint8_t lut[256];
        for (int i = 0; i < 256; ++i) lut[i] = uint8_t(std::clamp((i - lo) * 255 / (hi - lo), 0, 255));

        // Fit keeping aspect, centred, nearest-neighbour.
        float s = std::min(float(W) / w, float(H) / h);
        int dw = int(w * s), dh = int(h * s), ox = (W - dw) / 2, oy = (H - dh) / 2;
        xmap.resize(size_t(dw));
        for (int x = 0; x < dw; ++x) xmap[size_t(x)] = std::min(w - 1, int(x / s));
        for (int y = 0; y < dh; ++y) {
            const uint8_t* src = &frame[size_t(std::min(h - 1, int(y / s))) * size_t(w)];
            auto* dst = reinterpret_cast<uint32_t*>(map + size_t(oy + y) * fix.line_length) + ox;
            for (int x = 0; x < dw; ++x) {
                uint32_t v = lut[src[xmap[size_t(x)]]];
                dst[x] = 0xFF000000u | v << 16 | v << 8 | v;
            }
        }
    }
    std::memset(map, 0, size);
    return 0;
}
