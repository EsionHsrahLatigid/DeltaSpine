#pragma once

#include "PluginProcessor.h"
#include <ehl/juce_design/EhlDesign.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <memory>

class DeltaSpineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit DeltaSpineAudioProcessorEditor(DeltaSpineAudioProcessor&);
    ~DeltaSpineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = 520;
    static constexpr int defaultHeight = 332;

private:
    class DeltaMatrix final : public juce::Component
    {
    public:
        void setSnapshot(const deltaspine::dsp::DeltaSnapshot& next);
        void paint(juce::Graphics&) override;
    private:
        deltaspine::dsp::DeltaSnapshot snapshot;
    };

    void timerCallback() override;
    void updateReadout();
    void configureControl(juce::Slider& slider, juce::Label& label, const juce::String& text);

    DeltaSpineAudioProcessor& ownerProcessor;
    ehl::juce_design::LookAndFeel lookAndFeel;
    DeltaMatrix matrix;
    juce::Label status;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    std::array<juce::Slider, 7> sliders;
    std::array<juce::Label, 7> labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> sliderAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeltaSpineAudioProcessorEditor)
};

