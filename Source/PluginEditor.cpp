#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
    const juce::Colour gold      { 0xfff0b15f };
    const juce::Colour goldBright{ 0xffffcf7b };
    const juce::Colour bronze    { 0xff8a542b };
    const juce::Colour nearBlack { 0xff11100f };
    const juce::Colour panel     { 0xee171410 };

    juce::Image loadBinaryImageContaining(const juce::String& token)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String resourceName(BinaryData::namedResourceList[i]);
            if (!resourceName.containsIgnoreCase(token))
                continue;

            int size = 0;
            const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), size);
            if (data != nullptr && size > 0)
                return juce::ImageFileFormat::loadFrom(data, (size_t) size);
        }
        return {};
    }

    juce::String patternSymbol(int eventType)
    {
        switch (eventType)
        {
            case AcousticGuitarByJGKAudioProcessor::Down:       return juce::CharPointer_UTF8("\xE2\x86\x93");
            case AcousticGuitarByJGKAudioProcessor::Up:         return juce::CharPointer_UTF8("\xE2\x86\x91");
            case AcousticGuitarByJGKAudioProcessor::Palm:       return "M";
            case AcousticGuitarByJGKAudioProcessor::Choke:      return "X";
            case AcousticGuitarByJGKAudioProcessor::Scratch:    return "SCR";
            case AcousticGuitarByJGKAudioProcessor::Percussion: return "P";
            case AcousticGuitarByJGKAudioProcessor::Rest:
            default:                                             return "-";
        }
    }
}

class AcousticGuitarByJGKAudioProcessorEditor::JGKLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JGKLookAndFeel()
    {
        setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.96f));
        setColour(juce::ComboBox::backgroundColourId, nearBlack.withAlpha(0.94f));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::outlineColourId, bronze.withAlpha(0.95f));
        setColour(juce::ComboBox::arrowColourId, gold);
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.74f));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool selected = button.getToggleState();

        juce::ColourGradient grad(selected || down ? goldBright : juce::Colour(0xff2a2621),
                                  0.0f, r.getY(),
                                  selected || down ? juce::Colour(0xffa96529) : juce::Colour(0xff11100f),
                                  0.0f, r.getBottom(), false);
        if (highlighted && !selected)
        {
            grad = juce::ColourGradient(juce::Colour(0xff453529), 0.0f, r.getY(),
                                        juce::Colour(0xff17130f), 0.0f, r.getBottom(), false);
        }
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(selected ? goldBright : bronze.withAlpha(0.95f));
        g.drawRoundedRectangle(r, 4.0f, selected ? 1.8f : 1.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        g.setColour(button.getToggleState() ? juce::Colour(0xff1b130b) : juce::Colours::white.withAlpha(0.95f));
        g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3), juce::Justification::centred, 1);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(5.0f);
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.43f;
        const auto centre = area.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colours::black.withAlpha(0.72f));
        g.fillEllipse(centre.x - radius - 4.0f, centre.y - radius - 4.0f, (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);
        g.setColour(bronze.darker(0.45f));
        g.drawEllipse(centre.x - radius - 2.0f, centre.y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 2.0f);

        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius + 4.5f, radius + 4.5f, 0.0f,
                          rotaryStartAngle, angle, true);
        g.setColour(gold);
        g.strokePath(arc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::ColourGradient metal(juce::Colour(0xffe6c79e), centre.x - radius, centre.y - radius,
                                   juce::Colour(0xff5b4431), centre.x + radius, centre.y + radius, false);
        metal.addColour(0.48, juce::Colour(0xffb58b60));
        metal.addColour(0.72, juce::Colour(0xff3d3026));
        g.setGradientFill(metal);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xfff5d6a0).withAlpha(0.6f));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        juce::Path pointer;
        const float pointerLength = radius * 0.70f;
        const float pointerThickness = 2.3f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -radius * 0.82f,
                                    pointerThickness, pointerLength, 1.1f);
        g.setColour(juce::Colour(0xff1d1712));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    }
};

AcousticGuitarByJGKAudioProcessorEditor::AcousticGuitarByJGKAudioProcessorEditor(AcousticGuitarByJGKAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), lookAndFeel(std::make_unique<JGKLookAndFeel>())
{
    setLookAndFeel(lookAndFeel.get());
    backgroundOff = loadBinaryImageContaining("JGK_UI_Off");
    backgroundOnBase = loadBinaryImageContaining("JGK_UI_OnBase");

    // Keep the first release fixed-size for maximum host compatibility.
    // We can re-enable host resizing once the FL Studio runtime is proven stable.
    setResizable(false, false);
    setSize(1200, 800);

    const char* rootNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    for (int i = 0; i < 12; ++i)
    {
        rootButtons[(size_t) i] = std::make_unique<juce::TextButton>(rootNames[i]);
        auto& b = *rootButtons[(size_t) i];
        b.setClickingTogglesState(false);
        b.onClick = [this, i]
        {
            processor.setSelectedChord(i, getIntParam("chordType"), true);
            refreshSelections();
        };
        addAndMakeVisible(b);
    }

    const char* typeNames[8] = { "Major", "Minor", "7", "Maj7", "Min7", "Sus2", "Sus4", "Add9" };
    for (int i = 0; i < 8; ++i)
    {
        typeButtons[(size_t) i] = std::make_unique<juce::TextButton>(typeNames[i]);
        auto& b = *typeButtons[(size_t) i];
        b.onClick = [this, i]
        {
            processor.setSelectedChord(getIntParam("root"), i, true);
            refreshSelections();
        };
        addAndMakeVisible(b);
    }

    voicingBox.addItem("Open (Standard)", 1);
    voicingBox.addItem("Higher", 2);
    voicingBox.addItem("Wide", 3);
    addAndMakeVisible(voicingBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(), "voicing", voicingBox));

    inversionBox.addItem("Root Position", 1);
    inversionBox.addItem("1st Inversion", 2);
    inversionBox.addItem("2nd Inversion", 3);
    addAndMakeVisible(inversionBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.getAPVTS(), "inversion", inversionBox));

    capoValue.setJustificationType(juce::Justification::centred);
    capoValue.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    capoValue.setColour(juce::Label::backgroundColourId, nearBlack.withAlpha(0.88f));
    capoValue.setColour(juce::Label::outlineColourId, bronze);
    addAndMakeVisible(capoValue);
    addAndMakeVisible(capoMinus);
    addAndMakeVisible(capoPlus);
    capoMinus.onClick = [this] { setIntParam("capo", juce::jmax(0, getIntParam("capo") - 1)); refreshSelections(); };
    capoPlus.onClick  = [this] { setIntParam("capo", juce::jmin(12, getIntParam("capo") + 1)); refreshSelections(); };

    configureKnob(strumKnob, "strum", false, " ms");
    configureKnob(humanizeKnob, "humanize", true);
    configureKnob(palmKnob, "mute", true);
    configureKnob(dynamicsKnob, "dynamics", true);
    configureKnob(toneKnob, "tone", true);
    configureKnob(roomKnob, "room", true);
    configureKnob(tightLooseKnob, "tightLoose", true);
    configureKnob(outputKnob, "output", false, " dB");

    configureKnob(scratchVolumeKnob, "scratchVolume", true);
    configureKnob(scratchLengthKnob, "scratchLength", false, " s");
    configureKnob(scratchTimingKnob, "scratchTiming", true);
    configureKnob(percVolumeKnob, "percVolume", true);
    configureKnob(percToneKnob, "percTone", true);
    configureKnob(percMixKnob, "percMix", true);

    const char* modeNames[3] = { "STRUM", "FINGERPICK", "PERCUSSION" };
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[(size_t) i] = std::make_unique<juce::TextButton>(modeNames[i]);
        modeButtons[(size_t) i]->onClick = [this, i] { setMode(i); };
        addAndMakeVisible(*modeButtons[(size_t) i]);
    }

    presetBox.addItem("Straight 8ths", 1);
    presetBox.addItem("Pop Acoustic", 2);
    presetBox.addItem("Singer-Songwriter", 3);
    presetBox.addItem("Slow Ballad", 4);
    presetBox.addItem("Driving Acoustic", 5);
    presetBox.addItem("Palm Muted", 6);
    presetBox.addItem("Folk", 7);
    presetBox.addItem("Custom", 8);
    presetBox.setSelectedId(3, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            processor.setPatternPreset(presetBox.getSelectedId() - 1);
        refreshSelections();
    };
    addAndMakeVisible(presetBox);

    for (int i = 0; i < 8; ++i)
    {
        patternButtons[(size_t) i] = std::make_unique<juce::TextButton>();
        auto& b = *patternButtons[(size_t) i];
        b.setTooltip("Click to apply the selected edit tool. With no tool selected, click cycles through Down, Up, Palm Mute, Choke, Scratch, Percussion and Rest.");
        b.onClick = [this, i]
        {
            int next = selectedEditEvent;
            if (next < 0)
            {
                const int current = getIntParam("step" + juce::String(i + 1));
                next = (current + 1) % 7;
            }
            processor.setPatternStep(i, next);
            presetBox.setSelectedId(8, juce::dontSendNotification);
            refreshSelections();
        };
        addAndMakeVisible(b);
    }

    const std::array<int, 6> tools
    {
        AcousticGuitarByJGKAudioProcessor::Down,
        AcousticGuitarByJGKAudioProcessor::Up,
        AcousticGuitarByJGKAudioProcessor::Palm,
        AcousticGuitarByJGKAudioProcessor::Choke,
        AcousticGuitarByJGKAudioProcessor::Scratch,
        AcousticGuitarByJGKAudioProcessor::Rest
    };
    const char* toolNames[6] = { "DOWN", "UP", "MUTE", "CHOKE", "SCRATCH", "REST" };
    for (int i = 0; i < 6; ++i)
    {
        editToolButtons[(size_t) i] = std::make_unique<juce::TextButton>(toolNames[i]);
        editToolButtons[(size_t) i]->onClick = [this, i, tools]
        {
            selectedEditEvent = (selectedEditEvent == tools[i]) ? -1 : tools[i];
            refreshSelections();
        };
        addAndMakeVisible(*editToolButtons[(size_t) i]);
    }

    const char* padNames[12] = { "C", "G", "Am", "F", "Dm", "Em", "D", "A", "Bm", "E", "Bb", "F#" };
    const std::array<int, 12> padRoots { 0, 7, 9, 5, 2, 4, 2, 9, 11, 4, 10, 6 };
    const std::array<int, 12> padTypes { 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0 };
    for (int i = 0; i < 12; ++i)
    {
        chordPadButtons[(size_t) i] = std::make_unique<juce::TextButton>(padNames[i]);
        auto& b = *chordPadButtons[(size_t) i];
        auto* buttonPtr = &b;
        b.onStateChange = [this, i, padRoots, padTypes, buttonPtr]
        {
            if (buttonPtr->isDown())
                processor.pressChordFromUI(padRoots[(size_t) i], padTypes[(size_t) i]);
            else
                processor.releaseChordFromUI();
            refreshSelections();
        };
        addAndMakeVisible(b);
    }

    const char* lengthNames[3] = { "1 Bar", "2 Bars", "4 Bars" };
    const char* speedNames[3] = { "1/4", "1/8", "1/16" };
    const char* playNames[3] = { "Latch", "Hold", "MIDI" };
    for (int i = 0; i < 3; ++i)
    {
        lengthButtons[(size_t) i] = std::make_unique<juce::TextButton>(lengthNames[i]);
        speedButtons[(size_t) i] = std::make_unique<juce::TextButton>(speedNames[i]);
        playModeButtons[(size_t) i] = std::make_unique<juce::TextButton>(playNames[i]);
        lengthButtons[(size_t) i]->onClick = [this, i] { setPatternLength(i); };
        speedButtons[(size_t) i]->onClick = [this, i] { setPatternSpeed(i); };
        playModeButtons[(size_t) i]->onClick = [this, i] { setPlayMode(i); };
        addAndMakeVisible(*lengthButtons[(size_t) i]);
        addAndMakeVisible(*speedButtons[(size_t) i]);
        addAndMakeVisible(*playModeButtons[(size_t) i]);
    }

    for (int i = 0; i < 5; ++i)
    {
        variationButtons[(size_t) i] = std::make_unique<juce::TextButton>(i == 0 ? "-" : juce::String(i));
        variationButtons[(size_t) i]->onClick = [this, i] { setVariation(i); };
        addAndMakeVisible(*variationButtons[(size_t) i]);
    }

    const char* scratchNames[4] = { "Down", "Up", "Rake", "Slap" };
    const char* percNames[4] = { "Body", "Thumb", "Knuckle", "Slap" };
    for (int i = 0; i < 4; ++i)
    {
        scratchTypeButtons[(size_t) i] = std::make_unique<juce::TextButton>(scratchNames[i]);
        percTypeButtons[(size_t) i] = std::make_unique<juce::TextButton>(percNames[i]);
        scratchTypeButtons[(size_t) i]->onClick = [this, i] { setScratchType(i); };
        percTypeButtons[(size_t) i]->onClick = [this, i] { setPercType(i); };
        addAndMakeVisible(*scratchTypeButtons[(size_t) i]);
        addAndMakeVisible(*percTypeButtons[(size_t) i]);
    }

    refreshSelections();
    startTimerHz(30);
}

AcousticGuitarByJGKAudioProcessorEditor::~AcousticGuitarByJGKAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void AcousticGuitarByJGKAudioProcessorEditor::configureKnob(juce::Slider& slider,
                                                            const juce::String& paramId,
                                                            bool percent,
                                                            const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 17);
    slider.setScrollWheelEnabled(false);
    if (percent)
        slider.textFromValueFunction = [] (double value) { return juce::String((int) std::round(value * 100.0)) + "%"; };
    else if (suffix.isNotEmpty())
        slider.textFromValueFunction = [suffix] (double value)
        {
            const int decimals = suffix == " s" ? 2 : suffix == " dB" ? 1 : 0;
            return juce::String(value, decimals) + suffix;
        };
    addAndMakeVisible(slider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), paramId, slider));
}

juce::Rectangle<int> AcousticGuitarByJGKAudioProcessorEditor::scaled(int x, int y, int w, int h) const
{
    const float sx = (float) getWidth() / 1536.0f;
    const float sy = (float) getHeight() / 1024.0f;
    return { (int) std::round(x * sx), (int) std::round(y * sy),
             (int) std::round(w * sx), (int) std::round(h * sy) };
}

void AcousticGuitarByJGKAudioProcessorEditor::resized()
{
    // Chord root: 6 columns x 2 rows.
    for (int i = 0; i < 12; ++i)
    {
        const int col = i % 6;
        const int row = i / 6;
        rootButtons[(size_t) i]->setBounds(scaled(102 + col * 46, 132 + row * 48, 42, 40));
    }

    // Chord type: 4 columns x 2 rows.
    for (int i = 0; i < 8; ++i)
    {
        const int col = i % 4;
        const int row = i / 4;
        typeButtons[(size_t) i]->setBounds(scaled(400 + col * 67, 132 + row * 48, 62, 40));
    }

    voicingBox.setBounds(scaled(1001, 137, 184, 34));
    inversionBox.setBounds(scaled(1001, 203, 184, 33));
    capoMinus.setBounds(scaled(1215, 134, 45, 42));
    capoValue.setBounds(scaled(1266, 134, 78, 42));
    capoPlus.setBounds(scaled(1350, 134, 45, 42));

    juce::Slider* mainKnobs[8] = { &strumKnob, &humanizeKnob, &palmKnob, &dynamicsKnob,
                                   &toneKnob, &roomKnob, &tightLooseKnob, &outputKnob };
    const int knobXs[8] = { 177, 320, 464, 608, 748, 867, 997, 1124 };
    for (int i = 0; i < 8; ++i)
        mainKnobs[i]->setBounds(scaled(knobXs[i], 548, 92, 87));

    modeButtons[0]->setBounds(scaled(72, 657, 190, 40));
    modeButtons[1]->setBounds(scaled(267, 657, 190, 40));
    modeButtons[2]->setBounds(scaled(463, 657, 160, 40));

    presetBox.setBounds(scaled(44, 739, 204, 33));

    for (int i = 0; i < 8; ++i)
        patternButtons[(size_t) i]->setBounds(scaled(278 + i * 56, 758, 50, 52));

    for (int i = 0; i < 3; ++i)
    {
        lengthButtons[(size_t) i]->setBounds(scaled(812 + i * 62, 745, 58, 44));
        speedButtons[(size_t) i]->setBounds(scaled(1028 + i * 52, 745, 48, 44));
    }

    for (int i = 0; i < 6; ++i)
        editToolButtons[(size_t) i]->setBounds(scaled(1202 + i * 48, 744, 44, 55));

    scratchVolumeKnob.setBounds(scaled(342, 853, 72, 76));
    scratchLengthKnob.setBounds(scaled(443, 853, 72, 76));
    scratchTimingKnob.setBounds(scaled(536, 853, 72, 76));
    for (int i = 0; i < 4; ++i)
        scratchTypeButtons[(size_t) i]->setBounds(scaled(627 + i * 47, 855, 43, 53));

    percVolumeKnob.setBounds(scaled(945, 853, 72, 76));
    percToneKnob.setBounds(scaled(1044, 853, 72, 76));
    percMixKnob.setBounds(scaled(1130, 853, 72, 76));
    for (int i = 0; i < 4; ++i)
        percTypeButtons[(size_t) i]->setBounds(scaled(1220 + i * 61, 855, 58, 53));

    for (int i = 0; i < 12; ++i)
        chordPadButtons[(size_t) i]->setBounds(scaled(184 + i * 61, 965, 56, 41));

    for (int i = 0; i < 3; ++i)
        playModeButtons[(size_t) i]->setBounds(scaled(954 + i * 69, 966, 65, 40));

    for (int i = 0; i < 5; ++i)
        variationButtons[(size_t) i]->setBounds(scaled(1215 + i * 48, 966, 44, 40));
}

void AcousticGuitarByJGKAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int capo = getIntParam("capo");
    const auto& bg = capo > 0 && backgroundOnBase.isValid() ? backgroundOnBase : backgroundOff;

    if (bg.isValid())
        g.drawImageWithin(bg, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit, false);
    else
        g.fillAll(juce::Colour(0xff17110d));

    // Dynamic chord-name panel covers the baked-in C Major label.
    auto chordText = scaled(694, 143, 150, 71).toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(chordText, 5.0f);
    g.setColour(goldBright);
    g.setFont(juce::FontOptions(23.0f * (float) getWidth() / 1536.0f, juce::Font::bold));
    g.drawFittedText(processor.getDisplayChordName().toUpperCase(), chordText.toNearestInt(), juce::Justification::centred, 2);

    drawChordDiagram(g, scaled(850, 126, 105, 108).toFloat());

    // Dynamic capo/actual-pitch readout.
    auto status = scaled(1215, 188, 185, 50).toFloat();
    g.setColour(panel.withAlpha(0.96f));
    g.fillRoundedRectangle(status, 4.0f);
    g.setColour(juce::Colour(0xfff2c27d));
    g.setFont(juce::FontOptions(12.0f * (float) getWidth() / 1536.0f));
    const juce::String statusText = "CAPO USED: " + (capo == 0 ? juce::String("OFF") : juce::String(capo))
                                  + "\nACTUAL PITCH: " + processor.getActualPitchName().toUpperCase();
    g.drawFittedText(statusText, status.toNearestInt().reduced(4), juce::Justification::centredLeft, 2);

    if (capo > 0)
        drawCapoOnNeck(g, capo);
}

void AcousticGuitarByJGKAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const int step = processor.getCurrentPatternStep();
    if (step >= 0 && step < 8)
    {
        auto r = patternButtons[(size_t) step]->getBounds().toFloat().expanded(2.0f);
        g.setColour(goldBright.withAlpha(0.95f));
        g.drawRoundedRectangle(r, 5.0f, 2.3f);
    }
}

void AcousticGuitarByJGKAudioProcessorEditor::drawChordDiagram(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(panel);
    g.fillRoundedRectangle(area, 4.0f);
    area = area.reduced(8.0f, 7.0f);

    const auto frets = processor.getDisplayFrets();
    int minPositive = 99;
    for (auto fret : frets)
        if (fret > 0) minPositive = juce::jmin(minPositive, fret);
    const int baseFret = (minPositive > 4 && minPositive < 99) ? minPositive : 1;

    const float gridTop = area.getY() + 17.0f;
    const float gridBottom = area.getBottom() - 16.0f;
    const float gridLeft = area.getX() + 4.0f;
    const float gridRight = area.getRight() - 4.0f;
    const float stringGap = (gridRight - gridLeft) / 5.0f;
    const float fretGap = (gridBottom - gridTop) / 5.0f;

    g.setColour(juce::Colours::white.withAlpha(0.74f));
    for (int s = 0; s < 6; ++s)
    {
        const float x = gridLeft + s * stringGap;
        g.drawLine(x, gridTop, x, gridBottom, 1.0f);
    }
    for (int f = 0; f <= 5; ++f)
    {
        const float y = gridTop + f * fretGap;
        g.drawLine(gridLeft, y, gridRight, y, f == 0 ? 2.0f : 1.0f);
    }

    const float dotRadius = juce::jmax(2.5f, area.getWidth() * 0.04f);
    for (int s = 0; s < 6; ++s)
    {
        const int fret = frets[(size_t) s];
        const float x = gridLeft + s * stringGap;
        if (fret < 0)
        {
            g.setFont(juce::FontOptions(10.0f));
            g.drawText("X", (int) x - 6, (int) area.getY(), 12, 13, juce::Justification::centred);
        }
        else if (fret == 0)
        {
            g.drawEllipse(x - dotRadius, area.getY() + 2.0f, dotRadius * 2.0f, dotRadius * 2.0f, 1.2f);
        }
        else if (fret >= baseFret && fret <= baseFret + 4)
        {
            const float y = gridTop + ((float) (fret - baseFret) + 0.5f) * fretGap;
            g.setColour(goldBright);
            g.fillEllipse(x - dotRadius, y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.74f));
        }
    }

    if (baseFret > 1)
    {
        g.setColour(gold);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(juce::String(baseFret), (int) area.getX(), (int) (gridTop + 2.0f), 16, 12, juce::Justification::left);
    }
}

void AcousticGuitarByJGKAudioProcessorEditor::drawCapoOnNeck(juce::Graphics& g, int fret)
{
    // Approximate the real fret spacing on the approved guitar artwork.
    static constexpr float fretX[12] =
    {
        1152.0f, 1105.0f, 1060.0f, 1017.0f, 977.0f, 939.0f,
        903.0f, 869.0f, 837.0f, 807.0f, 779.0f, 752.0f
    };

    const float sx = (float) getWidth() / 1536.0f;
    const float sy = (float) getHeight() / 1024.0f;
    const float x = fretX[juce::jlimit(1, 12, fret) - 1] * sx;
    const float y = 321.0f * sy;
    const float h = 116.0f * sy;
    const float w = 13.0f * sx;

    juce::Rectangle<float> bar(x - w * 0.5f, y, w, h);
    g.setColour(juce::Colours::black.withAlpha(0.86f));
    g.fillRoundedRectangle(bar.expanded(3.0f * sx, 2.0f * sy), 5.0f * sx);
    juce::ColourGradient metal(juce::Colour(0xffe6e0d3), bar.getX(), bar.getY(),
                               juce::Colour(0xff45423f), bar.getRight(), bar.getBottom(), false);
    g.setGradientFill(metal);
    g.fillRoundedRectangle(bar, 3.0f * sx);
    g.setColour(gold);
    g.drawRoundedRectangle(bar, 3.0f * sx, 1.1f * sx);
    g.fillEllipse(x - 7.0f * sx, y + h - 8.0f * sy, 14.0f * sx, 14.0f * sy);
}

void AcousticGuitarByJGKAudioProcessorEditor::timerCallback()
{
    const int step = processor.getCurrentPatternStep();
    const int capo = getIntParam("capo");
    const int root = processor.getDisplayRoot();
    const int type = processor.getDisplayType();

    if (step != lastDisplayedStep || capo != lastCapo || root != lastRoot || type != lastType)
    {
        lastDisplayedStep = step;
        lastCapo = capo;
        lastRoot = root;
        lastType = type;
        refreshSelections();
        repaint();
    }
}

void AcousticGuitarByJGKAudioProcessorEditor::setIntParam(const juce::String& id, int value)
{
    if (auto* p = processor.getAPVTS().getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float) value));
        p->endChangeGesture();
    }
}

int AcousticGuitarByJGKAudioProcessorEditor::getIntParam(const juce::String& id) const
{
    if (auto* v = processor.getAPVTS().getRawParameterValue(id))
        return (int) std::round(v->load());
    return 0;
}

void AcousticGuitarByJGKAudioProcessorEditor::refreshSelections()
{
    const int root = getIntParam("root");
    const int type = getIntParam("chordType");
    for (int i = 0; i < 12; ++i)
        rootButtons[(size_t) i]->setToggleState(i == root, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i)
        typeButtons[(size_t) i]->setToggleState(i == type, juce::dontSendNotification);

    const int capo = getIntParam("capo");
    capoValue.setText(capo == 0 ? "OFF" : juce::String(capo), juce::dontSendNotification);

    for (int i = 0; i < 3; ++i)
    {
        modeButtons[(size_t) i]->setToggleState(i == getIntParam("performanceMode"), juce::dontSendNotification);
        lengthButtons[(size_t) i]->setToggleState(i == getIntParam("patternLength"), juce::dontSendNotification);
        speedButtons[(size_t) i]->setToggleState(i == getIntParam("patternSpeed"), juce::dontSendNotification);
        playModeButtons[(size_t) i]->setToggleState(i == getIntParam("playMode"), juce::dontSendNotification);
    }
    for (int i = 0; i < 5; ++i)
        variationButtons[(size_t) i]->setToggleState(i == getIntParam("variation"), juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
    {
        scratchTypeButtons[(size_t) i]->setToggleState(i == getIntParam("scratchType"), juce::dontSendNotification);
        percTypeButtons[(size_t) i]->setToggleState(i == getIntParam("percType"), juce::dontSendNotification);
    }

    const char* stepIds[8] = { "step1", "step2", "step3", "step4", "step5", "step6", "step7", "step8" };
    for (int i = 0; i < 8; ++i)
    {
        const int event = getIntParam(stepIds[i]);
        patternButtons[(size_t) i]->setButtonText(patternSymbol(event));
        patternButtons[(size_t) i]->setToggleState(i == processor.getCurrentPatternStep(), juce::dontSendNotification);
    }

    const int toolEvents[6] =
    {
        AcousticGuitarByJGKAudioProcessor::Down,
        AcousticGuitarByJGKAudioProcessor::Up,
        AcousticGuitarByJGKAudioProcessor::Palm,
        AcousticGuitarByJGKAudioProcessor::Choke,
        AcousticGuitarByJGKAudioProcessor::Scratch,
        AcousticGuitarByJGKAudioProcessor::Rest
    };
    for (int i = 0; i < 6; ++i)
        editToolButtons[(size_t) i]->setToggleState(selectedEditEvent == toolEvents[i], juce::dontSendNotification);
}

void AcousticGuitarByJGKAudioProcessorEditor::setMode(int mode)
{
    setIntParam("performanceMode", mode);
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setPlayMode(int mode)
{
    setIntParam("playMode", mode);
    if (mode == 2)
        processor.clearUIChord();
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setPatternLength(int value)
{
    setIntParam("patternLength", value);
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setPatternSpeed(int value)
{
    setIntParam("patternSpeed", value);
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setVariation(int value)
{
    setIntParam("variation", value);
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setScratchType(int value)
{
    setIntParam("scratchType", value);
    refreshSelections();
}

void AcousticGuitarByJGKAudioProcessorEditor::setPercType(int value)
{
    setIntParam("percType", value);
    refreshSelections();
}
