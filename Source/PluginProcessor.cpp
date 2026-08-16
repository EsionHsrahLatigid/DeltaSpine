#include "PluginProcessor.h"
#include "ParameterIDs.h"
#include "PluginEditor.h"

#include <algorithm>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;
using Layout = APVTS::ParameterLayout;

std::unique_ptr<juce::AudioParameterFloat> makeFloat(const char* id,
                                                      const char* name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, defaultValue);
}
} // namespace

DeltaSpineAudioProcessor::DeltaSpineAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("DeltaSpineState"), createParameterLayout())
{
    cacheParameterPointers();
}

Layout DeltaSpineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> values;
    values.reserve(8);
    juce::StringArray modes { "DPCM", "ADPCM", "1BIT" };
    values.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { deltaspine::parameters::mode, 1 }, "Mode", modes, 1));
    values.push_back(makeFloat(deltaspine::parameters::predict, "Predict", { 0.0f, 1.0f, 0.001f }, 0.65f));
    values.push_back(makeFloat(deltaspine::parameters::depth, "Depth", { 1.0f, 8.0f, 1.0f }, 4.0f));
    values.push_back(makeFloat(deltaspine::parameters::adapt, "Adapt", { 0.0f, 1.0f, 0.001f }, 0.55f));
    values.push_back(makeFloat(deltaspine::parameters::leak, "Leak", { 0.0f, 1.0f, 0.001f }, 0.08f));
    values.push_back(makeFloat(deltaspine::parameters::density, "Density", { 0.0f, 1.0f, 0.001f }, 0.45f));
    values.push_back(makeFloat(deltaspine::parameters::mix, "Mix", { 0.0f, 1.0f, 0.001f }, 1.0f));
    values.push_back(makeFloat(deltaspine::parameters::output, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f));
    return { values.begin(), values.end() };
}

void DeltaSpineAudioProcessor::cacheParameterPointers()
{
    parameter.mode = parameters.getRawParameterValue(deltaspine::parameters::mode);
    parameter.predict = parameters.getRawParameterValue(deltaspine::parameters::predict);
    parameter.depth = parameters.getRawParameterValue(deltaspine::parameters::depth);
    parameter.adapt = parameters.getRawParameterValue(deltaspine::parameters::adapt);
    parameter.leak = parameters.getRawParameterValue(deltaspine::parameters::leak);
    parameter.density = parameters.getRawParameterValue(deltaspine::parameters::density);
    parameter.mix = parameters.getRawParameterValue(deltaspine::parameters::mix);
    parameter.output = parameters.getRawParameterValue(deltaspine::parameters::output);
}

void DeltaSpineAudioProcessor::prepareToPlay(double sampleRate, int)
{
    setLatencySamples(0);
    for (auto& core : cores)
        core.prepare(sampleRate, 1);
}

void DeltaSpineAudioProcessor::releaseResources()
{
}

void DeltaSpineAudioProcessor::reset()
{
    for (auto& core : cores)
        core.reset();
}

bool DeltaSpineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo()
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

deltaspine::dsp::DeltaCoderParameters DeltaSpineAudioProcessor::readParameters() const noexcept
{
    deltaspine::dsp::DeltaCoderParameters result;
    const auto modeValue = std::clamp(static_cast<int>(parameter.mode->load() + 0.5f), 0, 2);
    result.mode = modeValue == 0 ? deltaspine::dsp::Mode::dpcm
                : modeValue == 1 ? deltaspine::dsp::Mode::adpcm
                                 : deltaspine::dsp::Mode::pdm;
    result.predict = parameter.predict->load();
    result.depth = parameter.depth->load();
    result.adapt = parameter.adapt->load();
    result.leak = parameter.leak->load();
    result.density = parameter.density->load();
    result.mix = parameter.mix->load();
    result.outputDb = parameter.output->load();
    return result;
}

void DeltaSpineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto inputChannels = std::clamp(getTotalNumInputChannels(), 1, 2);
    const auto outputChannels = std::min(buffer.getNumChannels(), 2);
    if (numSamples <= 0 || outputChannels <= 0)
        return;

    for (int channel = outputChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    const auto params = readParameters();
    auto* left = buffer.getWritePointer(0);
    auto* right = outputChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftIn = left[sample];
        const auto rightIn = inputChannels > 1 && right != nullptr ? right[sample] : leftIn;
        left[sample] = cores[0].processSample(leftIn, params);
        if (right != nullptr)
            right[sample] = cores[1].processSample(rightIn, params);
    }
}

void DeltaSpineAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void DeltaSpineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid() && state.hasType(parameters.state.getType()))
            parameters.replaceState(state);
    }
}

void DeltaSpineAudioProcessor::copyDeltaSnapshot(deltaspine::dsp::DeltaSnapshot& destination) const noexcept
{
    deltaspine::dsp::DeltaSnapshot left;
    deltaspine::dsp::DeltaSnapshot right;
    cores[0].copySnapshot(left);
    cores[1].copySnapshot(right);

    destination = left;
    for (std::size_t i = 0; i < destination.cells.size(); ++i)
        destination.cells[i] = std::max(left.cells[i], right.cells[i]);
    destination.inputRms = 0.5f * (left.inputRms + right.inputRms);
    destination.wetRms = 0.5f * (left.wetRms + right.wetRms);
    destination.step = 0.5f * (left.step + right.step);
    destination.predictor = 0.5f * (left.predictor + right.predictor);
    destination.warning = left.warning || right.warning;
    destination.mode = left.mode;
}

juce::AudioProcessorEditor* DeltaSpineAudioProcessor::createEditor()
{
    return new DeltaSpineAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeltaSpineAudioProcessor();
}
