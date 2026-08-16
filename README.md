# DeltaSpine

DeltaSpine is an EsionHsrahLatigid JUCE audio effect that abuses predictive delta coding as a controllable fracture processor.

It combines fixed DPCM, adaptive residual coding, and 1-bit density remodulation behind a compact monochrome EHL interface. The DSP core is deterministic, bounded, zero-latency, and allocation-free during audio processing.

## Controls

- `Mode`: DPCM, ADPCM, or 1BIT.
- `Predict`: predictor strength and loop memory.
- `Depth`: effective residual code depth.
- `Adapt`: adaptive step response.
- `Leak`: predictor and density-loop bleed.
- `Density`: residual instability and 1-bit density pressure.
- `Mix`: dry/wet balance.
- `Output`: final gain trim.

## Build

Fast DSP tests:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure
```

Full plug-in build with a local JUCE checkout:

```sh
cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/absolute/path/to/JUCE -DEHL_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

Readable products are staged under `artifacts/plugin-release/<platform>/`.

## Identity

- Manufacturer: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Bundle ID: `jp.ehl.deltaspine`
- Plug-in code: `DlSp`
- Formats: VST3, Standalone, AU on macOS

## Research Basis

The implementation is based on predictive quantizing and differential coding literature, ITU ADPCM recommendations, 1-bit PDM audio interfaces, and JUCE realtime processor contracts. Internal research notes are kept outside public release copy.

