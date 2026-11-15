#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class CyberpunkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CyberpunkLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour::fromRGB (14, 20, 32));
        setColour (juce::PopupMenu::textColourId,                  juce::Colour::fromRGB (210, 245, 255));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB (0, 180, 210));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour::fromRGB (8, 12, 20));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre  = bounds.getCentre();
        const float angle  = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto accentColour = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // 1. Hintergrund-Skalenring
        const float arcRadius = radius * 0.88f;
        const float lineW     = juce::jmax (2.2f, radius * 0.11f);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                                     0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour::fromRGB (25, 36, 52));
        g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Aktiver Leucht-Bogen
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                                    0.0f, rotaryStartAngle, angle, true);

            g.setColour (accentColour.withAlpha (0.28f));
            g.strokePath (valueArc, juce::PathStrokeType (lineW * 2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (accentColour);
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Regler-Kappe
        const float capRadius = radius * 0.66f;
        juce::ColourGradient capGrad (juce::Colour::fromRGB (42, 50, 66), centre.x, centre.y - capRadius,
                                      juce::Colour::fromRGB (16, 20, 28), centre.x, centre.y + capRadius, false);
        g.setGradientFill (capGrad);
        g.fillEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        g.setColour (juce::Colour::fromRGB (70, 90, 115).withAlpha (0.6f));
        g.drawEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // 4. Zeiger-Strich
        juce::Path pointer;
        const float pointerLength = capRadius * 0.72f;
        const float pointerThickness = juce::jmax (2.0f, radius * 0.09f);
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -capRadius + 2.5f,
                                     pointerThickness, pointerLength, 1.0f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        g.setColour (accentColour);
        g.fillPath (pointer);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        if (static_cast<bool> (slider.getProperties().getWithDefault ("isWheel", false)))
        {
            drawWheelSlider (g, x, y, width, height, sliderPos, slider);
            return;
        }

        // --- Standard Scene-Fader ---
        const auto bounds  = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float centreX = bounds.getCentreX();
        const auto accentColour = slider.findColour (juce::Slider::thumbColourId);

        const float trackW = 4.0f;
        const auto trackRect = juce::Rectangle<float> (centreX - trackW * 0.5f, bounds.getY() + 6.0f,
                                                       trackW, bounds.getHeight() - 12.0f);
        g.setColour (juce::Colour::fromRGB (10, 14, 22));
        g.fillRoundedRectangle (trackRect, 2.0f);
        g.setColour (juce::Colour::fromRGB (50, 70, 95).withAlpha (0.6f));
        g.drawRoundedRectangle (trackRect, 2.0f, 1.0f);

        const float fillTop = juce::jlimit (trackRect.getY(), trackRect.getBottom(), sliderPos);
        const auto activeTrack = juce::Rectangle<float> (trackRect.getX(), fillTop,
                                                         trackW, trackRect.getBottom() - fillTop);
        g.setColour (accentColour.withAlpha (0.85f));
        g.fillRoundedRectangle (activeTrack, 2.0f);

        const float capW = juce::jmin (bounds.getWidth() * 0.78f, 34.0f);
        const float capH = 16.0f;
        const auto capRect = juce::Rectangle<float> (centreX - capW * 0.5f, sliderPos - capH * 0.5f, capW, capH);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (capRect.translated (0.0f, 2.0f), 3.0f);

        juce::ColourGradient capGrad (juce::Colour::fromRGB (52, 62, 80), capRect.getX(), capRect.getY(),
                                      juce::Colour::fromRGB (20, 26, 36), capRect.getX(), capRect.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillRoundedRectangle (capRect, 3.0f);

        g.setColour (juce::Colour::fromRGB (95, 120, 150).withAlpha (0.75f));
        g.drawRoundedRectangle (capRect, 3.0f, 1.0f);

        g.setColour (accentColour);
        g.fillRect (capRect.getX() + 3.0f, sliderPos - 1.0f, capW - 6.0f, 2.0f);
    }

private:
    void drawWheelSlider (juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, juce::Slider& slider)
    {
        const auto bounds       = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const auto accentColour = slider.findColour (juce::Slider::thumbColourId);

        // 1. Vertiefter Schacht für das Rad
        g.setColour (juce::Colour::fromRGB (8, 11, 18));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colour::fromRGB (45, 65, 90).withAlpha (0.75f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f);

        // 2. Zylindrischer Rad-Körper (3D-Verlauf oben dunkel -> Mitte hell -> unten dunkel)
        const auto wheelRect = bounds.reduced (3.0f, 4.0f);
        juce::ColourGradient cylGrad (juce::Colour::fromRGB (14, 18, 26), wheelRect.getX(), wheelRect.getY(),
                                      juce::Colour::fromRGB (14, 18, 26), wheelRect.getX(), wheelRect.getBottom(), false);
        cylGrad.addColour (0.5, juce::Colour::fromRGB (46, 56, 74));
        g.setGradientFill (cylGrad);
        g.fillRoundedRectangle (wheelRect, 3.5f);

        // 3. Perspektivisch mitlaufende Riffelung auf der Walze
        const float normPos = static_cast<float> ((slider.getValue() - slider.getMinimum())
                                                  / (slider.getMaximum() - slider.getMinimum()));
        const float wheelH  = wheelRect.getHeight();
        const float centreY = wheelRect.getCentreY();
        const float halfH   = wheelH * 0.5f;

        // Phasenverschiebung durch die aktuelle Radstellung
        const float phaseOffset = (0.5f - normPos) * 1.4f;
        const int numGrooves    = 11;

        for (int i = -numGrooves; i <= numGrooves; ++i)
        {
            const float angle = (static_cast<float> (i) / static_cast<float> (numGrooves)) * 1.5f + phaseOffset;
            if (angle > -1.45f && angle < 1.45f)
            {
                const float grooveY = centreY + std::sin (angle) * halfH;
                const float depth   = std::cos (angle); // 1.0 in der Mitte, 0.0 am Rand

                g.setColour (juce::Colours::black.withAlpha (0.45f * depth));
                g.drawLine (wheelRect.getX() + 2.0f, grooveY, wheelRect.getRight() - 2.0f, grooveY, 1.2f);

                g.setColour (juce::Colour::fromRGB (110, 135, 165).withAlpha (0.22f * depth));
                g.drawLine (wheelRect.getX() + 2.0f, grooveY + 1.2f, wheelRect.getRight() - 2.0f, grooveY + 1.2f, 1.0f);
            }
        }

        // 4. Leuchtende Griff-Kerbe an der aktuellen Position
        const float clampedY = juce::jlimit (wheelRect.getY() + 5.0f, wheelRect.getBottom() - 5.0f, sliderPos);
        const auto barRect   = juce::Rectangle<float> (wheelRect.getX() + 2.0f, clampedY - 2.5f,
                                                       wheelRect.getWidth() - 4.0f, 5.0f);

        // Außen-Glow
        g.setColour (accentColour.withAlpha (0.35f));
        g.fillRoundedRectangle (barRect.expanded (1.5f, 2.5f), 3.0f);

        // Leucht-Kern
        g.setColour (accentColour);
        g.fillRoundedRectangle (barRect, 2.0f);
    }
};