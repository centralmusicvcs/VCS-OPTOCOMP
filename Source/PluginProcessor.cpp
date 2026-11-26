#include "PluginProcessor.h"
#include "PluginEditor.h"

OptoCompProcessor::OptoCompProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pPeak  = apvts.getRawParameterValue ("peak");
    pGain  = apvts.getRawParameterValue ("gain");
    pLimit = apvts.getRawParameterValue ("limit");
    pColor = apvts.getRawParameterValue ("color");
    pMix   = apvts.getRawParameterValue ("mix");

    setLatencySamples (0); // fara lookahead, fara oversampling => latenta zero
}

juce::AudioProcessorValueTreeState::ParameterLayout OptoCompProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "peak", 1 }, "Peak Reduction",
                                                  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
                                                  NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "limit", 1 }, "Mode",
                                                   StringArray { "Compress", "Limit" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "color", 1 }, "Tube Color",
                                                  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                  NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    return l;
}

void OptoCompProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;

    fastAtt = timeToCoef (10.0f, fs);   // atac ~10 ms
    fastRel = timeToCoef (60.0f, fs);   // release rapid ~60 ms
    slowAtt = timeToCoef (150.0f, fs);  // componenta lenta (memoria celulei)

    grFast = grSlow = 0.0f;
    dcX.fill (0.0f);
    dcY.fill (0.0f);

    sPeak.reset  (sampleRate, 0.02);
    sGain.reset  (sampleRate, 0.02);
    sColor.reset (sampleRate, 0.02);
    sMix.reset   (sampleRate, 0.02);

    sPeak.setCurrentAndTargetValue  (pPeak->load());
    sGain.setCurrentAndTargetValue  (pGain->load());
    sColor.setCurrentAndTargetValue (pColor->load());
    sMix.setCurrentAndTargetValue   (pMix->load());
}

bool OptoCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void OptoCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    sPeak.setTargetValue  (pPeak->load());
    sGain.setTargetValue  (pGain->load());
    sColor.setTargetValue (pColor->load());
    sMix.setTargetValue   (pMix->load());

    const bool limitMode = pLimit->load() > 0.5f;
    const float ratio = limitMode ? 10.0f : 3.0f;
    const float knee  = limitMode ? 6.0f  : 12.0f;
    const float slope = 1.0f - 1.0f / ratio;
    const float threshold = -2.0f; // dB; Peak Reduction ridica nivelul din sidechain

    // Release-ul lent depinde de cat a comprimat celula recent ("memorie")
    const float slowRelMs = 1500.0f + grSlow * 300.0f;
    const float slowRel = timeToCoef (slowRelMs, fs);

    float* data[2] = { buffer.getWritePointer (0),
                       numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    float grMax = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float peak  = sPeak.getNextValue();
        const float gainDb = sGain.getNextValue();
        const float color = sColor.getNextValue();
        const float mix   = sMix.getNextValue();

        // Detector sidechain (stereo-linked)
        float det = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            det = juce::jmax (det, std::abs (data[ch][n]));

        const float lvlDb = juce::Decibels::gainToDecibels (det, -100.0f) + peak * 0.4f;
        const float over = lvlDb - threshold;

        // Curba statica cu knee moale
        float target = 0.0f;
        if (2.0f * over > knee)
            target = slope * over;
        else if (2.0f * over > -knee)
        {
            const float x = over + knee * 0.5f;
            target = slope * x * x / (2.0f * knee);
        }

        // Celula T4: doua stari
        grFast = (target > grFast ? fastAtt : fastRel) * grFast
                 + (1.0f - (target > grFast ? fastAtt : fastRel)) * target;
        grSlow = (target > grSlow ? slowAtt : slowRel) * grSlow
                 + (1.0f - (target > grSlow ? slowAtt : slowRel)) * target;

        const float grEff = juce::jmax (grFast, 0.7f * grSlow);
        grMax = juce::jmax (grMax, grEff);

        const float g = juce::Decibels::decibelsToGain (-grEff + gainDb);

        // Saturatie asimetrica (armonica 2) + blocker DC
        const float drive = 1.0f + 2.0f * color;
        const float bias = 0.1f * color;
        const float biasOut = std::tanh (bias);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = data[ch][n];
            float y = dry * g;

            if (color > 0.001f)
            {
                y = (std::tanh (y * drive + bias) - biasOut) / drive;
                const float out = y - dcX[(size_t) ch] + 0.995f * dcY[(size_t) ch];
                dcX[(size_t) ch] = y;
                dcY[(size_t) ch] = out;
                y = out;
            }

            data[ch][n] = dry * (1.0f - mix) + y * mix;
        }
    }

    grMeter.store (grMax);
}

juce::AudioProcessorEditor* OptoCompProcessor::createEditor()
{
    return new OptoCompEditor (*this);
}

void OptoCompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OptoCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OptoCompProcessor();
}
