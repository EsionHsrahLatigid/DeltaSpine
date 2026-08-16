# DeltaSpine Design

DeltaSpine uses the shared `juce-ehl-design-module` as a pinned submodule.

The editor is a compact monochrome 8-bit control surface:

- canonical short EHL mark appears once through `paintEditorChrome`;
- one residual activity matrix;
- one status row with mode, input level, wet level, adaptive step, and predictor state;
- mode selector plus seven direct controls;
- no color accents, decorative glow, pseudo-hardware, or ornamental damage.

The current verified local editor footprint is `520 x 332`.

