# Third-party sources

Dependencies are fetched separately and are not copied into this repository.
Preserve their notices when distributing firmware or source packages.

| Component | Pinned revision | Notices |
|---|---|---|
| [TFLite Micro](https://github.com/zephyrproject-rtos/tflite-micro) | `fcc760af130f3a595b5802cdebcc77461e54f382` | Apache-2.0 and bundled third-party notices, including the microfrontend FFT |
| [libfvad](https://github.com/dpirch/libfvad) | `532ab666c20d3cfda38bca63abbb0f152706c369` | `LICENSE`, `PATENTS`, `AUTHORS` in the upstream repository |
| [SpeexDSP 1.2.1](https://github.com/xiph/speexdsp) | `1b28a0f61bc31162979e1f26f3981fc3637095c8` | `COPYING` and notices in individual source files, including KISS FFT |
| [cpuimage WebRTC_AECM](https://github.com/cpuimage/WebRTC_AECM) | `4a3647cde7509a1ab08a69c198de572b8897d162` | `LICENSE` (BSD-3-Clause) and notices in individual source files |

AECM source files reference upstream WebRTC AUTHORS/PATENTS files that are not
included in this standalone mirror. This port preserves those references; it does
not invent or replace their contents. Source adaptations are recorded in
`cmake/aecm.cmake` and generated only in the build directory.

The Fuurin wake-word model remains in the consuming `pico-clip` application;
its weights and training assets are not included in this toolkit.
