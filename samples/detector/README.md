# Detector example

This sample shows how to feed PCM to the detector, receive speech and wake events,
and switch between wake detection and VAD.

It starts with synthetic silence so you can check the build and initialization
without microphone hardware. To detect actual speech, connect your audio source as
described below.

## Before you start

Use a configured Zephyr workspace with the toolkit and libfvad source checkouts.
See [Installation](../../docs/detector.md#integration) for dependency setup.
The commands below use Zephyr's `mps2/an521/cpu0` QEMU target. Replace the absolute
paths with your checkout locations.

## VAD example

Build without TensorFlow Lite Micro or a wake model:

```sh
west build -b mps2/an521/cpu0 /path/to/fuurin-audio-toolkit/samples/detector \
  -d /tmp/fuurin-vad-sample -- \
  -DZEPHYR_EXTRA_MODULES=/path/to/fuurin-audio-toolkit \
  -DFUURIN_LIBFVAD_DIR=/path/to/libfvad \
  -DEXTRA_CONF_FILE=vad_only.conf
west build -d /tmp/fuurin-vad-sample -t run
```

The console prints:

```text
Fuurin detector ready; this sample feeds silence, not a microphone
```

With silence input, no speech events are expected. Stop QEMU with Ctrl+A, then X.

## Wake word example

Add a TFLite Micro checkout and a compatible model:

```sh
west build -b mps2/an521/cpu0 /path/to/fuurin-audio-toolkit/samples/detector \
  -d /tmp/fuurin-wake-sample -- \
  '-DZEPHYR_EXTRA_MODULES=/path/to/fuurin-audio-toolkit;/path/to/tflite-micro' \
  -DFUURIN_LIBFVAD_DIR=/path/to/libfvad \
  -DFUURIN_SAMPLE_MODEL=/path/to/model.tflite
west build -d /tmp/fuurin-wake-sample -t run
```

In [src/main.c](src/main.c), set `wake_threshold` and `warmup_feature_frames` for
your model. The existing values are examples for Hey Pico v2. Check the
[model requirements](../../docs/detector.md#models-and-memory) before substituting a model.

Once connected to microphone audio, the sample prints `Wake detected` when the
phrase is detected, switches to VAD, and prints `Speech started` / `Speech ended`.
After speech ends, it returns to wake detection and restarts warmup.

## Connect your microphone

Build for your board and replace the zero-filled `pcm` buffer in `src/main.c`
with captured **16 kHz, mono, signed 16-bit PCM**. If your application uses AEC,
feed the processed microphone audio to the detector.

Call `fuurin_audio_process()` from one worker thread for each audio chunk. The
sample uses 160 samples per call, but the detector accepts other chunk sizes.
Pace processing from audio arrival rather than the sample's synthetic 10 ms sleep.

Callbacks run during processing. The sample records wake and speech-end events in
flags, then changes modes after `fuurin_audio_process()` returns. Use the same pattern
to queue application work such as starting a connection.

For error handling, reset behavior, and lifecycle rules, see the
[Detector API](../../docs/detector.md#api).
