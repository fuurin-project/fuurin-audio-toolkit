# Integration and detector reference

[Back to README](../README.md)

## Integration

Add the module to the consuming project's `west.yml`, replacing the placeholders
with the repository URL and a pinned commit:

```yaml
manifest:
  projects:
    - name: fuurin-audio-toolkit
      url: <your-repository-url>
      revision: <pinned-commit>
      path: modules/audio/fuurin-audio-toolkit
      import: true
```

The module's `west.yml` pins libfvad, TFLite Micro, SpeexDSP, and WebRTC AECM revisions.
Run `west update fuurin-audio-toolkit libfvad tflite-micro` to fetch the detector sources.
Fetch the selected AEC backend dependencies as well when enabling the pipeline.
Zephyr's optional group may exclude TFLite Micro from default updates; updating it
explicitly by project name resolves this. If the parent manifest defines dependencies
with the same names, verify that the effective revisions are compatible.

For a local checkout, register the modules **before `find_package(Zephyr)`**:

```cmake
list(APPEND ZEPHYR_EXTRA_MODULES
  /path/to/fuurin-audio-toolkit
  /path/to/tflite-micro)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

libfvad defaults to `${WEST_TOPDIR}/modules/lib/libfvad`. Override it with
`-DFUURIN_LIBFVAD_DIR=/path/to/libfvad`. An existing CMake target named `fvad` is
reused instead of compiled again. Consumers do not need a separate AEC toolkit or Pico Clip.

Example `prj.conf`:

```ini
CONFIG_FUURIN_AUDIO=y
CONFIG_STD_CPP17=y
CONFIG_SPEED_OPTIMIZATIONS=y
CONFIG_MAIN_STACK_SIZE=16384
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=32768
```

This is a starting configuration for the sample. Applications can use a dedicated
worker thread and adjust stack/heap sizes. Enable `CONFIG_FPU=y` on Cortex-M hardware
with an available FPU. CMSIS-NN is enabled by default on Cortex-M.

## API

```c
#include <fuurin_audio/detector.h>

struct fuurin_audio_config config;
fuurin_audio_config_defaults(&config);
config.model = aligned_model_bytes;  /* 16-byte aligned, kept alive */
config.model_size = model_byte_count;
config.wake_threshold = 0.37265625f;  /* example: Hey Pico v2 */
config.warmup_feature_frames = 216;  /* set from your model's context */
config.callback = on_event;
int ret = fuurin_audio_init(&config);
/* Check ret before proceeding. */
ret = fuurin_audio_process(pcm, sample_count, k_uptime_get());
```

`process()` accepts arbitrary PCM chunks; preprocessing and VAD retain incomplete
frames. `now_ms` must be nonnegative and monotonically nondecreasing; equal timestamps
are allowed. It controls wake cooldown. VAD speech/silence durations are measured
from consumed audio samples, independently of call intervals.

Each new speech segment emits `SPEECH_STARTED`. Continuous silence lasting
`speech_end_ms` emits `SPEECH_ENDED`; events are not repeated every 10 ms.
VAD events do not provide probabilities.

Callbacks run synchronously inside `process()` and should be brief. **Do not call
mode switching, reset, init, deinit, or process from a callback**; those calls return
`-EBUSY`. Record or queue the event, then act after `process()` returns.

```c
/* After a wake event and application-controlled connection setup: */
fuurin_audio_set_mode(FUURIN_AUDIO_VAD);
/* After the application ends its conversation: */
fuurin_audio_set_mode(FUURIN_AUDIO_WAKE);
```

Mode changes clear model state, frontend state, VAD state, and detection history.
Returning to WAKE restarts warmup. Calling `set_mode()` with the current mode does
not reset it. Call `fuurin_audio_reset()` after audio loss.

Reset and mode changes do not emit `SPEECH_ENDED` for the previous segment; the
application must clear its own session state. `fuurin_audio_deinit()` releases
frontend and libfvad heap allocations and allows subsequent initialization.
With `model=NULL, model_size=0`, only VAD is available; switching to WAKE returns `-ENOTSUP`.

## Models and memory

The module contains no model weights and does not assume a particular wake phrase.
It supports a microWakeWord-compatible 40-bin microfrontend with a 30 ms window,
10 ms hop, noise reduction, PCAN, and log scaling. Models must have an int8 input
of shape `[1, stride, 40]` and a single uint8 output. Internal streaming state and
operators must be supported by the current TFLM version. Models trained with other
frontend settings cannot be substituted directly.

The application supplies the threshold, smoothing length, and warmup length.
Input/output quantization scales and zero points are read from the model.

The default internal tensor arena is 32 KiB, with a separate 2 KiB variable allocator.
The frontend and libfvad use the libc heap. `fuurin_audio_arena_used()` reports tensor
arena usage, not total RAM consumption.

Applications can supply `tensor_arena` and `tensor_arena_size` to control placement.
Set `CONFIG_FUURIN_AUDIO_INTERNAL_ARENA_SIZE=0` to remove the internal arena.
Caller-owned model data and arenas must be 16-byte aligned and remain valid until deinit.

Switching to VAD destroys the wake interpreter and releases microfrontend libc heap
allocations. Returning to WAKE rebuilds them and restarts warmup. Model data and the
caller-owned tensor arena must remain valid; the fixed arena is not returned to the
libc heap. If reconstruction fails, the call returns `-ENOMEM` and remains in VAD.
Retry when sufficient memory is available.

## Sample and tests

`samples/detector` demonstrates the C API using silence input; **it does not capture
hardware audio**. Without `FUURIN_SAMPLE_MODEL`, it runs VAD only. With a model,
it demonstrates switching to VAD after a wake event. Replace the sample's PCM source
to connect a microphone.

```sh
west build -b mps2/an521/cpu0 /path/to/fuurin-audio-toolkit/samples/detector \
  -d /tmp/fuurin-sample -- \
  '-DZEPHYR_EXTRA_MODULES=/path/to/fuurin-audio-toolkit;/path/to/tflite-micro' \
  -DFUURIN_LIBFVAD_DIR=/path/to/libfvad
```

Regression tests in `tests/` use an externally supplied Hey Pico fixture. They check
400 scores against a golden reference, full state reset, mode
isolation, real libfvad events, lifecycle behavior, and callback guards.
See [regression tests](../tests/README.md) for commands and fixture hashes.

## License

The module code is licensed under Apache-2.0; see `LICENSE`. Dependencies retain their
upstream licenses and copyright notices; see `THIRD_PARTY.md`. TFLite Micro, CMSIS-NN,
and libfvad are linked from their dependency checkouts. Application-supplied model
weights are subject to their own licenses.

## Component selection

| Use case | Settings |
|---|---|
| Wake word + VAD | `FUURIN_AUDIO=y` (detector and wake word enabled by default) |
| VAD events without TFLite | `FUURIN_AUDIO=y`, `FUURIN_AUDIO_WAKEWORD=n` |
| AEC pipeline only | `FUURIN_AUDIO=y`, `FUURIN_AUDIO_DETECTOR=n`, `FUURIN_AUDIO_AFE=y` |
| Frame VAD / PCM bypass pipeline without AEC | Previous row plus `FUURIN_AUDIO_AEC=n` |

These are Kconfig names; add the `CONFIG_` prefix in `prj.conf`. AEC-only and VAD-only
builds do not require registering the TFLite Micro module. Select AECM with
`CONFIG_FUURIN_AUDIO_AEC_BACKEND_AECM=y`, or select SpeexDSP with
`CONFIG_FUURIN_AUDIO_AEC_BACKEND_SPEEXDSP=y`.

- `<fuurin_audio/detector.h>`: wake detection and VAD event API.
- `<fuurin_audio/audio.h>`: AEC / frame VAD pipeline accepting 8/16 kHz, 20 ms PCM.
- `<fuurin_audio/render_history.h>`: playback reference history.

The preprocessing API uses the `zat_*` function prefix. When the detector owns VAD,
set `zat_config.vad_disabled=true` to skip pipeline libfvad allocation and classification.
The `speech` output is then always false. The default value, false, preserves frame VAD.
Multiple pipeline instances are supported; the singleton restriction applies only to the detector.

Host AEC/VAD tests:

```sh
cmake -S fuurin-audio-toolkit -B /tmp/fuurin-afe-host
cmake --build /tmp/fuurin-afe-host -j4
ctest --test-dir /tmp/fuurin-afe-host --output-on-failure
```

See [the pipeline guide](pipeline.md) for the AEC API and reference timing contract, and
[configuration tests](../tests/configurations/) for configurations without TFLite.
