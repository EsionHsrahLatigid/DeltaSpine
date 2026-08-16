#include "../Source/dsp/DeltaCoderCore.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace deltaspine::dsp;

struct Failure final : std::exception
{
    explicit Failure(std::string messageIn) : message(std::move(messageIn)) {}
    const char* what() const noexcept override { return message.c_str(); }
    std::string message;
};

struct Metrics
{
    float rms = 0.0f;
    float peak = 0.0f;
    float dc = 0.0f;
    int zeroCrossings = 0;
    int clipped = 0;
    int uniqueBuckets = 0;
};

[[noreturn]] void fail(const std::string& message)
{
    throw Failure(message);
}

void expect(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

void near(float actual, float expected, float tolerance, const std::string& message)
{
    if (std::abs(actual - expected) > tolerance)
        fail(message + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}

Metrics measure(const std::vector<float>& signal)
{
    Metrics result;
    double sum = 0.0;
    double sumSquares = 0.0;
    bool hadPrevious = false;
    float previous = 0.0f;
    std::vector<int> buckets;
    buckets.reserve(signal.size());

    for (auto sample : signal)
    {
        expect(std::isfinite(sample), "output must be finite");
        sum += sample;
        sumSquares += static_cast<double>(sample) * sample;
        result.peak = std::max(result.peak, std::abs(sample));
        if (std::abs(sample) >= 0.979f)
            ++result.clipped;
        if (hadPrevious && ((previous < 0.0f && sample >= 0.0f) || (previous >= 0.0f && sample < 0.0f)))
            ++result.zeroCrossings;
        previous = sample;
        hadPrevious = true;
        buckets.push_back(static_cast<int>(std::round(sample * 4096.0f)));
    }

    std::sort(buckets.begin(), buckets.end());
    result.uniqueBuckets = static_cast<int>(std::unique(buckets.begin(), buckets.end()) - buckets.begin());
    result.rms = signal.empty() ? 0.0f : static_cast<float>(std::sqrt(sumSquares / static_cast<double>(signal.size())));
    result.dc = signal.empty() ? 0.0f : static_cast<float>(sum / static_cast<double>(signal.size()));
    return result;
}

std::vector<float> render(DeltaCoderParameters params,
                          const std::vector<float>& input,
                          const std::vector<int>& partitions,
                          double sampleRate = 48000.0)
{
    auto core = std::make_unique<DeltaCoderCore>();
    core->prepare(sampleRate, 1);
    std::vector<float> output;
    output.reserve(input.size());
    std::size_t index = 0;
    std::size_t partition = 0;
    while (index < input.size())
    {
        const auto count = std::min<std::size_t>(static_cast<std::size_t>(partitions[partition % partitions.size()]),
                                                 input.size() - index);
        for (std::size_t i = 0; i < count; ++i)
            output.push_back(core->processSample(input[index++], params));
        ++partition;
    }
    return output;
}

std::vector<float> sine(float hz, std::size_t samples)
{
    std::vector<float> result(samples);
    for (std::size_t i = 0; i < samples; ++i)
        result[i] = 0.35f * std::sin(2.0f * 3.14159265358979323846f * hz * static_cast<float>(i) / 48000.0f);
    return result;
}

std::vector<float> burst(std::size_t samples)
{
    std::vector<float> result(samples, 0.0f);
    for (std::size_t i = 160; i < samples; ++i)
    {
        const auto gate = ((i / 97) % 3) == 0 ? 1.0f : 0.25f;
        result[i] = gate * (0.31f * std::sin(0.11f * static_cast<float>(i))
                          + 0.19f * std::sin(0.47f * static_cast<float>(i)));
    }
    return result;
}

std::vector<float> seededNoise(std::size_t samples)
{
    std::vector<float> result(samples);
    unsigned state = 0x6d2b79f5u;
    for (auto& sample : result)
    {
        state = state * 1664525u + 1013904223u;
        sample = (static_cast<float>((state >> 8u) & 0xffffu) / 32768.0f - 1.0f) * 0.28f;
    }
    return result;
}

float differenceEnergy(const std::vector<float>& a, const std::vector<float>& b)
{
    expect(a.size() == b.size(), "difference vectors should be same size");
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const auto d = static_cast<double>(a[i] - b[i]);
        sum += d * d;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(a.size())));
}

void silence_and_nonfinite_are_safe()
{
    DeltaCoderParameters params;
    for (auto mode : { Mode::dpcm, Mode::adpcm, Mode::pdm })
    {
        params.mode = mode;
        const auto silent = render(params, std::vector<float>(4096, 0.0f), { 1, 17, 64, 257 });
        expect(measure(silent).rms == 0.0f, "silence must remain silence");
    }

    auto core = std::make_unique<DeltaCoderCore>();
    core->prepare(44100.0, 2);
    for (int i = 0; i < 12000; ++i)
    {
        const auto sample = i == 17 ? INFINITY : i == 901 ? NAN : 0.12f * std::sin(0.013f * static_cast<float>(i));
        expect(std::isfinite(core->processSample(sample, params)), "non-finite input must be guarded");
    }
}

void dpcm_depth_changes_residual_oracle()
{
    auto input = sine(733.0f, 24000);
    DeltaCoderParameters coarse;
    coarse.mode = Mode::dpcm;
    coarse.depth = 1.0f;
    coarse.predict = 0.72f;
    coarse.density = 0.72f;

    auto fine = coarse;
    fine.depth = 8.0f;
    fine.density = 0.0f;

    const auto coarseOut = render(coarse, input, { 31, 128, 511 });
    const auto fineOut = render(fine, input, { 31, 128, 511 });
    const auto coarseMetrics = measure(coarseOut);
    const auto fineMetrics = measure(fineOut);
    expect(differenceEnergy(coarseOut, fineOut) > 0.025f, "bit depth should materially change the residual path");
    expect(coarseMetrics.uniqueBuckets < fineMetrics.uniqueBuckets, "coarse DPCM should have fewer quantized buckets than fine DPCM");
    expect(coarseMetrics.rms > 0.015f, "coarse DPCM should remain audible");
    expect(std::abs(coarseMetrics.dc) < coarseMetrics.rms, "coarse DPCM should not become DC-only");
}

void adaptive_path_reacts_to_burst_levels()
{
    DeltaCoderParameters params;
    params.mode = Mode::adpcm;
    params.depth = 3.0f;
    params.adapt = 1.0f;
    params.predict = 0.82f;
    auto output = render(params, burst(48000), { 16, 64, 127, 512 });
    const auto metrics = measure(output);
    expect(metrics.rms > 0.03f, "adaptive coder should produce active output");
    expect(metrics.zeroCrossings > 100, "adaptive coder should preserve changing motion");
    expect(metrics.uniqueBuckets > 48, "adaptive coder should not lock to a tiny codebook");

    auto lowAdapt = params;
    lowAdapt.adapt = 0.0f;
    expect(differenceEnergy(output, render(lowAdapt, burst(48000), { 16, 64, 127, 512 })) > 0.01f,
           "adapt control should alter the coder trajectory");
}

void pdm_one_bit_mode_is_extreme_and_bounded()
{
    DeltaCoderParameters params;
    params.mode = Mode::pdm;
    params.depth = 1.0f;
    params.predict = 0.95f;
    params.density = 1.0f;
    params.leak = 0.03f;
    params.outputDb = 6.0f;
    const auto output = render(params, seededNoise(64000), { 1, 17, 64, 333 });
    const auto metrics = measure(output);
    std::cout << "pdm rms=" << metrics.rms
              << " peak=" << metrics.peak
              << " dc=" << metrics.dc
              << " zeroCross=" << metrics.zeroCrossings
              << " unique=" << metrics.uniqueBuckets
              << " clipped=" << metrics.clipped << '\n';
    expect(metrics.rms > 0.08f, "1-bit mode should be strongly audible");
    expect(metrics.peak <= 0.986f, "1-bit mode should stay below the final ceiling");
    expect(metrics.zeroCrossings > 1000, "1-bit mode should not flatten");
    expect(metrics.uniqueBuckets > 64, "1-bit reconstruction should not be a constant rail");
    expect(metrics.clipped < static_cast<int>(output.size() / 30), "1-bit mode should not be sustained clipping");
    expect(std::abs(metrics.dc) < metrics.rms * 0.9f, "1-bit mode should not be DC-dominated");
}

void block_partition_sample_rate_reset_determinism()
{
    DeltaCoderParameters params;
    params.mode = Mode::adpcm;
    params.depth = 2.0f;
    params.predict = 0.9f;
    params.adapt = 0.8f;
    params.density = 0.7f;
    const auto input = seededNoise(32000);
    const auto reference = render(params, input, { 512 }, 48000.0);
    const auto split = render(params, input, { 1, 17, 63, 128, 511 }, 48000.0);
    expect(reference == split, "render should be block-size independent");
    expect(reference == render(params, input, { 512 }, 48000.0), "same render should be deterministic");
    expect(measure(render(params, input, { 64 }, 96000.0)).rms > 0.01f, "96 kHz prepare should stay active");

    auto core = std::make_unique<DeltaCoderCore>();
    core->prepare(48000.0, 1);
    for (int i = 0; i < 1000; ++i)
        (void) core->processSample(0.2f, params);
    core->reset();
    near(core->processSample(0.0f, params), 0.0f, 0.0f, "reset should clear residual output on silence");
}

void snapshot_handoff_is_lock_free_friendly()
{
    auto core = std::make_unique<DeltaCoderCore>();
    core->prepare(48000.0, 1);
    DeltaCoderParameters params;
    params.mode = Mode::pdm;
    params.density = 0.85f;
    std::atomic<bool> stop { false };
    std::atomic<int> invalid { 0 };
    std::thread reader([&]() {
        DeltaSnapshot snapshot;
        while (!stop.load(std::memory_order_acquire))
        {
            core->copySnapshot(snapshot);
            if (!std::isfinite(snapshot.inputRms) || !std::isfinite(snapshot.wetRms)
                || !std::isfinite(snapshot.step) || !std::isfinite(snapshot.predictor))
                invalid.fetch_add(1, std::memory_order_relaxed);
            for (auto cell : snapshot.cells)
                if (!std::isfinite(cell) || cell < 0.0f || cell > 1.0f)
                    invalid.fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (int i = 0; i < 180000; ++i)
        expect(std::isfinite(core->processSample(0.25f * std::sin(0.019f * static_cast<float>(i)), params)),
               "stress output should stay finite");

    stop.store(true, std::memory_order_release);
    reader.join();
    expect(invalid.load(std::memory_order_relaxed) == 0, "UI snapshot should not observe invalid values");
}

void extreme_audibility_rejects_collapses()
{
    for (auto mode : { Mode::dpcm, Mode::adpcm, Mode::pdm })
    {
        DeltaCoderParameters params;
        params.mode = mode;
        params.depth = 1.0f;
        params.predict = 1.0f;
        params.adapt = 1.0f;
        params.leak = 0.0f;
        params.density = 1.0f;
        params.mix = 1.0f;
        params.outputDb = 12.0f;
        const auto output = render(params, sine(1000.0f, 64000), { 7, 19, 251 });
        const auto metrics = measure(output);
        std::cout << "mode=" << static_cast<int>(mode)
                  << " rms=" << metrics.rms
                  << " peak=" << metrics.peak
                  << " dc=" << metrics.dc
                  << " zeroCross=" << metrics.zeroCrossings
                  << " unique=" << metrics.uniqueBuckets
                  << " clipped=" << metrics.clipped << '\n';
        expect(metrics.rms > 0.01f, "extreme coder should not become all-zero");
        expect(metrics.peak <= 0.986f, "extreme coder should stay bounded");
        expect(std::abs(metrics.dc) < metrics.rms * 0.95f, "extreme coder should not be DC dominated");
        expect(metrics.zeroCrossings > 16, "extreme coder should not become static");
        expect(metrics.uniqueBuckets > 24, "extreme coder should not become one constant value");
        expect(metrics.clipped < static_cast<int>(output.size() / 16), "extreme coder should not sit on rails");
    }
}

void run(const char* name, void (*fn)())
{
    fn();
    std::cout << "[pass] " << name << '\n';
}
} // namespace

int main()
{
    try
    {
        run("silence_and_nonfinite_are_safe", silence_and_nonfinite_are_safe);
        run("dpcm_depth_changes_residual_oracle", dpcm_depth_changes_residual_oracle);
        run("adaptive_path_reacts_to_burst_levels", adaptive_path_reacts_to_burst_levels);
        run("pdm_one_bit_mode_is_extreme_and_bounded", pdm_one_bit_mode_is_extreme_and_bounded);
        run("block_partition_sample_rate_reset_determinism", block_partition_sample_rate_reset_determinism);
        run("snapshot_handoff_is_lock_free_friendly", snapshot_handoff_is_lock_free_friendly);
        run("extreme_audibility_rejects_collapses", extreme_audibility_rejects_collapses);
    }
    catch (const Failure& failure)
    {
        std::cerr << "[fail] " << failure.what() << '\n';
        return 1;
    }
    return 0;
}
