# DONTFLOAT mini-DAW hosts

Minimal in-process plugin hosts ("mini-DAWs") for every plugin **format** and
**product kind**. Each host loads an audio file (default `tests/midi/test_1.wav`),
instantiates the DONTFLOAT plugin for its product, streams the whole file through
the plugin, writes the processed output to a WAV, and exercises the shared plugin
core session (prepare + analyze). These are headless command-line hosts.

## Targets

One executable per format × product (built when the matching format is enabled):

| Format | Full | Scratch | Pitcher |
|--------|------|---------|---------|
| CLAP | `mini_daw_clap_full` | `mini_daw_clap_scratch` | `mini_daw_clap_pitcher` |
| LV2  | `mini_daw_lv2_full`  | `mini_daw_lv2_scratch`  | `mini_daw_lv2_pitcher`  |
| VST3 | `mini_daw_vst3_full` | `mini_daw_vst3_scratch` | `mini_daw_vst3_pitcher` |

- **CLAP** hosts the real plugin via `clap_entry` → factory → `process()`.
- **LV2** hosts the real plugin via `lv2_descriptor` → `connect_port`/`run()`.
- **VST3** streams through the shared plugin **core session** (the same path the
  VST3 wrapper feeds). A full realtime VST3 module requires the proprietary
  Steinberg SDK (`DONTFLOAT_VST3_SDK_ROOT`); without it the DONTFLOAT VST3
  binary is not built, so the VST3 mini-DAW uses the core engine.

Disable with `-DDONTFLOAT_BUILD_MINI_DAW=OFF`.

## Usage

```bash
# load test_1.wav, stream through the plugin, print a summary (no display needed)
QT_QPA_PLATFORM=offscreen ./mini_daw_clap_full --seconds 4 --no-output

# write the processed output to a WAV
QT_QPA_PLATFORM=offscreen ./mini_daw_lv2_scratch \
    --input tests/midi/test_1.wav --output out.wav

# process the whole file through the Pitcher CLAP plugin
QT_QPA_PLATFORM=offscreen ./mini_daw_clap_pitcher --full --no-output
```

### Options

| Flag | Default | Meaning |
|------|---------|---------|
| `--input, -i <path>` | `tests/midi/test_1.wav` | audio file to load |
| `--output, -o <path>` | derived | processed output WAV path |
| `--block <n>` | `512` | host processing block size (frames) |
| `--seconds <s>` | `8` | cap processed length (`--full` = whole file) |
| `--no-output` | write | skip writing the output WAV |

> Note: these hosts need a Qt platform plugin; run headless with
> `QT_QPA_PLATFORM=offscreen`.

## Tests

Each CLAP/LV2/VST3 × product mini-DAW is registered as a CTest that runs headless
on `tests/midi/test_1.wav` (labels `plugins;mini-daw;<format>;<kind>`).
