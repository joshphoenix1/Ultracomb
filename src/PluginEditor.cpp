#include "PluginEditor.h"

using namespace juce;

//==============================================================================
UltraCombEditor::UltraCombEditor (UltraCombProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      xyPad (p.apvts)
{
    setLookAndFeel (&imperialLnf);
    setSize (750, 450);

    // --- Knobs ---
    auto setupRotary = [this] (Slider& s)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (s);
    };

    setupRotary (spreadSlider);
    setupRotary (polesSlider);
    setupRotary (densitySlider);

    // --- Dry/Wet vertical slider ---
    dryWetSlider.setSliderStyle (Slider::LinearVertical);
    dryWetSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (dryWetSlider);

    // --- Toggle buttons ---
    addAndMakeVisible (phaseButton);
    addAndMakeVisible (limiterButton);

    // --- XY Pad ---
    addAndMakeVisible (xyPad);

    // --- Attachments ---
    spreadAtt  = std::make_unique<SA> (p.apvts, "spread",      spreadSlider);
    polesAtt   = std::make_unique<SA> (p.apvts, "poles",        polesSlider);
    densityAtt = std::make_unique<SA> (p.apvts, "density",      densitySlider);
    dryWetAtt  = std::make_unique<SA> (p.apvts, "dryWet",       dryWetSlider);
    phaseAtt   = std::make_unique<BA> (p.apvts, "invertPhase",  phaseButton);
    limiterAtt = std::make_unique<BA> (p.apvts, "limiterOn",    limiterButton);

    // --- Timer for level meters (30 fps) ---
    startTimerHz (30);
}

UltraCombEditor::~UltraCombEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void UltraCombEditor::timerCallback()
{
    float newL = processor.peakLevelL.load (std::memory_order_relaxed);
    float newR = processor.peakLevelR.load (std::memory_order_relaxed);

    // Peak hold with decay
    displayLevelL = jmax (newL, displayLevelL * 0.92f);
    displayLevelR = jmax (newR, displayLevelR * 0.92f);

    // Only repaint the meter strips
    repaint (0, 36, 20, getHeight() - 46);
    repaint (getWidth() - 20, 36, 20, getHeight() - 46);
}

//==============================================================================
void UltraCombEditor::paint (Graphics& g)
{
    // Background
    g.fillAll (Colour (0xff0a0a0a));

    // Subtle top line
    g.setColour (Colour (0xff8b0000));
    g.fillRect (0, 0, getWidth(), 2);

    // ---- Title ----
    g.setColour (Colour (0xffcc0000));
    g.setFont (Font (20.0f, Font::bold));
    g.drawText ("ULTRACOMB SAL", getLocalBounds().removeFromTop (34),
                Justification::centred);

    // ---- Section geometry ----
    auto content = getLocalBounds();
    content.removeFromTop (38);
    content.removeFromBottom (10);
    content.removeFromLeft (22);
    content.removeFromRight (22);

    constexpr int sec1W = 170, sec3W = 150;
    constexpr int gap = 6;
    int sec2W = content.getWidth() - sec1W - sec3W - gap * 2;

    auto sec1 = content.removeFromLeft (sec1W);
    content.removeFromLeft (gap);
    auto sec2 = content.removeFromLeft (sec2W);
    content.removeFromLeft (gap);
    auto sec3 = content;

    // Section borders
    g.setColour (Colour (0xff8b0000));
    g.drawRoundedRectangle (sec1.toFloat(), 4.0f, 1.5f);
    g.drawRoundedRectangle (sec2.toFloat(), 4.0f, 1.5f);
    g.drawRoundedRectangle (sec3.toFloat(), 4.0f, 1.5f);

    // Section titles
    g.setColour (Colour (0xffcc0000));
    g.setFont (Font (10.0f, Font::bold));
    g.drawText ("INPUT & DISPERSION", sec1.getX(), sec1.getY() + 4, sec1.getWidth(), 14,
                Justification::centred);
    g.drawText ("THE COMB ENGINE", sec2.getX(), sec2.getY() + 4, sec2.getWidth(), 14,
                Justification::centred);
    g.drawText ("MASTER OUTPUT", sec3.getX(), sec3.getY() + 4, sec3.getWidth(), 14,
                Justification::centred);

    // ---- Knob labels ----
    g.setColour (Colour (0xffaaaaaa));
    g.setFont (10.0f);

    auto labelBelow = [&] (const Slider& s, const String& text)
    {
        auto b = s.getBounds();
        g.drawText (text, b.getX(), b.getBottom() + 1, b.getWidth(), 13,
                    Justification::centred);
    };

    labelBelow (spreadSlider,  "SPREAD");
    labelBelow (polesSlider,   "POLES");
    labelBelow (densitySlider, "DENSITY");

    // Dry/Wet labels
    auto dwb = dryWetSlider.getBounds();
    g.drawText ("WET",  dwb.getX() - 10, dwb.getY() - 14, dwb.getWidth() + 20, 12,
                Justification::centred);
    g.drawText ("DRY",  dwb.getX() - 10, dwb.getBottom() + 2, dwb.getWidth() + 20, 12,
                Justification::centred);

    // ---- Level meters ----
    drawMeter (g, 3,              38, 16, getHeight() - 52, displayLevelL);
    drawMeter (g, getWidth() - 19, 38, 16, getHeight() - 52, displayLevelR);

    // ---- Bottom branding ----
    g.setColour (Colour (0xff444444));
    g.setFont (9.0f);
    g.drawText ("HOMEBREW DSP", 0, getHeight() - 14, getWidth(), 12,
                Justification::centred);
}

//==============================================================================
void UltraCombEditor::drawMeter (Graphics& g, int x, int y, int w, int h, float level)
{
    // Track background
    g.setColour (Colour (0xff111111));
    g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 2.0f);

    float clipped = jlimit (0.0f, 1.0f, level);
    float fillH = clipped * (float) h;

    if (fillH < 1.0f)
        return;

    // Gradient: green (bottom) -> yellow -> red (top)
    g.saveState();
    g.reduceClipRegion (x, (int) ((float) (y + h) - fillH), w, (int) fillH + 1);

    ColourGradient grad (Colour (0xffff0000), (float) x, (float) y,
                         Colour (0xff00cc00), (float) x, (float) (y + h), false);
    grad.addColour (0.35, Colour (0xffff6600));
    grad.addColour (0.55, Colour (0xffffff00));
    g.setGradientFill (grad);
    g.fillRoundedRectangle ((float) x + 1.0f, (float) y,
                            (float) (w - 2), (float) h, 2.0f);

    g.restoreState();
}

//==============================================================================
void UltraCombEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (38);
    area.removeFromBottom (10);
    area.removeFromLeft (22);
    area.removeFromRight (22);

    constexpr int sec1W = 170, sec3W = 150;
    constexpr int gap = 6;
    int sec2W = area.getWidth() - sec1W - sec3W - gap * 2;

    auto sec1 = area.removeFromLeft (sec1W).reduced (15, 22);
    area.removeFromLeft (gap);
    auto sec2 = area.removeFromLeft (sec2W).reduced (12, 22);
    area.removeFromLeft (gap);
    auto sec3 = area.reduced (15, 22);

    // ---- Section 1: three knobs stacked vertically ----
    constexpr int knobSize = 70;
    constexpr int knobGap  = 16;   // gap includes label space
    int totalKnob = knobSize * 3 + knobGap * 2;
    int yOff = (sec1.getHeight() - totalKnob) / 2;

    auto placeKnob = [&] (Slider& s, int index)
    {
        int ky = sec1.getY() + yOff + index * (knobSize + knobGap);
        s.setBounds (sec1.getX() + (sec1.getWidth() - knobSize) / 2, ky,
                     knobSize, knobSize);
    };

    placeKnob (spreadSlider,  0);
    placeKnob (polesSlider,   1);
    placeKnob (densitySlider, 2);

    // ---- Section 2: XY pad + phase toggle ----
    auto xyArea = sec2.withTrimmedBottom (32);
    xyPad.setBounds (xyArea);

    phaseButton.setBounds (sec2.getX() + (sec2.getWidth() - 120) / 2,
                           sec2.getBottom() - 28, 120, 24);

    // ---- Section 3: Dry/Wet slider + limiter toggle ----
    auto sliderArea = sec3.withTrimmedBottom (32).withTrimmedTop (16);
    dryWetSlider.setBounds (sec3.getX() + (sec3.getWidth() - 30) / 2,
                            sliderArea.getY(), 30, sliderArea.getHeight());

    limiterButton.setBounds (sec3.getX() + (sec3.getWidth() - 110) / 2,
                             sec3.getBottom() - 28, 110, 24);
}
