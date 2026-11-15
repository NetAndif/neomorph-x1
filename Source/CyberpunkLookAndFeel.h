#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

class CyberpunkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CyberpunkLookAndFeel()
    {
        customTypeface = juce::Typeface::createSystemTypefaceFor (
            BinaryData::RajdhaniSemiBold_ttf,
            BinaryData::RajdhaniSemiBold_ttfSize
        );

        setDefaultSansSerifTypeface (customTypeface);

        // Standard-Textfarben auf #cccccc angleichen
        const auto textCol = juce::Colour (0xffcccccc);
        setColour (juce::Label::textColourId, textCol);
        setColour (juce::ComboBox::textColourId, textCol);
        setColour (juce::PopupMenu::textColourId, textCol);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB (10, 15, 24));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB (0, 240, 255).withAlpha (0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour::fromRGB (0, 240, 255));
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& /*font*/) override
    {
        return customTypeface;
    }

    // 1. Matrix- & Parameter-Drehregler (unipolar & bipolar)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                    static_cast<float> (width), static_cast<float> (height)).reduced (3.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const bool isBipolar = static_cast<bool> (slider.getProperties().getWithDefault ("isBipolar", false));

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float arcRadius = radius - 2.0f;

        // Äußerer Hintergrund-Track
        juce::Path bgArc;
        bgArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour::fromRGB (22, 32, 46).withAlpha (0.85f));
        g.strokePath (bgArc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Aktiver Wert-Arc (ab Mitte bei bipolar, ab Start bei unipolar)
        const float zeroAngle = isBipolar
                                    ? (rotaryStartAngle + rotaryEndAngle) * 0.5f
                                    : rotaryStartAngle;

        if (std::abs (angle - zeroAngle) > 0.01f)
        {
            juce::Path valArc;
            valArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                  std::min (zeroAngle, angle), std::max (zeroAngle, angle), true);

            g.setColour (accent.withAlpha (0.28f));
            g.strokePath (valArc, juce::PathStrokeType (4.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (accent);
            g.strokePath (valArc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Metallische Kappe innen
        const float capRadius = radius * 0.72f;
        juce::ColourGradient capGrad (juce::Colour::fromRGB (28, 38, 52), centre.x, centre.y - capRadius,
                                      juce::Colour::fromRGB (10, 15, 22), centre.x, centre.y + capRadius, false);
        g.setGradientFill (capGrad);
        g.fillEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        g.setColour (juce::Colour::fromRGB (55, 80, 105).withAlpha (0.7f));
        g.drawEllipse (centre.x - capRadius, centre.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // Leuchtender Zeigerstrich
        juce::Path pointer;
        const float ptrLength = capRadius * 0.85f;
        const float ptrThickness = 2.2f;
        pointer.addRoundedRectangle (-ptrThickness * 0.5f, -ptrLength, ptrThickness, ptrLength * 0.58f, 1.0f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        g.setColour (accent);
        g.fillPath (pointer);
    }

    // 2. Scene-Fader & Pitch-/Mod-Wheels
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           const juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                    static_cast<float> (width), static_cast<float> (height));
        const auto accent = slider.findColour (juce::Slider::thumbColourId);
        const bool isWheel = static_cast<bool> (slider.getProperties().getWithDefault ("isWheel", false));

        if (isWheel)
        {
            // Pitch- / Mod-Wheel Gehäuse
            const auto wheelRect = bounds.reduced (4.0f, 2.0f);
            g.setColour (juce::Colour::fromRGB (12, 18, 28).withAlpha (0.85f));
            g.fillRoundedRectangle (wheelRect, 5.0f);
            g.setColour (juce::Colour::fromRGB (45, 70, 95).withAlpha (0.7f));
            g.drawRoundedRectangle (wheelRect, 5.0f, 1.0f);

            // Riffelung auf dem Rad andeuten
            g.setColour (juce::Colour::fromRGB (35, 50, 70).withAlpha (0.5f));
            for (float ry = wheelRect.getY() + 6.0f; ry < wheelRect.getBottom() - 6.0f; ry += 5.0f)
                g.drawHorizontalLine (juce::roundToInt (ry), wheelRect.getX() + 3.0f, wheelRect.getRight() - 3.0f);

            // Leuchtender Positions-Balken auf dem Wheel
            const float barH = 8.0f;
            auto barRect = juce::Rectangle<float> (wheelRect.getX() + 2.0f,
                                                   juce::jlimit (wheelRect.getY() + 2.0f,
                                                                 wheelRect.getBottom() - barH - 2.0f,
                                                                 sliderPos - barH * 0.5f),
                                                   wheelRect.getWidth() - 4.0f,
                                                   barH);

            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (barRect.expanded (2.0f, 2.0f), 3.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (barRect, 2.5f);
        }
        else
        {
            // Scene-Fader Track
            const float trackW = 4.0f;
            auto trackRect = juce::Rectangle<float> (bounds.getCentreX() - trackW * 0.5f,
                                                     bounds.getY() + 8.0f,
                                                     trackW,
                                                     bounds.getHeight() - 16.0f);

            g.setColour (juce::Colour::fromRGB (10, 15, 22).withAlpha (0.9f));
            g.fillRoundedRectangle (trackRect, 2.0f);

            // Aktiver Bereich unterhalb des Fader-Knopfes
            const float clampedPos = juce::jlimit (trackRect.getY(), trackRect.getBottom(), sliderPos);
            auto activeTrack = trackRect.withTop (clampedPos);
            g.setColour (accent.withAlpha (0.75f));
            g.fillRoundedRectangle (activeTrack, 2.0f);

            // Fader-Kappe
            const float capW = bounds.getWidth() * 0.78f;
            const float capH = 16.0f;
            auto capRect = juce::Rectangle<float> (bounds.getCentreX() - capW * 0.5f,
                                                   clampedPos - capH * 0.5f,
                                                   capW,
                                                   capH);

            g.setColour (juce::Colour::fromRGB (18, 26, 38));
            g.fillRoundedRectangle (capRect, 3.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (capRect, 3.0f, 1.2f);

            // Leuchtende Mittellinie auf der Fader-Kappe
            g.drawHorizontalLine (juce::roundToInt (capRect.getCentreY()),
                                  capRect.getX() + 3.0f,
                                  capRect.getRight() - 3.0f);
        }
    }

    // 3. Wavetable-Dropdown (ComboBox)
    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                                    static_cast<float> (width),
                                                    static_cast<float> (height)).reduced (0.5f);

        g.setColour (juce::Colour::fromRGB (12, 18, 28).withAlpha (0.92f));
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (box.hasKeyboardFocus (true)
                         ? juce::Colour::fromRGB (0, 240, 255)
                         : juce::Colour::fromRGB (48, 75, 102).withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Kleiner Pfeil rechts
        const float arrowX = static_cast<float> (width) - 13.0f;
        const float arrowY = static_cast<float> (height) * 0.5f;
        juce::Path arrow;
        arrow.addTriangle (arrowX - 3.5f, arrowY - 2.0f,
                           arrowX + 3.5f, arrowY - 2.0f,
                           arrowX,        arrowY + 2.5f);

        g.setColour (juce::Colour (0xffcccccc));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (juce::FontOptions (juce::jlimit (10.5f, 13.5f, static_cast<float> (box.getHeight()) * 0.58f)));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
    }

    // 4. Popup-Menüs (Settings & Dropdown-Listen)
    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (13.5f));
    }

    // 5. Cyberpunk-Buttons (falls sichtbare TextButtons verwendet werden)
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool /*shouldDrawButtonAsDown*/) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const bool isOn   = button.getToggleState();
        const auto accent = button.findColour (juce::TextButton::buttonOnColourId);

        if (isOn)
        {
            g.setColour (accent.withAlpha (0.22f));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (accent);
            g.drawRoundedRectangle (bounds, 4.0f, 1.4f);
        }
        else
        {
            g.setColour (juce::Colour::fromRGB (14, 20, 30).withAlpha (shouldDrawButtonAsHighlighted ? 0.95f : 0.80f));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (juce::Colour::fromRGB (45, 70, 95).withAlpha (shouldDrawButtonAsHighlighted ? 0.9f : 0.6f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        const bool isOn   = button.getToggleState();
        const auto accent = button.findColour (juce::TextButton::buttonOnColourId);

        g.setColour (isOn ? accent : juce::Colour (0xffcccccc));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::Typeface::Ptr customTypeface;
};