# fuurin-audio-toolkit

Reusable mono PCM audio front end for Zephyr, with a portable C core:

```text
microphone PCM ──→ AEC ──┬──→ VAD decision → application endpoint/turn logic
                    ↑   ├──→ continuous PCM → wake-word model
actual playback PCM ┘   └──→ PCM → application encoder/uplink
```

AEC is selectable between SpeexDSP 1.2.1 fixed-point AEC and the cpuimage
WebRTC AECM port; VAD uses libfvad.
See [AECM port notes](aecm-port.md) for settings, source adaptations and limits.
The application supplies its own wake word model. VAD never gates the model's input.
No microphone pre-roll, NS, AGC, semantic turn detection, or barge-in controller
is included. The render history stores speaker reference PCM for AEC only.

## Audio contract

- Signed 16-bit mono PCM at 8 or 16 kHz.
- Each `zat_process()` call consumes exactly 20 ms: 160 or 320 samples.
- AEC internally processes two 10 ms blocks per call to reduce working RAM.
- Both inputs and output are separate, non-overlapping buffers.
- The render reference is the PCM actually sent to the DAC, after any software
  gain/mixing. Supply zeros when nothing is playing; NULL is an error with AEC on.
- Capture and render must share a sample clock or be compensated for clock drift.
  Align their timelines and account for codec, FIFO, DMA and acoustic delay.
- Feed every output frame to wake word, whether or not VAD reports speech.
- Reset after discontinuities or an audio-format restart. Reset preserves
  lifetime diagnostics but clears AEC adaptation and VAD state.

## Dependencies

Keep these sibling directories in the workspace:

```text
workspace/
  fuurin-audio-toolkit/
  pico-clip/                  # optional example integration
  modules/lib/libfvad/
  modules/lib/speexdsp/       # Speex backend
  modules/lib/webrtc-aecm/    # AECM backend
```

Exact dependency commits are pinned in [west.yml](../west.yml). In the current
Pico workspace, `west update libfvad speexdsp webrtc-aecm` fetches the pinned dependencies. In a new toolkit-only
workspace, use `west init -l fuurin-audio-toolkit` followed by `west update`.
The toolkit manifest supplies DSP dependencies, not Zephyr or a board application;
an application manifest should supply its own Zephyr version.

Source locations can also be provided using the CMake cache variables
`ZAT_LIBFVAD_DIR`, `ZAT_SPEEXDSP_DIR` and `ZAT_AECM_DIR`. Dependency sources are
not modified; AECM adaptations are generated in the build directory.

## Zephyr integration

Register the local module before `find_package(Zephyr ...)`:

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../fuurin-audio-toolkit)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_audio_app)
target_link_libraries(app PRIVATE fuurin_audio_toolkit)
```

Enable `CONFIG_FUURIN_AUDIO=y` and `CONFIG_FUURIN_AUDIO_AFE=y` in the application configuration.
For a pipeline-only build, set `CONFIG_FUURIN_AUDIO_DETECTOR=n`.
Select `CONFIG_FUURIN_AUDIO_AEC_BACKEND_AECM=y` or
`CONFIG_FUURIN_AUDIO_AEC_BACKEND_SPEEXDSP=y`. Only the chosen backend
is linked. AECM selects Zephyr's minimal C++ support; no full C++ standard library
is required. The
application must provide sufficient libc heap for the DSP state as well as its
other users. Allocate before launching the audio worker; do not create/destroy a
pipeline in an interrupt. A pipeline has one owner; processing/reset/stats reads
must be serialized by the application. Independent pipelines may process in
parallel. Concurrent creation attempts may return NULL; create them serially.

```c
#include <fuurin_audio/audio.h>

struct zat_config cfg = {
    .sample_rate = 16000,
    .echo_tail_ms = 80,
    .vad_mode = 2,
    .aec_enabled = true,
};
struct zat_pipeline *afe = zat_create(&cfg);
/* Check afe for NULL before starting capture. */

/* In the audio worker, with three distinct 320-sample buffers: */
bool speech;
int rc = zat_process(afe, mic_pcm, aligned_dac_pcm, clean_pcm, 320, &speech);
if (rc == 0) {
    /* Always enqueue clean_pcm for wake word.
     * Publish speech to the application's endpoint state machine.
     * Encode clean_pcm when the application enables uplink. */
}
```

The Speex allocator adapter handles partial initialization failures without
dereferencing NULL and releases all allocations. Processing allocates no heap.
Only the selected AEC backend is linked. Backend allocator contexts are private
to the toolkit: do not call upstream initialization, resizing or destruction APIs
directly. The AECM adapter tracks allocations and handles its public create failure
paths. Processing and reset do not allocate heap.

When a separate detector owns VAD, set `zat_config.vad_disabled=true` to skip
frame VAD allocation and classification. The pipeline then returns `speech=false`.

## Verification and limits

The project supports host checks independently of Zephyr:

```sh
cmake -S fuurin-audio-toolkit -B /tmp/zat-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/zat-build
ctest --test-dir /tmp/zat-build --output-on-failure
```

Host tests cover silence, bypass, clipping statistics, invalid arguments,
reset, injected Speex initialization allocation failures, render gaps/delay/
timestamp rollover, and synthetic delayed linear echo at 8/16 kHz. Synthetic
signals do not establish hardware acoustic performance or speech quality.
AECM lifecycle and allocation cleanup tests are in `../tests/configurations/`.

Before enabling automatic barge-in, validate real playback-only false triggers,
near-end speech during playback, latency, dropped frames, clipping, wake-word
accuracy after AEC, and network stability. Barge-in additionally requires stopping
playback, clearing queued audio and cancelling the old response. VAD supplies
speech activity, not semantic completion of an utterance.

`src/aec.c` (Speex) and `src/aec_aecm.c` (AECM) implement backend initialization,
processing, reset and destruction behind the same pipeline API. AECM startup
initially passes input through while its buffers stabilize, so do not assume echo
is removed immediately after create/reset.

## License

Toolkit code: Apache-2.0, see [LICENSE](../LICENSE).
Dependencies retain their own licenses: libfvad BSD-style license and WebRTC
patent grant; SpeexDSP BSD-style license, including the KISS FFT source notices;
WebRTC_AECM BSD-3-Clause with its retained source-file notices.
See [THIRD_PARTY.md](../THIRD_PARTY.md).
