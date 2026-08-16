# DeltaSpine Research and Decision Foundation

## Research question

How should DeltaSpine turn differential coding into a harsh, controllable audio effect without collapsing into silence, DC lock, or unstable feedback?

## Request type

Comprehensive research: implementation reference lookup plus current best-practice evidence for a chosen JUCE effect design.

## Primary sources

- J. B. O'Neal Jr., *Predictive Quantizing Systems (Differential Pulse Code Modulation) for the Transmission of Television Signals* (Bell System Technical Journal, 1966). URL: https://onlinelibrary.wiley.com/doi/abs/10.1002/j.1538-7305.1966.tb01052.x
  - DPCM and predictive quantizing are explicitly treated as techniques that remove redundancy via previous-sample and previous-line feedback.
  - The paper also states the tradeoff that removing redundancy makes the transmitted signal more fragile.
- ITU-T Recommendation G.726, *40, 32, 24, 16 kbit/s Adaptive Differential Pulse Code Modulation (ADPCM)*. URL: https://www.itu.int/rec/T-REC-G.726
  - The recommendation is in force and dates from 1990-12-14, with later annexes and test sequences still listed by ITU.
  - ITU also exposes the corresponding ANSI-C reference code in the G.191 library.
- ITU-T Recommendation G.727, *5-, 4-, 3- and 2-bit/sample embedded adaptive differential pulse code modulation (ADPCM)*. URL: https://www.itu.int/ITU-T/recommendations/rec.aspx?rec=927
  - Useful as the embedded/bit-scaled ADPCM reference point for mode scaling and deterministic test vectors.
- TI Precision Labs, *Audio serial interface formats* (2022-08-19). URL: https://www.ti.com/video/6311104210112
  - Defines PDM as a one-bit audio representation where density of 1s corresponds to amplitude.
  - Notes that PDM is advantageous because it can be converted back with low-pass filtering.
- TI product page, *PCMD3140-Q1 Quad-Channel, PDM Input to TDM or I2S Output Converter* (datasheet posted 2022-07-12). URL: https://www.ti.com/product/PCMD3140-Q1
  - Confirms PDM as a real one-bit microphone/data path, exposes low-latency filter selection, programmable HPF/biquads, and 768 kHz output-rate capability.
- H. P. Jaiswal et al., *Adaptive Delta Modulation* (IETE Journal of Research, 1971). URL: https://www.tandfonline.com/doi/abs/10.1080/03772063.1971.11486822
  - Reinforces that step-size/companding choices are central to keeping signal-to-quantization-noise ratio usable.
- JUCE `AudioProcessor` docs. URL: https://docs.juce.com/master/classjuce_1_1AudioProcessor.html
  - `prepareToPlay`, `processBlock`, `processBlockBypassed`, `setLatencySamples`, `suspendProcessing`, and `reset` are the relevant realtime and latency hooks.

## Repository pattern references

- `BitRash/README.md` and `BitRash/CMakeLists.txt` show the current EHL pattern for:
  - `COMPANY_NAME "EsionHsrahLatigid"`
  - `PLUGIN_MANUFACTURER_CODE EHL_`
  - `BUNDLE_ID "jp.ehl.<slug>"`
  - stable artifact staging under `artifacts/plugin-release/<platform>`
  - reusable `ehl_stage_products`
- `BinGrave/README.md` and `BinGrave/CMakeLists.txt` show the same release pattern with an independent DSP core and shared JUCE design module.
- `juce-ehl-design-module/DESIGN_CONTRACT.md` fixes the compact monochrome 8-bit UI system and the canonical `ehl` header mark.

## Decision map

### 1. Common topology

DeltaSpine should use a shared predictor/residual core with three operational faces:

1. fixed-predictor DPCM;
2. adaptive ADPCM;
3. 1-bit PDM-style remodulation for the most destructive mode.

The core should remain deterministic, block-boundary safe, and allocation-free in the audio callback.

### 2. DPCM mode

- Use a bounded linear predictor on the current input history.
- Quantize the residual with a controllable step size or bit-depth.
- Reconstruct `output = predictor + quantizedResidual`.
- Add a gentle leak and DC blocker so the loop cannot pin itself to a constant offset.

### 3. ADPCM mode

- Use the DPCM core plus step-size adaptation.
- Keep adaptation deterministic and clamped.
- Prefer a normalized residual path over a large, unconstrained predictor update.
- Treat the ITU G.726/G.727 mode families as the source of the bit-scaled design space.

### 4. PDM mode

- Treat PDM as a 1-bit density remodulator, not as a fake PCM compressor.
- Use a bounded accumulator with a short reconstruction filter.
- Maintain an explicit leak / bias reset so the 1-bit loop does not freeze into an all-zero or all-one rail.
- Expose the mode as a deliberately extreme texture, not a transparency claim.

### 5. Safety and audibility

The plugin must not fail by:

- collapsing into silence after an active input;
- pinning to DC or a single rail;
- producing NaN/Inf;
- becoming ultrasonic-only;
- hiding all motion behind overcompression.

The output strategy should therefore include:

- bounded feedback;
- a DC blocker on the reconstructed path;
- finite guards;
- an audibility-preserving recovery path when the residual path collapses under active input;
- a final output trim.

## Proposed parameter set

The set below is the current research recommendation, not a final implementation contract.

- `mode`: `DPCM`, `ADPCM`, `PDM`
- `predict`: `0.0 .. 1.0`, default `0.65`
- `depth`: `1 .. 8` effective bits, default `4`
- `adapt`: `0.0 .. 1.0`, default `0.55`
- `leak`: `0.0 .. 1.0`, default `0.08`
- `density`: `0.0 .. 1.0`, default `0.45`
- `mix`: `0.0 .. 1.0`, default `1.0`
- `output`: `-24 dB .. +12 dB`, default `0 dB`

If the final layout needs fewer controls, `density` can be folded into `mode`, and `predict` can become a single `shape` macro.

## UI direction

- Use the shared `juce-ehl-design-module`.
- Keep the editor monochrome, compact, and operational.
- Use the canonical short `ehl` mark in the header.
- Avoid decorative pseudo-hardware, color, glow, and fake meters.
- Prefer a compact state card that shows mode, depth, predictor state, and a tiny residual activity surface.
- Use the canonical JUCE size family from the shared design module unless a measured target-specific footprint justifies a smaller editor.

## Anti-collapse test plan

The plugin should be validated against:

- impulse and burst input at `44.1`, `48`, and `96 kHz`;
- 1 kHz sine, swept sine, pink noise, and sparse transient material;
- block sizes `1`, `17`, `64`, `256`, `512`;
- repeated state restore / reset cycles;
- extreme parameter combinations for each mode;
- deterministic same-seed render equality;
- RMS, peak, crest, DC share, zero-crossing count, and active-window counts;
- finite-output and non-silent-output assertions under active input;
- bypass / latency compensation checks via `processBlockBypassed` and `setLatencySamples` when needed.

## Risks and open questions

- Exact predictor order: a shallow order may be safer and more characterful than a large adaptive matrix.
- Whether PDM should use explicit oversampling or an internal fixed-rate accumulator.
- Whether audibility recovery should be automatic, user-exposed, or hidden behind safety logic.
- Whether the final plugin should expose a visible latency readout if PDM mode needs alignment delay.

## Reusable takeaway

DeltaSpine should be a bounded predictive coder effect: DPCM at the base, ADPCM as the adaptive middle, and PDM as the extreme one-bit face, all wrapped in a compact EHL monochrome UI with explicit anti-collapse safety.

