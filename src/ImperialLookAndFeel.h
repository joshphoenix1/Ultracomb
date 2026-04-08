#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Dark imperial aesthetic: black body, red accents, Star Wars vibes
//==============================================================================
class ImperialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ImperialLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff0a0a0a));
        setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffcccccc));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                              (float) width, (float) height).reduced (4.0f);
        float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer ring — dark red
        g.setColour (juce::Colour (0xff8b0000));
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // Knob body — dark gradient
        juce::ColourGradient bodyGrad (juce::Colour (0xff2a2a2a), cx, cy - radius,
                                       juce::Colour (0xff0a0a0a), cx, cy + radius, false);
        g.setGradientFill (bodyGrad);
        float inner = radius * 0.82f;
        g.fillEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);

        // Indicator line — bright red
        float lineLen = radius * 0.65f;
        float lx = cx + lineLen * std::sin (angle);
        float ly = cy - lineLen * std::cos (angle);
        g.setColour (juce::Colour (0xffff0000));
        g.drawLine (cx, cy, lx, ly, 2.5f);

        // Centre dot
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    //--------------------------------------------------------------------------
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minPos*/, float /*maxPos*/,
                           juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style != juce::Slider::LinearVertical)
            return;

        float trackW = 6.0f;
        float trackX = (float) x + (float) width * 0.5f - trackW * 0.5f;

        // Track background
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRoundedRectangle (trackX, (float) y, trackW, (float) height, 3.0f);

        // Filled portion (from bottom up)
        float fillH = (float) (y + height) - sliderPos;
        if (fillH > 1.0f)
        {
            g.setColour (juce::Colour (0xffcc0000));
            g.fillRoundedRectangle (trackX, sliderPos, trackW, fillH, 3.0f);
        }

        // Thumb
        float thumbH = 16.0f;
        g.setColour (juce::Colour (0xffff0000));
        g.fillRoundedRectangle (trackX - 5.0f, sliderPos - thumbH * 0.5f,
                                trackW + 10.0f, thumbH, 4.0f);
    }

    //--------------------------------------------------------------------------
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawAsHighlighted, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float h = bounds.getHeight();

        // LED circle
        float ledSize = juce::jmin (h * 0.55f, 12.0f);
        float ledX = bounds.getX() + 4.0f;
        float ledY = bounds.getCentreY() - ledSize * 0.5f;

        if (button.getToggleState())
        {
            // Red glow
            g.setColour (juce::Colour (0x40ff0000));
            g.fillEllipse (ledX - 3.0f, ledY - 3.0f, ledSize + 6.0f, ledSize + 6.0f);
            g.setColour (juce::Colour (0xffff0000));
        }
        else
        {
            g.setColour (juce::Colour (0xff3a0000));
        }
        g.fillEllipse (ledX, ledY, ledSize, ledSize);

        // Label
        g.setColour (shouldDrawAsHighlighted ? juce::Colour (0xffffffff)
                                             : juce::Colour (0xffcccccc));
        g.setFont (11.0f);
        g.drawText (button.getButtonText(),
                    (int) (ledX + ledSize + 6.0f), 0,
                    (int) (bounds.getWidth() - ledSize - 14.0f), (int) h,
                    juce::Justification::centredLeft);
    }
};
