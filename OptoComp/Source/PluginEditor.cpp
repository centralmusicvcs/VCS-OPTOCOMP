#include "PluginEditor.h"

//==============================================================================
void OptoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float startAngle, float endAngle,
                                        juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float radius = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Gradatii in jurul butonului
    g.setColour (juce::Colour (0xff222222));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = radius * 0.80f;
        const float r2 = radius * (i % 5 == 0 ? 0.98f : 0.90f);
        const float s = std::sin (a), co = std::cos (a);
        g.drawLine (c.x + r1 * s, c.y - r1 * co, c.x + r2 * s, c.y - r2 * co,
                    i % 5 == 0 ? 2.0f : 1.2f);
    }

    const float kr = radius * 0.70f;

    // Umbra
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr + 2.0f, c.y - kr + 4.0f, kr * 2.0f, kr * 2.0f);

    // Corpul butonului (negru, lucios)
    juce::ColourGradient body (juce::Colour (0xff4a4a4a), c.x - kr * 0.4f, c.y - kr * 0.5f,
                               juce::Colour (0xff0e0e0e), c.x + kr * 0.6f, c.y + kr * 0.7f, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);

    // Inel interior
    g.setColour (juce::Colour (0xff2a2a2a));
    g.drawEllipse (c.x - kr * 0.78f, c.y - kr * 0.78f, kr * 1.56f, kr * 1.56f, 1.5f);

    // Indicator alb
    juce::Path p;
    p.addRoundedRectangle (-2.0f, -kr * 0.92f, 4.0f, kr * 0.42f, 1.5f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (juce::Colours::white);
    g.fillPath (p);
}

void OptoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto b = button.getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (juce::Colour (0xff555555));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.5f);

    const float d = b.getHeight() - 6.0f;
    const float x = button.getToggleState() ? b.getRight() - d - 3.0f : b.getX() + 3.0f;
    const float y = b.getY() + 3.0f;

    juce::ColourGradient metal (juce::Colour (0xfff4f4f4), x, y,
                                juce::Colour (0xff8a8a8a), x, y + d, false);
    g.setGradientFill (metal);
    g.fillEllipse (x, y, d, d);
}

//==============================================================================
OptoCompEditor::OptoCompEditor (OptoCompProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    auto setup = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
        s.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (s);
    };

    setup (gainKnob);
    setup (peakKnob);
    setup (colorKnob);
    setup (mixKnob);

    modeSwitch.setClickingTogglesState (true);
    addAndMakeVisible (modeSwitch);

    gainA  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "gain",  gainKnob);
    peakA  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "peak",  peakKnob);
    colorA = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "color", colorKnob);
    mixA   = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "mix",   mixKnob);
    modeA  = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "limit", modeSwitch);

    setSize (680, 350);
    startTimerHz (30);
}

OptoCompEditor::~OptoCompEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OptoCompEditor::resized()
{
    gainKnob.setBounds  (40, 55, 150, 150);
    peakKnob.setBounds  (490, 55, 150, 150);
    colorKnob.setBounds (150, 245, 70, 70);
    mixKnob.setBounds   (460, 245, 70, 70);
    modeSwitch.setBounds (310, 272, 60, 28);
}

void OptoCompEditor::timerCallback()
{
    const float gr = juce::jlimit (0.0f, 20.0f, proc.grMeter.load());
    // Ac cu inertie: urca repede, coboara mai lent (ca un VU real)
    needle += (gr > needle ? 0.35f : 0.10f) * (gr - needle);
    repaint (meterBounds.expanded (10.0f).toNearestInt());
}

//==============================================================================
void OptoCompEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const juce::Colour ink (0xff1d1d1d);

    // Panou argintiu periat
    juce::ColourGradient bg (juce::Colour (0xffdcdcd8), 0.0f, 0.0f,
                             juce::Colour (0xffb4b4af), 0.0f, b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    for (int yy = 0; yy < getHeight(); yy += 2)
    {
        g.setColour (juce::Colours::white.withAlpha ((float) ((yy * 37) % 11) / 90.0f));
        g.drawHorizontalLine (yy, 0.0f, b.getWidth());
    }

    g.setColour (juce::Colour (0xff6e6e6a));
    g.drawRect (b, 3.0f);

    // Suruburi
    auto screw = [&] (float sx, float sy)
    {
        g.setColour (juce::Colour (0xff8e8e8a));
        g.fillEllipse (sx - 7.0f, sy - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colour (0xff4a4a48));
        g.drawEllipse (sx - 7.0f, sy - 7.0f, 14.0f, 14.0f, 1.2f);
        g.drawLine (sx - 4.0f, sy - 1.0f, sx + 4.0f, sy + 1.0f, 1.6f);
    };
    screw (16.0f, 16.0f);
    screw (b.getWidth() - 16.0f, 16.0f);
    screw (16.0f, b.getHeight() - 16.0f);
    screw (b.getWidth() - 16.0f, b.getHeight() - 16.0f);

    // Titlu
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("OPTO LEVELING COMPRESSOR", juce::Rectangle<int> (150, 12, 380, 22),
                juce::Justification::centred, false);

    // ---- VU-metru ----
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (meterBounds.expanded (9.0f), 7.0f);
    g.setColour (juce::Colour (0xfff2e8c9));
    g.fillRoundedRectangle (meterBounds, 3.0f);

    g.saveState();
    g.reduceClipRegion (meterBounds.toNearestInt());

    const juce::Point<float> pivot (meterBounds.getCentreX(), meterBounds.getBottom() + 30.0f);
    const float R = 125.0f;
    const float aMin = -0.62f, aMax = 0.62f;

    for (int i = 0; i <= 20; ++i)
    {
        const float a = aMin + ((float) i / 20.0f) * (aMax - aMin);
        const float s = std::sin (a), co = std::cos (a);
        const float len = (i % 5 == 0) ? 12.0f : 6.0f;
        g.setColour (i >= 15 ? juce::Colour (0xffb02a1e) : juce::Colour (0xff222222));
        g.drawLine (pivot.x + R * s, pivot.y - R * co,
                    pivot.x + (R - len) * s, pivot.y - (R - len) * co,
                    (i % 5 == 0) ? 1.8f : 1.0f);

        if (i % 5 == 0)
        {
            const float tr = R - 24.0f;
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (juce::String (i),
                        juce::Rectangle<int> ((int) (pivot.x + tr * s) - 12,
                                              (int) (pivot.y - tr * co) - 7, 24, 14),
                        juce::Justification::centred, false);
        }
    }

    g.setColour (juce::Colour (0xff222222));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("GAIN REDUCTION dB",
                juce::Rectangle<int> ((int) meterBounds.getX(), (int) meterBounds.getBottom() - 22,
                                      (int) meterBounds.getWidth(), 14),
                juce::Justification::centred, false);

    // Acul
    const float na = aMin + (juce::jlimit (0.0f, 20.0f, needle) / 20.0f) * (aMax - aMin);
    const float ns = std::sin (na), nc = std::cos (na);
    g.setColour (juce::Colours::black);
    g.drawLine (pivot.x + 20.0f * ns, pivot.y - 20.0f * nc,
                pivot.x + (R + 5.0f) * ns, pivot.y - (R + 5.0f) * nc, 1.8f);

    // Reflexie pe sticla
    juce::ColourGradient glass (juce::Colours::white.withAlpha (0.28f), 0.0f, meterBounds.getY(),
                                juce::Colours::white.withAlpha (0.0f), 0.0f,
                                meterBounds.getY() + meterBounds.getHeight() * 0.55f, false);
    g.setGradientFill (glass);
    g.fillRect (meterBounds);

    g.restoreState();

    // ---- Etichete ----
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));

    auto label = [&] (const juce::String& t, juce::Rectangle<int> r, juce::Justification j)
    {
        g.drawText (t, r, j, false);
    };

    label ("GAIN",           { 40, 208, 150, 20 },  juce::Justification::centred);
    label ("PEAK REDUCTION", { 465, 208, 200, 20 }, juce::Justification::centred);

    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    label ("TUBE COLOR", { 135, 318, 100, 18 }, juce::Justification::centred);
    label ("MIX",        { 445, 318, 100, 18 }, juce::Justification::centred);
    label ("COMPRESS",   { 232, 276, 72, 20 },  juce::Justification::centredRight);
    label ("LIMIT",      { 376, 276, 60, 20 },  juce::Justification::centredLeft);
}
