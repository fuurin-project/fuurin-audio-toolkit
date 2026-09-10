# Optional component configurations

These tests register only fuurin-audio-toolkit, without a TFLite Micro checkout
in ZEPHYR_EXTRA_MODULES. They exercise eight init/process/reset/deinit cycles
and require libc heap usage to return to the baseline after each cycle.

From the workspace root:

```sh
west build -b mps2/an521/cpu0 fuurin-audio-toolkit/tests/configurations -d /tmp/fuurin-vad-only
west build -b mps2/an521/cpu0 fuurin-audio-toolkit/tests/configurations -d /tmp/fuurin-aec-only -- -DEXTRA_CONF_FILE=aecm.conf
west build -b mps2/an521/cpu0 fuurin-audio-toolkit/tests/configurations -d /tmp/fuurin-bypass-only -- -DEXTRA_CONF_FILE=bypass.conf
west build -d /tmp/fuurin-vad-only -t run
west build -d /tmp/fuurin-aec-only -t run
west build -d /tmp/fuurin-bypass-only -t run
```

AECM tests validate lifecycle, processing and allocation cleanup on silence, not acoustic
performance. Host `tests/pipeline` covers Speex synthetic echo attenuation,
reference timing, bypass, VAD, allocation failure and disabled frame VAD.
A timeout stops QEMU after PASS, so its final termination message is expected.
