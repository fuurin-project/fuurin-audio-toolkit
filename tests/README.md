# Standalone module regression tests

Requires Zephyr, this module, TFLite Micro and libfvad; does not link Pico Clip,
its drivers, networking, or shared memory.

The external test fixture is Hey Pico v2 (61,296 bytes), SHA-256:
`e91ee75fdca64f4e59e45fdc07d812691fa934bbfdc57e5dca8c8b04395f2212`.
The model is not shipped in this module. Pass its absolute path:

```sh
west build -b mps2/an521/cpu0 /path/to/fuurin-audio-toolkit/tests \
  -d /tmp/fuurin-module-test -- \
  '-DZEPHYR_EXTRA_MODULES=/path/to/fuurin-audio-toolkit;/path/to/tflite-micro' \
  -DFUURIN_LIBFVAD_DIR=/path/to/libfvad \
  -DFUURIN_TEST_MODEL=/path/to/hey_pico.tflite
west build -d /tmp/fuurin-module-test -t run
```

Checks through the public C API:

- 400 golden raw/smoothed scores from deterministic noise (the prior engine's
  results were also verified against the original Python runtime).
- Identical predictions after reset and after WAKE → VAD → WAKE.
- VAD start/end events compared with independent libfvad decisions on the same
  waveform, including arbitrary PCM chunk boundaries and silence hangover.
- No wake scores while in VAD mode; speech events only in VAD mode.
- Startup suppression, moving mean, detection threshold and cooldown.
- Deinit/reinit, invalid/misaligned model input, and VAD-only initialization.
- Callback re-entry rejected with -EBUSY; processing before init rejected.

Successful run ends with `PASS: 4x400 golden wake scores ...`.
QEMU validates functionality, not RP2350 timing or hardware capture.

Heap-release coverage: VAD releases frontend allocations, repeated WAKE/VAD
cycles restore the same allocated byte count, deinit restores the baseline,
and forced allocation failure leaves VAD usable and allows WAKE to be retried.
