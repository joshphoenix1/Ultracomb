#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
UltraCombProcessor::UltraCombProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout UltraCombProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // --- Input & Dispersion ---
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "spread", 1 }, "Spread",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "poles", 1 }, "Poles", 1, 6, 4));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "density", 1 }, "Density",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // --- Comb Engine ---
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "freqShift", 1 }, "Freq Shift",
        NormalisableRange<float> (-1000.0f, 1000.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.3f));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "invertPhase", 1 }, "Phase Invert", false));

    // --- Master Output ---
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "dryWet", 1 }, "Dry/Wet",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "limiterOn", 1 }, "Limiter", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void UltraCombProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    for (auto& ch : channels)
    {
        ch.reset();

        // Initialise Hilbert transform coefficients
        for (int i = 0; i < kHilbertOrder; ++i)
        {
            ch.hiltA[i].coeff = kHiltA[i];
            ch.hiltB[i].coeff = kHiltB[i];
        }
    }
}

void UltraCombProcessor::releaseResources()
{
    for (auto& ch : channels)
        ch.reset();
}

bool UltraCombProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != AudioChannelSet::mono() && mainOut != AudioChannelSet::stereo())
        return false;
    return mainOut == layouts.getMainInputChannelSet();
}

//==============================================================================
void UltraCombProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // --- Read parameters (atomic, once per block) ---
    const float spread     = apvts.getRawParameterValue ("spread")     ->load();
    const int   poles      = jlimit (1, kMaxPoles,
                               (int) std::round (apvts.getRawParameterValue ("poles")->load()));
    const float density    = apvts.getRawParameterValue ("density")    ->load();
    const float freqShift  = apvts.getRawParameterValue ("freqShift")  ->load();
    const float feedback   = apvts.getRawParameterValue ("feedback")   ->load();
    const bool  invertPh   = apvts.getRawParameterValue ("invertPhase")->load() >= 0.5f;
    const float dryWet     = apvts.getRawParameterValue ("dryWet")     ->load();
    const bool  limiterOn  = apvts.getRawParameterValue ("limiterOn")  ->load() >= 0.5f;

    // --- Disperser frequency placement ---
    // Centre = 1 kHz, spacing = 1..2.5x between filters, Q from density
    const float centreFreq    = 1000.0f;
    const float spacingFactor = 1.0f + spread * 1.5f;
    const float Q             = 0.5f + density * 7.5f;

    // --- Comb delay in samples (fixed ~3 ms) ---
    const int combDelay = jlimit (1, kMaxDelaySamples - 1,
                                  (int) (kCombDelayMs * 0.001f * (float) currentSampleRate));

    // --- Phase increment for frequency shifter ---
    const double twoPi   = MathConstants<double>::twoPi;
    const double phaseInc = twoPi * (double) freqShift / currentSampleRate;

    // --- Process each channel ---
    for (int ch = 0; ch < jmin (numChannels, 2); ++ch)
    {
        auto& state = channels[ch];
        float* data = buffer.getWritePointer (ch);

        // Update disperser coefficients for this block
        for (int i = 0; i < poles; ++i)
        {
            float filterFreq = centreFreq * std::pow (spacingFactor,
                                   (float) i - (float) (poles - 1) * 0.5f);
            filterFreq = jlimit (20.0f, (float) (currentSampleRate * 0.45), filterFreq);
            state.disperser[i].setParams (filterFreq, Q, (float) currentSampleRate);
        }

        float peak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            float dry = data[n];
            float wet = dry;

            // === 1. Disperser (all-pass chain) ===
            for (int i = 0; i < poles; ++i)
                wet = state.disperser[i].process (wet);

            // === 2. Feedback comb with frequency shifter ===
            // Read delayed output
            int readPos = state.delayWritePos - combDelay;
            if (readPos < 0) readPos += kMaxDelaySamples;
            float delayed = state.delayBuf[readPos];

            // Frequency-shift the delayed signal via Hilbert transform
            float I_sig = delayed;
            float Q_sig = delayed;
            for (int i = 0; i < kHilbertOrder; ++i)
            {
                I_sig = state.hiltA[i].process (I_sig);
                Q_sig = state.hiltB[i].process (Q_sig);
            }

            float cosP = (float) std::cos (state.oscPhase);
            float sinP = (float) std::sin (state.oscPhase);
            float shifted = I_sig * cosP - Q_sig * sinP;

            state.oscPhase += phaseInc;
            if (state.oscPhase > twoPi)       state.oscPhase -= twoPi;
            else if (state.oscPhase < 0.0)    state.oscPhase += twoPi;

            // Comb: output = dispersed input + feedback * shifted
            float combOut = wet + feedback * shifted;

            // Write to delay line
            state.delayBuf[state.delayWritePos] = combOut;
            state.delayWritePos = (state.delayWritePos + 1) % kMaxDelaySamples;

            wet = combOut;

            // === 3. Phase inversion ===
            if (invertPh)
                wet = -wet;

            // === 4. Dry/Wet mix ===
            float out = dry * (1.0f - dryWet) + wet * dryWet;

            // === 5. Limiter (soft clip) ===
            if (limiterOn)
                out = std::tanh (out);

            data[n] = out;
            peak = jmax (peak, std::abs (out));
        }

        if (ch == 0) peakLevelL.store (peak, std::memory_order_relaxed);
        else         peakLevelR.store (peak, std::memory_order_relaxed);
    }
}

//==============================================================================
AudioProcessorEditor* UltraCombProcessor::createEditor()
{
    return new UltraCombEditor (*this);
}

void UltraCombProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void UltraCombProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltraCombProcessor();
}
