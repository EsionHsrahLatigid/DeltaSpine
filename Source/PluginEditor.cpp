#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace
{
namespace design = ehl::juce_design;
}

DeltaSpineAudioProcessorEditor::DeltaSpineAudioProcessorEditor(DeltaSpineAudioProcessor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner)
{
    setLookAndFeel(&lookAndFeel);
    setName("DeltaSpine editor");
    setComponentID("deltaspine-editor");
    setTitle("DeltaSpine");
    setDescription("DeltaSpine monochrome 8-bit predictive coder editor");
    setWantsKeyboardFocus(true);

    matrix.setComponentID("deltaspine-delta-history");
    addAndMakeVisible(matrix);

    design::styleLabel(status);
    status.setComponentID("deltaspine-status");
    status.setJustificationType(juce::Justification::centredLeft);
    status.setColour(juce::Label::textColourId, design::Palette::mid());
    addAndMakeVisible(status);

    design::styleLabel(modeLabel);
    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setComponentID("deltaspine-label-mode");
    addAndMakeVisible(modeLabel);

    modeBox.setComponentID("deltaspine-mode");
    modeBox.addItem("DPCM", 1);
    modeBox.addItem("ADPCM", 2);
    modeBox.addItem("1BIT", 3);
    modeBox.setColour(juce::ComboBox::backgroundColourId, design::Palette::ink());
    modeBox.setColour(juce::ComboBox::textColourId, design::Palette::paper());
    modeBox.setColour(juce::ComboBox::outlineColourId, design::Palette::mid());
    modeBox.setColour(juce::ComboBox::arrowColourId, design::Palette::paper());
    addAndMakeVisible(modeBox);

    const juce::StringArray sliderNames { "PRED", "DEPTH", "ADAPT", "LEAK", "DENS", "MIX", "OUT" };
    for (std::size_t i = 0; i < sliders.size(); ++i)
        configureControl(sliders[i], labels[i], sliderNames[static_cast<int>(i)]);

    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::mode, modeBox);
    sliderAttachments[0] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::predict, sliders[0]);
    sliderAttachments[1] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::depth, sliders[1]);
    sliderAttachments[2] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::adapt, sliders[2]);
    sliderAttachments[3] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::leak, sliders[3]);
    sliderAttachments[4] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::density, sliders[4]);
    sliderAttachments[5] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::mix, sliders[5]);
    sliderAttachments[6] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        ownerProcessor.parameters, deltaspine::parameters::output, sliders[6]);

    setResizable(false, false);
    setSize(defaultWidth, defaultHeight);
    startTimerHz(24);
    updateReadout();
}

DeltaSpineAudioProcessorEditor::~DeltaSpineAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void DeltaSpineAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    design::paintEditorChrome(graphics, getLocalBounds(), "DeltaSpine", "PREDICTIVE CODER FRACTURE");

    auto bounds = getLocalBounds().withTrimmedTop(64).reduced(design::Metrics::margin, 0);
    auto matrixBounds = bounds.removeFromTop(116);
    graphics.setColour(design::Palette::low());
    graphics.fillRect(matrixBounds);
    graphics.setColour(design::Palette::mid());
    graphics.drawRect(matrixBounds, 1);

    auto statusBounds = bounds.removeFromTop(24).withTrimmedTop(8);
    graphics.setColour(design::Palette::ink());
    graphics.fillRect(statusBounds);
    graphics.setColour(design::Palette::mid());
    graphics.drawRect(statusBounds, 1);

    auto controls = bounds.withTrimmedTop(8).withTrimmedBottom(design::Metrics::margin);
    graphics.setColour(design::Palette::low());
    graphics.drawRect(controls, 1);
}

void DeltaSpineAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().withTrimmedTop(64).reduced(design::Metrics::margin, 0);
    matrix.setBounds(bounds.removeFromTop(116).reduced(8));
    status.setBounds(bounds.removeFromTop(24).withTrimmedTop(8).reduced(8, 0));
    auto controls = bounds.withTrimmedTop(8).withTrimmedBottom(design::Metrics::margin);

    constexpr auto gap = 4;
    constexpr auto labelH = 14;
    const auto rowH = controls.getHeight() / 2;
    auto top = controls.removeFromTop(rowH).reduced(8, 4);
    auto bottom = controls.reduced(8, 4);

    auto modeCell = top.removeFromLeft(top.getWidth() / 4).reduced(gap, 0);
    modeLabel.setBounds(modeCell.removeFromTop(labelH));
    modeBox.setBounds(modeCell.reduced(0, 4));

    const auto topW = top.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto cell = top.removeFromLeft(topW).reduced(gap, 0);
        labels[static_cast<std::size_t>(i)].setBounds(cell.removeFromTop(labelH));
        sliders[static_cast<std::size_t>(i)].setBounds(cell);
    }

    const auto bottomW = bottom.getWidth() / 4;
    for (int i = 3; i < 7; ++i)
    {
        auto cell = bottom.removeFromLeft(bottomW).reduced(gap, 0);
        labels[static_cast<std::size_t>(i)].setBounds(cell.removeFromTop(labelH));
        sliders[static_cast<std::size_t>(i)].setBounds(cell);
    }
}

void DeltaSpineAudioProcessorEditor::timerCallback()
{
    deltaspine::dsp::DeltaSnapshot snapshot;
    ownerProcessor.copyDeltaSnapshot(snapshot);
    matrix.setSnapshot(snapshot);
    updateReadout();
}

void DeltaSpineAudioProcessorEditor::updateReadout()
{
    deltaspine::dsp::DeltaSnapshot snapshot;
    ownerProcessor.copyDeltaSnapshot(snapshot);
    const auto mode = snapshot.mode == 0 ? "DPCM" : snapshot.mode == 1 ? "ADPCM" : "1BIT";
    const auto state = snapshot.warning ? "RESCUE" : "LIVE";
    status.setText(juce::String::formatted("%s   %s   IN %.4f   WET %.4f   STEP %.4f   PRED %.4f",
                                           state,
                                           mode,
                                           snapshot.inputRms,
                                           snapshot.wetRms,
                                           snapshot.step,
                                           snapshot.predictor),
                   juce::dontSendNotification);
}

void DeltaSpineAudioProcessorEditor::configureControl(juce::Slider& slider,
                                                      juce::Label& label,
                                                      const juce::String& text)
{
    label.setComponentID("deltaspine-label-" + text.toLowerCase());
    design::styleLabel(label);
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    slider.setComponentID("deltaspine-control-" + text.toLowerCase());
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 18);
    slider.setColour(juce::Slider::trackColourId, design::Palette::paper());
    slider.setColour(juce::Slider::backgroundColourId, design::Palette::low());
    slider.setColour(juce::Slider::thumbColourId, design::Palette::paper());
    slider.setColour(juce::Slider::textBoxTextColourId, design::Palette::paper());
    slider.setColour(juce::Slider::textBoxOutlineColourId, design::Palette::mid());
    addAndMakeVisible(slider);
}

void DeltaSpineAudioProcessorEditor::DeltaMatrix::setSnapshot(const deltaspine::dsp::DeltaSnapshot& next)
{
    snapshot = next;
    repaint();
}

void DeltaSpineAudioProcessorEditor::DeltaMatrix::paint(juce::Graphics& graphics)
{
    graphics.fillAll(design::Palette::ink());
    const auto area = getLocalBounds();
    const auto cellW = area.getWidth() / deltaspine::dsp::DeltaSnapshot::columns;
    const auto cellH = area.getHeight() / deltaspine::dsp::DeltaSnapshot::rows;

    for (int y = 0; y < deltaspine::dsp::DeltaSnapshot::rows; ++y)
    {
        for (int x = 0; x < deltaspine::dsp::DeltaSnapshot::columns; ++x)
        {
            const auto value = snapshot.cells[static_cast<std::size_t>(y * deltaspine::dsp::DeltaSnapshot::columns + x)];
            const auto cell = juce::Rectangle<int>(area.getX() + x * cellW,
                                                   area.getY() + y * cellH,
                                                   juce::jmax(1, cellW - 1),
                                                   juce::jmax(1, cellH - 1));
            graphics.setColour(value > 0.68f ? design::Palette::paper()
                              : value > 0.28f ? design::Palette::mid()
                              : design::Palette::low());
            if (value > 0.0f)
                graphics.fillRect(cell);
            else
                graphics.drawRect(cell, 1);
        }
    }

    if (snapshot.warning)
    {
        graphics.setColour(design::Palette::paper());
        for (int y = 0; y < area.getHeight(); y += 8)
            graphics.drawHorizontalLine(area.getY() + y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }

    if (hasKeyboardFocus(true))
    {
        graphics.setColour(design::Palette::paper());
        graphics.drawRect(area, 2);
    }
}

