# Fuurin Audio Toolkit

Fuurin Audio Toolkit is a collection of audio components for building voice applications
with Zephyr. It brings together **wake word detection, voice activity detection (VAD),
and acoustic echo cancellation (AEC)** in one reusable module.

## Overview

Use Fuurin to wake a device with a spoken phrase, detect when someone starts or stops
speaking, and reduce speaker echo in microphone audio. These components can be used
individually or together in applications such as voice assistants, intercoms, and
voice-controlled devices.

| Component | Supported backends |
|---|---|
| Wake word detection | microWakeWord with TensorFlow Lite Micro |
| Voice activity detection | libfvad |
| Acoustic echo cancellation | WebRTC AECM, SpeexDSP |

The toolkit also provides a PCM processing pipeline and playback reference history
for AEC. Your application supplies the audio and decides what to do with the results,
such as starting a conversation after a wake event or ending it after silence.

## Getting Started

You need a Zephyr application with a microphone PCM source. For wake word detection,
bring a compatible microWakeWord model. For AEC, also supply the audio sent to your
speaker so the toolkit can use it as an echo reference.

Start with the feature you want to add:

- **Detect speech:** Run the [VAD example](samples/detector/README.md#vad-example).
  It needs no model or TensorFlow Lite Micro.
- **Add a wake phrase:** Follow the [wake word example](samples/detector/README.md#wake-word-example)
  to load your model and switch to VAD after detection.
- **Reduce speaker echo:** Follow the [AEC integration guide](docs/pipeline.md#zephyr-integration)
  to process microphone audio alongside a playback reference.

See [Installation](docs/detector.md#integration) to add the module and its dependencies
to an existing project. Components are selected through Kconfig, so you can build
with just VAD, just AEC, or wake detection with VAD and AEC.

## Platform and Audio Support

Fuurin integrates with Zephyr through CMake and Kconfig and exposes C APIs. It works
with application-provided PCM rather than a particular microphone or codec driver.
The voice application has been tested on Raspberry Pi Pico 2 W; other boards need
their own audio integration and performance checks.

| Audio interface | Input format |
|---|---|
| Wake word / VAD detector | 16 kHz, mono, signed 16-bit PCM |
| AEC pipeline | 8 or 16 kHz, mono, signed 16-bit PCM, 20 ms frames |

Wake word models are supplied separately. Memory requirements depend on the selected
components and model; see [Models and memory](docs/detector.md#models-and-memory)
when sizing your application.

## Examples and Documentation

- [Detector example](samples/detector/README.md) — build the sample, connect microphone audio, and handle events.
- [Detector API](docs/detector.md#api) — initialization, callbacks, and wake/VAD mode switching.
- [Audio pipeline](docs/pipeline.md) — AEC processing and playback reference timing.
- [AECM configuration](docs/aecm-port.md) — echo suppression settings and backend details.
- [Component configuration](docs/detector.md#component-selection) — choose which features to include.
- [Tests](tests/README.md) — detector regression tests and [optional component tests](tests/configurations/README.md).

## License

Fuurin Audio Toolkit is licensed under [Apache-2.0](LICENSE).
Dependencies retain their [upstream licenses](THIRD_PARTY.md).
Application-supplied models are covered by their own licenses.
