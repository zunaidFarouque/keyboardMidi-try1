# AI Build and Test Guide

Instructions for AI agents (and humans) on how to build and test the MIDIQy project.

---

## Prerequisites

- **CMake** 3.22+
- **C++20** compiler (MSVC on Windows, Clang/GCC on macOS/Linux)
- **JUCE** (included as submodule at `JUCE/`)

---

## Build

### Standard build (Debug)

From the **project root**:

```bash
cmake --build build --config Debug
```

- **Windows (MSVC):** Use `--config Debug` or `--config Release`
- **macOS/Linux:** Typically single-config; `CMAKE_BUILD_TYPE` set at configure time

### First-time setup (no build folder yet)

```bash
cmake -B build
cmake --build build --config Debug
```

### Build outputs

| Target | Output path (Debug) |
|--------|---------------------|
| Main app | `build/MIDIQy_artefacts/Debug/MIDIQy.exe` |
| Tests | `build/Debug/MIDIQy_Tests.exe` |
| Benchmarks | `build/Debug/MIDIQy_Benchmarks.exe` |

---

## Run Tests

### Recommended: run test executable directly

```bash
build/Debug/MIDIQy_Tests.exe
```

Or from project root on Windows:

```bash
.\build\Debug\MIDIQy_Tests.exe
```

**Options:**
- `--gtest_brief=1` — shorter output
- `--gtest_filter=SanityCheck.*` — run specific tests
- `--gtest_list_tests` — list all tests

### Alternative: CTest

```bash
ctest -C Debug --output-on-failure
```

**Note:** On some setups, `gtest_discover_tests` may not populate CTest correctly. If `ctest` reports "No tests were found", use the direct executable approach above.

---

## Verify Tests Pass

After making changes:

1. **Build:** `cmake --build build --config Debug`
2. **Run tests:** `build/Debug/MIDIQy_Tests.exe --gtest_brief=1`
3. Exit code 0 and `[  PASSED  ] N tests` → success

---

## Benchmarks (optional)

Benchmarks are run manually, not via CTest:

```bash
build/Debug/MIDIQy_Benchmarks.exe
```

---

## Run the Main App

```bash
build/MIDIQy_artefacts/Debug/MIDIQy.exe
```

---

## Summary for AI

| Action | Command |
|--------|---------|
| Build | `cmake --build build --config Debug` |
| Test | `build/Debug/MIDIQy_Tests.exe --gtest_brief=1` |
| Verify | Check exit code 0 and `[  PASSED  ]` in output |

---

## Current Status

- **Tests:** 459 passing, 16 disabled (as of last run)
- **Build:** Debug and Release configs supported
- **Platform:** Windows (MSVC) primary; macOS/Linux should work with same CMake flow
