#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class SpringBackSlider : public juce::Slider
{
public:
    bool springBackEnabled = false;
    double restingValue    = 0.0;

    void mouseUp (const juce::MouseEvent& event) override
    {
        juce::Slider::mouseUp (event);
        if (springBackEnabled)
            setValue (restingValue, juce::sendNotificationSync);
    }
};

class TopBarPresetButton : public juce::TextButton
{
public:
    std::function<void()> onRightClick;

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown() && onRightClick != nullptr)
        {
            onRightClick();
            return;
        }
        juce::TextButton::mouseUp (e);
    }
};

class AssignableKnobSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown() && onRightClick != nullptr)
        {
            onRightClick();
            return;
        }
        juce::Slider::mouseDown (e);
    }
};
