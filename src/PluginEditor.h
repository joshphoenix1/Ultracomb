#pragma once

#include "PluginProcessor.h"
#include "ImperialLookAndFeel.h"
#include "XYPad.h"

class UltraCombEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit UltraCombEditor (UltraCombProcessor&);
    ~UltraCombEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, int x, int y, int w, int h, float level);

    UltraCombProcessor& processor;
    ImperialLookAndFeel imperialLnf;

    // --- Section 1: Input & Dispersion ---
    juce::Slider spreadSlider, polesSlider, densitySlider;

    // --- Section 2: The Comb Engine ---
    XYPad xyPad;
    juce::ToggleButton phaseButton { "PHASE INV" };

    // --- Section 3: Master Output ---
    juce::Slider dryWetSlider;
    juce::ToggleButton limiterButton { "LIMITER" };

    // --- Parameter attachments ---
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SA> spreadAtt, polesAtt, densityAtt, dryWetAtt;
    std::unique_ptr<BA> phaseAtt, limiterAtt;

    // --- Level meter display values ---
    float displayLevelL = 0.0f;
    float displayLevelR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UltraCombEditor)
};
