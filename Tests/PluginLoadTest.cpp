#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_processors/format_types/juce_VST3PluginFormat.h>
#include <juce_events/juce_events.h>
#include <iostream>
#include <memory>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: DeltaSpinePluginLoadTest <plugin-path> <expected-name>\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, argv[1]);
    if (descriptions.isEmpty())
    {
        std::cerr << "no VST3 descriptions found in " << argv[1] << '\n';
        return 1;
    }

    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance(
        format.createInstanceFromDescription(*descriptions[0], 48000.0, 256, error));
    if (instance == nullptr)
    {
        std::cerr << "failed to instantiate VST3: " << error << '\n';
        return 1;
    }

    if (instance->getName() != argv[2])
    {
        std::cerr << "unexpected plugin name: " << instance->getName() << '\n';
        return 1;
    }

    instance->prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> audio(2, 256);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto sample = static_cast<float>(0.15 * std::sin(0.07 * static_cast<double>(i)));
        audio.setSample(0, i, sample);
        audio.setSample(1, i, sample);
    }
    juce::MidiBuffer midi;
    instance->processBlock(audio, midi);
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        for (int i = 0; i < audio.getNumSamples(); ++i)
            if (!std::isfinite(audio.getSample(ch, i)))
            {
                std::cerr << "non-finite VST3 output\n";
                return 1;
            }

    std::cout << "Loaded " << instance->getName() << " from " << argv[1] << '\n';
    return 0;
}

