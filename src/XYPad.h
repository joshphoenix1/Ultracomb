#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// X-Y pad: X = Frequency Shift, Y = Feedback
//==============================================================================
class XYPad : public juce::Component
{
public:
    XYPad (juce::AudioProcessorValueTreeState& state)
    {
        xAttach = std::make_unique<juce::ParameterAttachment> (
            *state.getParameter ("freqShift"),
            [this] (float v) { xValue = v; repaint(); },
            nullptr);

        yAttach = std::make_unique<juce::ParameterAttachment> (
            *state.getParameter ("feedback"),
            [this] (float v) { yValue = v; repaint(); },
            nullptr);

        xAttach->sendInitialUpdate();
        yAttach->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour (juce::Colour (0xff060606));
        g.fillRoundedRectangle (b, 4.0f);

        // Border
        g.setColour (juce::Colour (0xff8b0000));
        g.drawRoundedRectangle (b, 4.0f, 1.0f);

        // Grid lines
        g.setColour (juce::Colour (0x25ff0000));
        for (int i = 1; i < 8; ++i)
        {
            float fx = b.getX() + b.getWidth() * (float) i / 8.0f;
            float fy = b.getY() + b.getHeight() * (float) i / 8.0f;
            g.drawVerticalLine   ((int) fx, b.getY(), b.getBottom());
            g.drawHorizontalLine ((int) fy, b.getX(), b.getRight());
        }

        // Centre crosshair (zero-shift line)
        g.setColour (juce::Colour (0x40ff0000));
        float midX = b.getCentreX();
        g.drawVerticalLine ((int) midX, b.getY(), b.getBottom());

        // Cursor position
        float normX = (xValue - kXMin) / (kXMax - kXMin);
        float normY = 1.0f - (yValue - kYMin) / (kYMax - kYMin);
        float cx = b.getX() + normX * b.getWidth();
        float cy = b.getY() + normY * b.getHeight();

        // Crosshair lines
        g.setColour (juce::Colour (0x60ff0000));
        g.drawLine (cx, b.getY(), cx, b.getBottom(), 1.0f);
        g.drawLine (b.getX(), cy, b.getRight(), cy, 1.0f);

        // Cursor dot with glow
        g.setColour (juce::Colour (0x40ff0000));
        g.fillEllipse (cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
        g.setColour (juce::Colour (0xffff0000));
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

        // Axis labels
        g.setColour (juce::Colour (0x80cccccc));
        g.setFont (9.0f);
        g.drawText ("FREQ SHIFT", b.removeFromBottom (12.0f), juce::Justification::centred);

        // Vertical label (draw rotated)
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (
            -juce::MathConstants<float>::halfPi, b.getX() + 8.0f, b.getCentreY()));
        g.drawText ("FEEDBACK", (int) (b.getX() - 20), (int) (b.getCentreY() - 30), 60, 12,
                    juce::Justification::centred);
        g.restoreState();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        xAttach->beginGesture();
        yAttach->beginGesture();
        updateFromMouse (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        updateFromMouse (e);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        xAttach->endGesture();
        yAttach->endGesture();
    }

private:
    void updateFromMouse (const juce::MouseEvent& e)
    {
        auto b = getLocalBounds().toFloat();
        float normX = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
        float normY = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - b.getY()) / b.getHeight());

        xAttach->setValueAsPartOfGesture (kXMin + normX * (kXMax - kXMin));
        yAttach->setValueAsPartOfGesture (kYMin + normY * (kYMax - kYMin));
    }

    std::unique_ptr<juce::ParameterAttachment> xAttach, yAttach;
    float xValue = 0.0f;
    float yValue = 0.3f;

    static constexpr float kXMin = -1000.0f, kXMax = 1000.0f;
    static constexpr float kYMin = 0.0f,     kYMax = 0.95f;
};
