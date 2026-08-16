#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace deltaspine::dsp
{
enum class Mode
{
    dpcm = 0,
    adpcm = 1,
    pdm = 2
};

struct DeltaCoderParameters
{
    Mode mode = Mode::adpcm;
    float predict = 0.65f;
    float depth = 4.0f;
    float adapt = 0.55f;
    float leak = 0.08f;
    float density = 0.45f;
    float mix = 1.0f;
    float outputDb = 0.0f;
};

struct DeltaSnapshot
{
    static constexpr int columns = 16;
    static constexpr int rows = 8;
    std::array<float, columns * rows> cells {};
    float inputRms = 0.0f;
    float wetRms = 0.0f;
    float step = 0.0f;
    float predictor = 0.0f;
    int mode = 0;
    bool warning = false;
};

class DeltaCoderCore final
{
public:
    void prepare(double sampleRate, int channels);
    void reset() noexcept;
    float processSample(float input, const DeltaCoderParameters& parameters) noexcept;
    void copySnapshot(DeltaSnapshot& destination) const noexcept;

private:
    static constexpr int visualCells = DeltaSnapshot::columns * DeltaSnapshot::rows;

    [[nodiscard]] float processDpcm(float input, const DeltaCoderParameters& parameters, bool adaptive) noexcept;
    [[nodiscard]] float processPdm(float input, const DeltaCoderParameters& parameters) noexcept;
    [[nodiscard]] float applyDcBlock(float sample) noexcept;
    void updateSnapshot(float input, float wet, float residual, const DeltaCoderParameters& parameters, bool warning) noexcept;

    float previousInput = 0.0f;
    float previousOutput = 0.0f;
    float previousDelta = 0.0f;
    float adaptiveStep = 0.125f;
    float pdmAccumulator = 0.0f;
    float pdmReconstructionA = 0.0f;
    float pdmReconstructionB = 0.0f;
    float dcInputPrevious = 0.0f;
    float dcOutputPrevious = 0.0f;
    float inputMeanSquare = 0.0f;
    float wetMeanSquare = 0.0f;
    int visualWrite = 0;

    std::array<std::atomic<float>, visualCells> snapshotCells {};
    std::atomic<float> snapshotInputRms { 0.0f };
    std::atomic<float> snapshotWetRms { 0.0f };
    std::atomic<float> snapshotStep { 0.0f };
    std::atomic<float> snapshotPredictor { 0.0f };
    std::atomic<int> snapshotMode { 0 };
    std::atomic<int> snapshotWarning { 0 };
};
} // namespace deltaspine::dsp

