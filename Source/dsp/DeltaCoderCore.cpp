#include "DeltaCoderCore.h"

#include <algorithm>
#include <cmath>

namespace deltaspine::dsp
{
namespace
{
constexpr float pi = 3.14159265358979323846f;

float finiteOrZero(float value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
}

float clamp01(float value) noexcept
{
    return std::clamp(finiteOrZero(value), 0.0f, 1.0f);
}

float softLimit(float value) noexcept
{
    value = std::clamp(finiteOrZero(value), -8.0f, 8.0f);
    const auto shaped = value / (1.0f + 0.34f * std::abs(value));
    return std::clamp(shaped, -0.955f, 0.955f);
}

float dbToGain(float db) noexcept
{
    db = std::clamp(finiteOrZero(db), -24.0f, 12.0f);
    return std::pow(10.0f, db / 20.0f);
}
} // namespace

void DeltaCoderCore::prepare(double, int)
{
    reset();
}

void DeltaCoderCore::reset() noexcept
{
    previousInput = 0.0f;
    previousOutput = 0.0f;
    previousDelta = 0.0f;
    adaptiveStep = 0.125f;
    pdmAccumulator = 0.0f;
    pdmReconstructionA = 0.0f;
    pdmReconstructionB = 0.0f;
    dcInputPrevious = 0.0f;
    dcOutputPrevious = 0.0f;
    inputMeanSquare = 0.0f;
    wetMeanSquare = 0.0f;
    visualWrite = 0;
    for (auto& cell : snapshotCells)
        cell.store(0.0f, std::memory_order_relaxed);
    snapshotInputRms.store(0.0f, std::memory_order_relaxed);
    snapshotWetRms.store(0.0f, std::memory_order_relaxed);
    snapshotStep.store(adaptiveStep, std::memory_order_relaxed);
    snapshotPredictor.store(0.0f, std::memory_order_relaxed);
    snapshotMode.store(0, std::memory_order_relaxed);
    snapshotWarning.store(0, std::memory_order_relaxed);
}

float DeltaCoderCore::processSample(float input, const DeltaCoderParameters& parameters) noexcept
{
    input = std::clamp(finiteOrZero(input), -1.5f, 1.5f);
    const auto mode = parameters.mode;

    float wet = 0.0f;
    if (std::abs(input) < 1.0e-8f && inputMeanSquare < 1.0e-12f)
    {
        pdmAccumulator *= 0.85f;
        pdmReconstructionA *= 0.75f;
        pdmReconstructionB *= 0.75f;
    }
    else if (mode == Mode::pdm)
    {
        wet = processPdm(input, parameters);
    }
    else
    {
        wet = processDpcm(input, parameters, mode == Mode::adpcm);
    }

    auto warning = false;
    const auto activeInput = inputMeanSquare > 3.0e-5f || std::abs(input) > 0.006f;
    if (activeInput && std::abs(wet) < 0.0006f && std::abs(input) > 0.01f)
    {
        wet += 0.18f * input;
        warning = true;
    }

    wet = applyDcBlock(wet);
    const auto mix = clamp01(parameters.mix);
    auto output = input + (wet - input) * mix;
    output = softLimit(output * dbToGain(parameters.outputDb));

    inputMeanSquare = 0.999f * inputMeanSquare + 0.001f * input * input;
    wetMeanSquare = 0.999f * wetMeanSquare + 0.001f * wet * wet;
    updateSnapshot(input, wet, wet - input, parameters, warning);
    return std::abs(input) < 1.0e-8f && inputMeanSquare < 1.0e-12f ? 0.0f : output;
}

float DeltaCoderCore::processDpcm(float input, const DeltaCoderParameters& parameters, bool adaptive) noexcept
{
    const auto predict = 1.18f * clamp01(parameters.predict);
    const auto leak = 0.0005f + 0.08f * clamp01(parameters.leak);
    const auto bits = std::clamp(static_cast<int>(std::round(finiteOrZero(parameters.depth))), 1, 8);
    const auto levels = static_cast<float>(1 << bits);
    const auto baseStep = std::max(1.0f / levels, 0.00390625f);

    const auto predictor = std::clamp(previousOutput * (predict * (1.0f - leak))
                                      + previousDelta * (0.38f + 0.45f * predict)
                                      + previousInput * (0.10f * predict),
                                      -1.4f, 1.4f);
    const auto residual = std::clamp(input - predictor, -1.8f, 1.8f);

    if (adaptive)
    {
        const auto adapt = clamp01(parameters.adapt);
        const auto target = baseStep * (1.0f + adapt * (4.0f + 28.0f * std::abs(residual)));
        adaptiveStep += (target - adaptiveStep) * (0.012f + 0.045f * adapt);
    }
    else
    {
        const auto crush = 1.0f + 3.0f * clamp01(parameters.density);
        adaptiveStep += (baseStep * crush - adaptiveStep) * 0.08f;
    }
    adaptiveStep = std::clamp(adaptiveStep, 0.002f, 1.25f);

    const auto quantizedResidual = std::clamp(std::round(residual / adaptiveStep) * adaptiveStep, -1.6f, 1.6f);
    auto reconstructed = predictor + quantizedResidual;
    const auto instability = clamp01(parameters.density);
    reconstructed += instability * 0.22f * std::sin((previousDelta + quantizedResidual) * pi * (2.0f + 10.0f * instability));
    reconstructed = std::clamp(reconstructed, -1.8f, 1.8f);

    previousDelta = std::clamp(quantizedResidual, -1.2f, 1.2f);
    previousInput = input;
    previousOutput = reconstructed * (1.0f - leak);
    return reconstructed;
}

float DeltaCoderCore::processPdm(float input, const DeltaCoderParameters& parameters) noexcept
{
    const auto density = clamp01(parameters.density);
    const auto predict = clamp01(parameters.predict);
    const auto leak = 0.001f + 0.08f * clamp01(parameters.leak);
    const auto drive = 1.0f + 6.0f * density;
    const auto target = std::clamp(input * drive + previousOutput * (0.65f * predict), -2.0f, 2.0f);

    pdmAccumulator = std::clamp((pdmAccumulator + target) * (1.0f - leak), -3.0f, 3.0f);
    const auto bit = pdmAccumulator >= 0.0f ? 1.0f : -1.0f;
    pdmAccumulator -= bit * (0.72f - 0.22f * density);

    const auto poleA = 0.62f - 0.24f * density;
    const auto poleB = 0.42f - 0.18f * density;
    pdmReconstructionA = poleA * pdmReconstructionA + (1.0f - poleA) * bit;
    pdmReconstructionB = poleB * pdmReconstructionB + (1.0f - poleB) * pdmReconstructionA;
    auto reconstructed = pdmReconstructionB * (0.65f + 0.95f * density);
    reconstructed += 0.18f * std::sin(pdmAccumulator * pi * (1.0f + 7.0f * density));
    previousDelta = reconstructed - previousOutput;
    previousOutput = std::clamp(reconstructed, -1.7f, 1.7f);
    previousInput = input;
    adaptiveStep = 1.0f;
    return previousOutput;
}

float DeltaCoderCore::applyDcBlock(float sample) noexcept
{
    const auto output = sample - dcInputPrevious + 0.995f * dcOutputPrevious;
    dcInputPrevious = sample;
    dcOutputPrevious = std::clamp(output, -2.0f, 2.0f);
    return dcOutputPrevious;
}

void DeltaCoderCore::updateSnapshot(float input,
                                    float wet,
                                    float residual,
                                    const DeltaCoderParameters& parameters,
                                    bool warning) noexcept
{
    const auto energy = std::clamp(std::abs(residual) * 2.0f + 0.25f * std::abs(wet), 0.0f, 1.0f);
    snapshotCells[static_cast<std::size_t>(visualWrite)].store(energy, std::memory_order_relaxed);
    visualWrite = (visualWrite + 1) % visualCells;
    snapshotInputRms.store(std::sqrt(std::max(inputMeanSquare, input * input * 0.001f)), std::memory_order_relaxed);
    snapshotWetRms.store(std::sqrt(std::max(wetMeanSquare, wet * wet * 0.001f)), std::memory_order_relaxed);
    snapshotStep.store(adaptiveStep, std::memory_order_relaxed);
    snapshotPredictor.store(previousOutput, std::memory_order_relaxed);
    snapshotMode.store(static_cast<int>(parameters.mode), std::memory_order_relaxed);
    snapshotWarning.store(warning ? 1 : 0, std::memory_order_relaxed);
}

void DeltaCoderCore::copySnapshot(DeltaSnapshot& destination) const noexcept
{
    for (std::size_t i = 0; i < destination.cells.size(); ++i)
        destination.cells[i] = snapshotCells[i].load(std::memory_order_relaxed);
    destination.inputRms = snapshotInputRms.load(std::memory_order_relaxed);
    destination.wetRms = snapshotWetRms.load(std::memory_order_relaxed);
    destination.step = snapshotStep.load(std::memory_order_relaxed);
    destination.predictor = snapshotPredictor.load(std::memory_order_relaxed);
    destination.mode = snapshotMode.load(std::memory_order_relaxed);
    destination.warning = snapshotWarning.load(std::memory_order_relaxed) != 0;
}
} // namespace deltaspine::dsp
