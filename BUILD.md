# DeltaSpine Build Notes

DeltaSpine follows the EHL JUCE CMake workflow.

## Presets

- `engine-debug`: builds `DeltaSpineTests` without JUCE.
- `plugin-release`: builds VST3, Standalone, AU on macOS, integration tests, host load test, and staged products.

## Local JUCE

Use `EHL_JUCE_SOURCE_DIR` to avoid network fetches:

```sh
cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/Users/2bit/prog/juce/Plitch/build/release/_deps/juce-src -DEHL_COPY_PLUGIN_AFTER_BUILD=OFF
```

## Artifacts

Staged artifacts are written to:

```text
artifacts/plugin-release/macos-arm64/
artifacts/plugin-release/windows-x64/
artifacts/plugin-release/linux-x64/
```

