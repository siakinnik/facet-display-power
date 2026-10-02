# display-power — Facet plugin

Turns the panel's screen on and off by schedule, touches and a person in front
of the camera. Runs as an out-of-process plugin of
[facet-core](../facet-core/docs/ARCHITECTURE.md).

## Behaviour

The day is split into **day** and **night** (configurable boundaries). Each
period has a mode:

| Mode | Screen |
|---|---|
| Always on | on 24/7 |
| Camera | on while the camera sees motion, plus "Keep on after leaving" |
| Off | off, e.g. at night |

On top of the mode:

- a touch wakes the screen for "Screen after a touch";
- "Manage the screen" off keeps the screen on;
- camera unavailable keeps the screen on (fail-safe);
- if the plugin crashes, the core turns the screen on.

Defaults: always on during the day (07:00–23:00), camera at night.

## Camera

- Found through V4L2 (`/dev/video*`) and remembered by its stable
  `/dev/v4l/by-path/…` path, which survives reboots while the camera stays in
  the same port. `by-id` is not used: webcams with an IR sensor expose both
  sensors under the same by-id name.
- The camera is opened **only** during "Camera" periods; otherwise it is closed.
  While open it is reserved (its stream buffers are allocated, so other
  programs get "device busy"), but with a check interval of 5 s or more the
  sensor is switched on only for each check (about a second) and off in
  between: it stays cool and frees USB bandwidth for the camera's other sensor.
- Once per interval (2 s by default) one fresh 320×240 frame is taken. Only
  brightness is used: it is scaled down to 80×60 and compared with the previous
  frame with auto-exposure compensation. Frames are never stored.
- Needs an uncompressed format (YUYV, GREY, NV12, YUV420); MJPEG-only cameras
  are not supported yet.
- The detector sees **motion**. Someone sitting still is covered by
  "Keep on after leaving". `PresenceDetector` can be swapped for a real person
  detector later.
- IR cameras with a pulsing emitter (Windows Hello style) produce banding and
  false motion; prefer the RGB camera for now.

Tools for setting up a device:

```bash
display-power --probe [seconds] [camera-id]   # list cameras, print live motion levels

# Full-screen live view on /dev/fb0 (dev tool, not installed); stop Facet first.
cmake --build build --target camview && build/camview [camera-id]
```

## Install

On a device that already runs [Facet](https://github.com/siakinnik/facet-core)
(prebuilt static binaries for x86_64, aarch64 and armv7):

```bash
curl -fsSL https://raw.githubusercontent.com/siakinnik/facet-core/main/scripts/get.sh | sudo bash -s -- --plugin siakinnik/facet-display-power
```

Run the same command to update. Remove with `… | sudo bash -s -- --remove-plugin display-power`.
Settings are kept in `/var/lib/facet/data/display-power/settings.json`.

The plugin has no home-screen tile: it is under Settings > Modules > Screen &
camera. It needs Facet 0.3 (API 2) and asks for two permissions, granted in
Settings > Apps: `display.power` (switch the screen) and `camera`.

## Build from source

```bash
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build
FACET_PLUGIN_PATH=$PWD/build ../facet-core/build/facet   # run with a locally built core
```

The plugin SDK comes from facet-core: a checkout in `../facet-core` (or
`-DFACET_CORE_DIR=…`) is used when present, otherwise CMake downloads it at
`FACET_CORE_REF` (default: the matching core release tag). facet-core's
`scripts/install.sh` also builds this plugin when it sits next to `facet-core`.
`build/display-power.plugin/` is a ready plugin directory.

## Releases

- **CI** runs on every push: translation check, version check
  (`CMakeLists.txt` vs `manifest.json`), build, unit tests, static build and a
  protocol smoke test (a scripted core session: hello, ping, shutdown).
- **Release**: bump the version in `CMakeLists.txt` (`project(VERSION)`,
  `DP_VERSION_SUFFIX`) and `manifest.json`, push, then Actions → Release →
  *Run workflow*. It tags `v<version>`, builds static binaries for x86_64,
  aarch64 and armv7 (ARM tested under QEMU) and publishes the release with
  `SHA256SUMS`. Archives are named `facet-display-power-<version>-linux-<arch>.tar.gz`
  and contain the plugin directory, which is what `get.sh --plugin` expects.

## Translations

Strings in the code are English and serve as keys; translations are in
`src/i18n/` (`ru.cpp`). The language follows the core at runtime.

## Files

```
src/policy.*       settings and the pure on/off decision (unit-tested)
src/presence.*     presence detector
src/camera*        V4L2 discovery and luma capture
src/watcher.*      camera thread (never blocks the plugin loop)
src/i18n/          translations
src/main.cpp       plugin glue: events, settings screen, tile subtitle
tools/camview.cpp  dev tool: live camera view on the framebuffer
tests/             unit tests
```
