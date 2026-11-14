#include "PluginProcessor.h"
#include "PluginEditor.h"

NeomorphX1AudioProcessorEditor::NeomorphX1AudioProcessorEditor (NeomorphX1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&cyberpunkLookAndFeel);

    // Haupt-Hintergrund aus BinaryData laden
    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::background_main_png,
                                                       BinaryData::background_main_pngSize);

    // Die 6 Tab-Overlays laden
    tabImages[0] = juce::ImageCache::getFromMemory (BinaryData::tab_oscillators_png, BinaryData::tab_oscillators_pngSize);
    tabImages[1] = juce::ImageCache::getFromMemory (BinaryData::tab_filter_png,      BinaryData::tab_filter_pngSize);
    tabImages[2] = juce::ImageCache::getFromMemory (BinaryData::tab_envelopes_png,   BinaryData::tab_envelopes_pngSize);
    tabImages[3] = juce::ImageCache::getFromMemory (BinaryData::tab_free_eg_png,     BinaryData::tab_free_eg_pngSize);
    tabImages[4] = juce::ImageCache::getFromMemory (BinaryData::tab_step_seq_png,    BinaryData::tab_step_seq_pngSize);
    tabImages[5] = juce::ImageCache::getFromMemory (BinaryData::tab_mod_matrix_png,  BinaryData::tab_mod_matrix_pngSize);

    // Unsichtbare Klick-Buttons über die 6 Tab-Reiter im Bild legen
    for (size_t i = 0; i < tabButtons.size(); ++i)
    {
        auto& btn = tabButtons[i];
        btn.setAlpha (0.0f);
        btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        btn.onClick = [this, i] { setActiveTab (static_cast<ActiveTab> (i)); };
        addAndMakeVisible (btn);
    }

    // Unsichtbarer Klick-Button über dem Settings-/Menü-Icon oben rechts
    settingsButton.setAlpha (0.0f);
    settingsButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // 10 Matrix-Drehregler konfigurieren & an APVTS binden
    for (size_t i = 0; i < matrixKnobs.size(); ++i)
    {
        auto& knob = matrixKnobs[i];
        knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB (0, 240, 255));
        knob.setDoubleClickReturnValue (true, 0.5);

        knob.onValueChange = [this, i]
        {
            const int pct = juce::roundToInt (matrixKnobs[i].getValue() * 100.0);
            hudStatusText = juce::String::formatted ("ASSIGN %d: %d%%", static_cast<int> (i + 1), pct);
            repaint();
        };

        addAndMakeVisible (knob);

        const auto paramID = "knob_" + juce::String (static_cast<int> (i + 1));
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, paramID, knob);
    }

    // 3 Scene-Fader konfigurieren & an APVTS binden
    const char* faderParamIDs[3] = { "scene_1", "scene_morph", "scene_2" };
    const char* faderLabels[3]   = { "SCENE 1", "MORPH", "SCENE 2" };
    const juce::Colour faderColours[3] = {
        juce::Colour::fromRGB (0, 240, 255),   // Cyan
        juce::Colour::fromRGB (255, 60, 180),  // Neon-Magenta für Morph
        juce::Colour::fromRGB (0, 240, 255)    // Cyan
    };

    for (size_t i = 0; i < sceneFaders.size(); ++i)
    {
        auto& fader = sceneFaders[i];
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setColour (juce::Slider::thumbColourId, faderColours[i]);
        fader.setDoubleClickReturnValue (true, i == 1 ? 0.0 : 0.8);

        const juce::String label (faderLabels[i]);
        fader.onValueChange = [this, i, label]
        {
            const int pct = juce::roundToInt (sceneFaders[i].getValue() * 100.0);
            hudStatusText = label + ": " + juce::String (pct) + "%";
            repaint();
        };

        addAndMakeVisible (fader);

        faderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, faderParamIDs[i], fader);
    }

    // Standardgröße (16:9 Verhältnis passend zum Artwork) + Skalierbarkeit
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (16.0 / 9.0);
    setResizeLimits (800, 450, 2200, 1238);
    setSize (1120, 630);
}

NeomorphX1AudioProcessorEditor::~NeomorphX1AudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NeomorphX1AudioProcessorEditor::setActiveTab (ActiveTab newTab)
{
    if (currentTab != newTab)
    {
        currentTab = newTab;
        repaint();
    }
}

void NeomorphX1AudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader ("NEOMORPH X1 // SYSTEM");
    menu.addItem (1, "Init Patch (Reset)");
    menu.addSeparator();

    juce::PopupMenu scaleMenu;
    scaleMenu.addItem (10, "80%  (896 x 504)");
    scaleMenu.addItem (11, "100% (1120 x 630)");
    scaleMenu.addItem (12, "125% (1400 x 788)");
    scaleMenu.addItem (13, "150% (1680 x 945)");
    menu.addSubMenu ("UI Scale", scaleMenu);

    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        menu.addSeparator();
        menu.addItem (20, "Audio / MIDI Settings...");
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton),
                        [this] (int result)
    {
        switch (result)
        {
            case 1:
                for (auto* param : audioProcessor.getParameters())
                    param->setValueNotifyingHost (param->getDefaultValue());
                hudStatusText = "INIT PATCH LOADED";
                repaint();
                break;
            case 10: setSize (896, 504);   break;
            case 11: setSize (1120, 630);  break;
            case 12: setSize (1400, 788);  break;
            case 13: setSize (1680, 945);  break;
            case 20:
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    holder->showAudioSettingsDialog();
                break;
            default:
                break;
        }
    });
}

void NeomorphX1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (10, 14, 22));

    auto bounds = getLocalBounds().toFloat();

    // 1. Haupt-Gehäuse zeichnen
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);

    // 2. Aktives Tab-Overlay im Mittelfenster zeichnen
    const auto& activeOverlay = tabImages[static_cast<size_t> (currentTab)];
    if (activeOverlay.isValid())
        g.drawImage (activeOverlay, bounds, juce::RectanglePlacement::stretchToFit);

    // 3. HUD-Status / Koordinaten-Anzeige unten links
    if (hudStatusText.isNotEmpty())
    {
        auto infoBox = juce::Rectangle<float> (12.0f, bounds.getHeight() - 32.0f, 340.0f, 22.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (infoBox, 4.0f);
        g.setColour (juce::Colour::fromRGB (0, 240, 255));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (hudStatusText, infoBox, juce::Justification::centred);
    }
}

void NeomorphX1AudioProcessorEditor::resized()
{
    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());

    // 1. Relative Positionen der 6 Tab-Reiter
    const float tabStartX = 0.305f;
    const float tabY      = 0.208f;
    const float tabH      = 0.036f;
    const float tabWidths[6] = { 0.092f, 0.068f, 0.082f, 0.068f, 0.105f, 0.115f };

    float currentX = tabStartX;
    for (size_t i = 0; i < tabButtons.size(); ++i)
    {
        tabButtons[i].setBounds (
            juce::roundToInt (currentX * w),
            juce::roundToInt (tabY * h),
            juce::roundToInt (tabWidths[i] * w),
            juce::roundToInt (tabH * h)
        );
        currentX += tabWidths[i];
    }

    // 2. Settings-Icon oben rechts
    const float iconSize = 0.032f * w;
    settingsButton.setBounds (
        juce::roundToInt (0.9515f * w - iconSize * 0.5f),
        juce::roundToInt (0.0343f * h - iconSize * 0.5f),
        juce::roundToInt (iconSize),
        juce::roundToInt (iconSize)
    );

    // 3. Die 10 Matrix-Drehregler (exakt aus deinen Messwerten)
    constexpr std::array<std::pair<float, float>, 10> knobCoords = {{
        { 0.8490f, 0.2054f }, { 0.9175f, 0.2087f },
        { 0.8496f, 0.3413f }, { 0.9181f, 0.3424f },
        { 0.8496f, 0.4750f }, { 0.9187f, 0.4772f },
        { 0.8502f, 0.6217f }, { 0.9181f, 0.6261f },
        { 0.8490f, 0.7750f }, { 0.9175f, 0.7728f }
    }};

    const float knobDiameter = 0.056f * w;
    for (size_t i = 0; i < matrixKnobs.size(); ++i)
    {
        const float cx = knobCoords[i].first  * w;
        const float cy = knobCoords[i].second * h;
        matrixKnobs[i].setBounds (
            juce::roundToInt (cx - knobDiameter * 0.5f),
            juce::roundToInt (cy - knobDiameter * 0.5f),
            juce::roundToInt (knobDiameter),
            juce::roundToInt (knobDiameter)
        );
    }

    // 4. Die 3 Scene-Fader (aus deinen oberen/unteren Schlitz-Messwerten)
    constexpr float faderX[3] = { 0.2823f, 0.3542f, 0.4265f };
    const float faderTopY     = 0.6148f * h - 10.0f; // +10px Puffer für die Kappe oben
    const float faderBottomY  = 0.8185f * h + 10.0f; // +10px Puffer für die Kappe unten
    const float faderHeight   = faderBottomY - faderTopY;
    const float faderWidth    = 0.036f * w;

    for (size_t i = 0; i < sceneFaders.size(); ++i)
    {
        const float cx = faderX[i] * w;
        sceneFaders[i].setBounds (
            juce::roundToInt (cx - faderWidth * 0.5f),
            juce::roundToInt (faderTopY),
            juce::roundToInt (faderWidth),
            juce::roundToInt (faderHeight)
        );
    }
}

void NeomorphX1AudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        const float relX = static_cast<float> (event.x) / static_cast<float> (getWidth());
        const float relY = static_cast<float> (event.y) / static_cast<float> (getHeight());

        const auto coordPair = juce::String::formatted ("{ %.4ff, %.4ff },", relX, relY);
        juce::SystemClipboard::copyTextToClipboard (coordPair);

        hudStatusText = "Kopiert: " + coordPair;
        repaint();
    }
}