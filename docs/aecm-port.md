# WebRTC AECM port

The toolkit supports the cpuimage WebRTC_AECM backend, based on the legacy mobile
AECM algorithm. It does not implement WebRTC AEC3.

## Select and build

The dependency is pinned in the module manifest to
`4a3647cde7509a1ab08a69c198de572b8897d162`:

```sh
west update webrtc-aecm
```

Enable the module and pipeline, then select AECM in the application `prj.conf`:

```ini
CONFIG_FUURIN_AUDIO=y
CONFIG_FUURIN_AUDIO_AFE=y
CONFIG_FUURIN_AUDIO_AEC_BACKEND_AECM=y
```

Additional optional settings (their current defaults):

```ini
CONFIG_FUURIN_AUDIO_AECM_ECHO_MODE=3
CONFIG_FUURIN_AUDIO_AECM_CNG=n
```

For Speex, replace the AECM selection with
`CONFIG_FUURIN_AUDIO_AEC_BACKEND_SPEEXDSP=y`. For a bypass comparison keep the chosen
backend and set `zat_config.aec_enabled=false` at runtime.
The Speex tail-length setting has no effect on AECM.

For host compilation, without running or adding acoustic tests:

```sh
cmake -S fuurin-audio-toolkit -B /tmp/fuurin-aecm-port \
  -DZAT_AEC_BACKEND=AECM -DBUILD_TESTING=OFF
cmake --build /tmp/fuurin-aecm-port
```

The existing host test suite is Speex-specific and is not registered for AECM.
Successful AECM compilation does not imply those tests ran or passed.

## Data flow

Each 20 ms call is split into two 10 ms calls: 80 samples at 8 kHz, or 160 at
16 kHz. For each block the adapter calls `BufferFarend`, then `Process`, with
`nearendClean=NULL` (no external noise suppressor) and `msInSndCardBuf=0` because
the caller has already aligned render PCM to capture time. A board reference
delay is applied once by the render history, not again in AECM.

AECM still runs its own startup buffering and acoustic delay estimator. Startup
passes microphone audio through before cancellation becomes active; resetting on
a capture discontinuity restarts that warm-up. Board timing and actual acoustic
alignment still need validation. Always supply the actual playback PCM, including
silence blocks, and feed all successful output frames to wake word independently
of the VAD decision.

Higher echo modes increase suppression; they are not evidence of better
recognition or double-talk preservation. Comfort noise is off by default to avoid
adding synthesized background sound ahead of VAD/wake word.

## Source adaptations

`cmake/aecm.cmake` generates an isolated copy from the pinned dependency in the
build tree. It never modifies the upstream checkout.

- Prefix all `WebRtc...` identifiers to prevent symbol collisions with libfvad's
  separate WebRTC signal-processing functions.
- Build the scalar C/C++ implementation, excluding NEON and MIPS source files.
- Replace the sole `<algorithm>` / `std::any_of` use with an equivalent small
  iterator helper. Build without C++ exceptions or RTTI, using minimal Zephyr C++.
- Check the top-level and core `calloc` results before dereferencing them.
- Initialize scalar function pointers once during serialized creation so a
  reset does not rewrite them while another instance is processing.
- Route allocations through a private adapter with per-instance accounting and
  aligned allocation headers. Failed realloc preserves the original allocation;
  the toolkit does not expose upstream resizing APIs. The exposed initialization
  path allocates those buffers initially from NULL.

Backend errors propagate as negative errno values and increment toolkit error
counts. The application avoids publishing unsuccessful output frames. Allocation
and destruction remain control-thread operations; each pipeline has one audio
worker. No backend processing or reset allocates heap.

Pipeline statistics report allocation bytes, processing counts, and PCM levels.
Allocation bytes include adapter headers but not the libc allocator's own overhead.
Inspect runtime allocation statistics when comparing AECM with Speex; linked static
RAM alone omits both backends' dynamically allocated state.
