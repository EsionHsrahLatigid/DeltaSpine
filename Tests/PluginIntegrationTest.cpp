#include "ParameterIDs.h"
#include "PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
bool check(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "[FAIL] " << message << '\n';
    return condition;
}

bool checkNear(float actual, float expected, float tolerance, const char* message)
{
    return check(std::abs(actual - expected) <= tolerance, message);
}

bool checkFloatParameter(DeltaSpineAudioProcessor& processor,
                         const char* id,
                         float start,
                         float end,
                         float interval,
                         float defaultValue)
{
    auto* parameter = processor.parameters.getParameter(id);
    if (!check(parameter != nullptr, (std::string("missing parameter ") + id).c_str()))
        return false;

    auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*>(parameter);
    bool passed = check(floatParameter != nullptr, (std::string("parameter should be float ") + id).c_str());
    if (floatParameter != nullptr)
    {
        passed &= checkNear(floatParameter->range.start, start, 0.0001f, "float range start should match");
        passed &= checkNear(floatParameter->range.end, end, 0.0001f, "float range end should match");
        passed &= checkNear(floatParameter->range.interval, interval, 0.0001f, "float range interval should match");
        passed &= checkNear(processor.parameters.getRawParameterValue(id)->load(), defaultValue, 0.0001f,
                            "float default should match");
    }
    return passed;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    auto processor = std::make_unique<DeltaSpineAudioProcessor>();
    bool passed = true;

    passed &= check(processor->getName() == "DeltaSpine", "product name should be DeltaSpine");
    passed &= check(!processor->acceptsMidi(), "processor should not accept MIDI");
    passed &= check(!processor->isMidiEffect(), "processor should be an audio effect");
    passed &= check(processor->getLatencySamples() == 0, "processor should be zero-latency before prepare");

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    passed &= check(editor != nullptr, "processor should create an editor");
    if (editor != nullptr)
    {
        passed &= check(editor->getWidth() == 520 && editor->getHeight() == 332,
                        "editor should use the verified compact workflow study");
        passed &= check(editor->findChildWithID("deltaspine-delta-history") != nullptr,
                        "editor should expose the delta history matrix");
        passed &= check(editor->findChildWithID("deltaspine-mode") != nullptr,
                        "editor should expose mode selection");
        passed &= check(editor->findChildWithID("deltaspine-control-pred") != nullptr,
                        "editor should expose the predictor control");
        editor->setBounds(0, 0, 520, 332);
        editor->resized();
    }

    juce::AudioProcessor::BusesLayout monoToStereo;
    monoToStereo.inputBuses.add(juce::AudioChannelSet::mono());
    monoToStereo.outputBuses.add(juce::AudioChannelSet::stereo());
    passed &= check(processor->isBusesLayoutSupported(monoToStereo), "mono input/stereo output should be supported");

    passed &= check(processor->getParameters().size() == 8, "processor should expose exactly eight public controls");
    auto* mode = processor->parameters.getParameter(deltaspine::parameters::mode);
    passed &= check(dynamic_cast<juce::AudioParameterChoice*>(mode) != nullptr, "Mode parameter should be a choice");
    passed &= checkNear(processor->parameters.getRawParameterValue(deltaspine::parameters::mode)->load(), 1.0f, 0.0001f,
                        "Mode default should be ADPCM");
    passed &= checkFloatParameter(*processor, deltaspine::parameters::predict, 0.0f, 1.0f, 0.001f, 0.65f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::depth, 1.0f, 8.0f, 1.0f, 4.0f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::adapt, 0.0f, 1.0f, 0.001f, 0.55f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::leak, 0.0f, 1.0f, 0.001f, 0.08f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::density, 0.0f, 1.0f, 0.001f, 0.45f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::mix, 0.0f, 1.0f, 0.001f, 1.0f);
    passed &= checkFloatParameter(*processor, deltaspine::parameters::output, -24.0f, 12.0f, 0.1f, 0.0f);

    auto* predict = processor->parameters.getParameter(deltaspine::parameters::predict);
    if (predict != nullptr && mode != nullptr)
    {
        predict->setValueNotifyingHost(predict->convertTo0to1(0.91f));
        mode->setValueNotifyingHost(1.0f);
        juce::MemoryBlock state;
        processor->getStateInformation(state);
        predict->setValueNotifyingHost(predict->convertTo0to1(0.11f));
        mode->setValueNotifyingHost(0.0f);
        processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        passed &= check(std::abs(processor->parameters.getRawParameterValue(deltaspine::parameters::predict)->load() - 0.91f) < 0.001f,
                        "APVTS float state should round-trip");
        passed &= check(processor->parameters.getRawParameterValue(deltaspine::parameters::mode)->load() > 1.5f,
                        "APVTS mode state should round-trip");
    }

    constexpr double sampleRate = 48000.0;
    processor->prepareToPlay(sampleRate, 1024);
    passed &= check(processor->getLatencySamples() == 0, "prepared processor should remain zero-latency");
    const int blockSizes[] { 1, 17, 64, 127, 511 };
    int generatedSamples = 0;
    for (const auto blockSize : blockSizes)
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto value = static_cast<float>(0.24 * std::sin(2.0 * juce::MathConstants<double>::pi
                                                                 * 530.0 * generatedSamples / sampleRate));
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, -value);
            ++generatedSamples;
        }
        juce::MidiBuffer midi;
        processor->processBlock(audio, midi);
        passed &= check(midi.isEmpty(), "processor should clear MIDI");
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                passed &= check(std::isfinite(audio.getSample(channel, sample)), "processed audio should remain finite");
    }

    processor->reset();
    juce::AudioBuffer<float> silence(2, 2048);
    silence.clear();
    juce::MidiBuffer midi;
    processor->processBlock(silence, midi);
    float silencePeak = 0.0f;
    for (int channel = 0; channel < silence.getNumChannels(); ++channel)
        silencePeak = std::max(silencePeak, silence.getMagnitude(channel, 0, silence.getNumSamples()));
    passed &= check(silencePeak == 0.0f, "silence in should remain silence out");

    if (passed)
        std::cout << "DeltaSpine plug-in integration checks passed\n";
    return passed ? 0 : 1;
}

