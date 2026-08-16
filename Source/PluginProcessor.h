#pragma once

#include "dsp/DeltaCoderCore.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class DeltaSpineAudioProcessor final : public juce::AudioProcessor
{
public:
    DeltaSpineAudioProcessor();
    ~DeltaSpineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void copyDeltaSnapshot(deltaspine::dsp::DeltaSnapshot& destination) const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ParameterPointers
    {
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* predict = nullptr;
        std::atomic<float>* depth = nullptr;
        std::atomic<float>* adapt = nullptr;
        std::atomic<float>* leak = nullptr;
        std::atomic<float>* density = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
    } parameter;

    void cacheParameterPointers();
    [[nodiscard]] deltaspine::dsp::DeltaCoderParameters readParameters() const noexcept;

    std::array<deltaspine::dsp::DeltaCoderCore, 2> cores;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeltaSpineAudioProcessor)
};

