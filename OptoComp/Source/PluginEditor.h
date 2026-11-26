#pragma once

#include "PluginProcessor.h"

// Aspectul butoanelor si al comutatorului, desenat in cod
class OptoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};

class OptoCompEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit OptoCompEditor (OptoCompProcessor&);
    ~OptoCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;

    OptoCompProcessor& proc;
    OptoLookAndFeel laf;

    juce::Slider gainKnob, peakKnob, colorKnob, mixKnob;
    juce::ToggleButton modeSwitch;

    std::unique_ptr<APVTS::SliderAttachment> gainA, peakA, colorA, mixA;
    std::unique_ptr<APVTS::ButtonAttachment> modeA;

    float needle = 0.0f; // gain reduction afisat (dB), cu inertie
    juce::Rectangle<float> meterBounds { 240.0f, 45.0f, 200.0f, 115.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoCompEditor)
};
