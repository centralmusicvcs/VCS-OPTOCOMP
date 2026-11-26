#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class OptoCompProcessor : public juce::AudioProcessor
{
public:
    OptoCompProcessor();
    ~OptoCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OptoComp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Poate fi citit din GUI pentru VU-metru (gain reduction in dB)
    std::atomic<float> grMeter { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static float timeToCoef (float ms, double fs)
    {
        return std::exp (-1.0f / (0.001f * ms * (float) fs));
    }

    // Parametri
    std::atomic<float>* pPeak  = nullptr;  // 0..100
    std::atomic<float>* pGain  = nullptr;  // -12..+24 dB
    std::atomic<float>* pLimit = nullptr;  // 0 = Compress, 1 = Limit
    std::atomic<float>* pColor = nullptr;  // 0..1
    std::atomic<float>* pMix   = nullptr;  // 0..1

    juce::SmoothedValue<float> sPeak, sGain, sColor, sMix;

    // Model celula optica T4 (doua stari, in dB de gain reduction)
    float grFast = 0.0f;
    float grSlow = 0.0f;
    float fastAtt = 0.0f, fastRel = 0.0f, slowAtt = 0.0f;
    double fs = 44100.0;

    // Blocker de DC dupa saturatie
    std::array<float, 2> dcX { 0.0f, 0.0f }, dcY { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoCompProcessor)
};
