# AGENTS.md

## Cursor Cloud specific instructions

DONTFLOAT is a **C++17 / Qt 6.8+ desktop audio application** (BPM / beat-grid editor) built
with **CMake + Ninja**. There are no web servers, databases, or containers — a single native
binary plus optional DAW plugins (CLAP/LV2/VST3). See `README.md` and `tests/README.md` for the
canonical build/test docs; only the non-obvious, environment-specific caveats are listed here.

### Environment (already provisioned by the update script / snapshot)

- Qt 6.8.3 (`linux_gcc_64`, with `qtmultimedia`) lives at `~/Qt/6.8.3/gcc_64`. It is **not** from
  apt — Ubuntu 24.04 only ships Qt 6.4, which is below the required `QT_MIN_VERSION` (6.8.0), so it
  is installed via `aqtinstall` (inside the `~/.venv-aqt` virtualenv, since the system Python is
  PEP 668 externally-managed).
- Qt env vars (`QT_DIR`, `PATH`, `CMAKE_PREFIX_PATH`, `QT_PLUGIN_PATH`, `LD_LIBRARY_PATH`) and
  `CC=gcc` / `CXX=g++` are exported from `~/.bashrc`. A **login/interactive shell picks these up
  automatically**; if you run in a non-interactive shell, `source ~/.bashrc` first.

### Non-obvious gotchas

- **Force GCC.** The default `/usr/bin/c++` alternative points at Clang, whose libstdc++ lookup is
  broken here (`cannot find -lstdc++`). CI uses GCC; `CC=gcc`/`CXX=g++` are set in `~/.bashrc` so
  `cmake --preset linux-debug` configures correctly. Don't remove them.
- **Headless.** For tests and for running the binary without a visible display use
  `QT_QPA_PLATFORM=offscreen`. A live X display is also available at `DISPLAY=:1` for real GUI work.
- **No audio device.** There is no PulseAudio/PipeWire server, so playback logs
  `pa_context_connect() failed` / `No audio devices available`. This is expected and does **not**
  break analysis, waveform rendering, or the test suite.

### Build / test / run (Linux)

- Configure + build: `cmake --preset linux-debug` then `cmake --build --preset linux-debug --parallel`
  (binary: `build/build_cmake/DONTFLOAT`). CI uses Release (`-DCMAKE_BUILD_TYPE=Release`).
- Tests: `cd build/build_cmake && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`.
  - **Run the suite the CI way** to get a green result: set `CI=1 GITHUB_ACTIONS=1`. Two
    integration tests — `bpm_analyzer_test`'s real-MP3 subtest and the whole
    `ui_responsiveness_test` — `QSKIP` themselves when `CI`/`GITHUB_ACTIONS` is set and are meant to
    run only locally. Without those vars they execute and can fail for environment reasons (real MP3
    beat detection on one fixture, offscreen marker-drag timing, missing audio server), not because
    of a code regression.
- Run app: GUI `./build/build_cmake/DONTFLOAT` (needs a display, e.g. `DISPLAY=:1`);
  console/batch BPM analysis `QT_QPA_PLATFORM=offscreen ./build/build_cmake/DONTFLOAT -c -f <audio>`.
- Lint: no repo-level linter/hook; `.clang-tidy` / `.clangd` are for IDE use only.
