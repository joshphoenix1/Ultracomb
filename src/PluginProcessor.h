#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// First-order all-pass filter for Hilbert transform
// H(z) = (a + z^-1) / (1 + a*z^-1)
struct FirstOrderAP
{
    float coeff = 0.0f;
    float state = 0.0f;

    void reset() { state = 0.0f; }

    float process (float x)
    {
        float y = coeff * x + state;
        state = x - coeff * y;
        return y;
    }
};

//==============================================================================
// Second-order all-pass biquad for disperser
// H(z) = (a2 + a1*z^-1 + z^-2) / (1 + a1*z^-1 + a2*z^-2)
struct SecondOrderAP
{
    float b0 = 1.0f, b1 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void setParams (float freq, float Q, float sampleRate)
    {
        float w0    = juce::MathConstants<float>::twoPi * freq / sampleRate;
        float alpha = std::sin (w0) / (2.0f * Q);
        float a0inv = 1.0f / (1.0f + alpha);
        b0 = (1.0f - alpha) * a0inv;   // = a2 normalised
        b1 = (-2.0f * std::cos (w0)) * a0inv; // = a1 normalised
        a1 = b1;
        a2 = b0;
    }

    void reset() { x1 = x2 = y1 = y2 = 0.0f; }

    float process (float x)
    {
        // b2 = 1.0 for all-pass
        float y = b0 * x + b1 * x1 + x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

//==============================================================================
class UltraCombProcessor : public juce::AudioProcessor
{
public:
    UltraCombProcessor();
    ~UltraCombProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Level meters (read by editor via timer)
    std::atomic<float> peakLevelL { 0.0f };
    std::atomic<float> peakLevelR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;

    static constexpr int kMaxPoles       = 6;
    static constexpr int kHilbertOrder   = 4;
    static constexpr int kMaxDelaySamples = 8192;
    static constexpr float kCombDelayMs  = 3.0f;

    // Broadband Hilbert transform coefficients (IIR allpass network)
    // Two parallel chains maintain ~90 deg phase difference from ~15 Hz to ~20 kHz
    static constexpr float kHiltA[kHilbertOrder] = {
        0.6923878f, 0.9360654f, 0.9882295f, 0.9987488f
    };
    static constexpr float kHiltB[kHilbertOrder] = {
        0.4021921f, 0.8561711f, 0.9722910f, 0.9952885f
    };

    struct ChannelState
    {
        // Disperser: chain of second-order all-pass filters
        SecondOrderAP disperser[kMaxPoles];

        // Hilbert transform branches (for frequency shifter)
        FirstOrderAP hiltA[kHilbertOrder];
        FirstOrderAP hiltB[kHilbertOrder];

        // Frequency shifter oscillator phase (double for precision over time)
        double oscPhase = 0.0;

        // Comb filter feedback delay line
        float delayBuf[kMaxDelaySamples] = {};
        int delayWritePos = 0;

        void reset()
        {
            for (auto& f : disperser) f.reset();
            for (auto& f : hiltA)     f.reset();
            for (auto& f : hiltB)     f.reset();
            oscPhase = 0.0;
            std::fill (std::begin (delayBuf), std::end (delayBuf), 0.0f);
            delayWritePos = 0;
        }
    };

    ChannelState channels[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UltraCombProcessor)
};
