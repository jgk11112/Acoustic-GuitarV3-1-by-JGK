#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

class AcousticGuitarByJGKAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit AcousticGuitarByJGKAudioProcessorEditor(AcousticGuitarByJGKAudioProcessor&);
    ~AcousticGuitarByJGKAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    class JGKLookAndFeel;

    void timerCallback() override;
    juce::Rectangle<int> scaled(int x, int y, int w, int h) const;
    void configureKnob(juce::Slider& slider, const juce::String& paramId, bool percent = false, const juce::String& suffix = {});
    void setIntParam(const juce::String& id, int value);
    int getIntParam(const juce::String& id) const;
    void refreshSelections();
    void drawChordDiagram(juce::Graphics& g, juce::Rectangle<float> area);
    void drawCapoOnNeck(juce::Graphics& g, int fret);
    void setMode(int mode);
    void setPlayMode(int mode);
    void setPatternLength(int value);
    void setPatternSpeed(int value);
    void setVariation(int value);
    void setScratchType(int value);
    void setPercType(int value);

    AcousticGuitarByJGKAudioProcessor& processor;
    std::unique_ptr<JGKLookAndFeel> lookAndFeel;

    juce::Image backgroundOff;
    juce::Image backgroundOnBase;

    std::array<std::unique_ptr<juce::TextButton>, 12> rootButtons;
    std::array<std::unique_ptr<juce::TextButton>, 8> typeButtons;
    std::array<std::unique_ptr<juce::TextButton>, 8> patternButtons;
    std::array<std::unique_ptr<juce::TextButton>, 6> editToolButtons;
    std::array<std::unique_ptr<juce::TextButton>, 12> chordPadButtons;
    std::array<std::unique_ptr<juce::TextButton>, 3> modeButtons;
    std::array<std::unique_ptr<juce::TextButton>, 3> lengthButtons;
    std::array<std::unique_ptr<juce::TextButton>, 3> speedButtons;
    std::array<std::unique_ptr<juce::TextButton>, 3> playModeButtons;
    std::array<std::unique_ptr<juce::TextButton>, 5> variationButtons;
    std::array<std::unique_ptr<juce::TextButton>, 4> scratchTypeButtons;
    std::array<std::unique_ptr<juce::TextButton>, 4> percTypeButtons;

    juce::TextButton capoMinus { "<" };
    juce::TextButton capoPlus { ">" };
    juce::Label capoValue;

    juce::ComboBox presetBox;
    juce::ComboBox voicingBox;
    juce::ComboBox inversionBox;

    juce::Slider strumKnob;
    juce::Slider humanizeKnob;
    juce::Slider palmKnob;
    juce::Slider dynamicsKnob;
    juce::Slider toneKnob;
    juce::Slider roomKnob;
    juce::Slider tightLooseKnob;
    juce::Slider outputKnob;

    juce::Slider scratchVolumeKnob;
    juce::Slider scratchLengthKnob;
    juce::Slider scratchTimingKnob;
    juce::Slider percVolumeKnob;
    juce::Slider percToneKnob;
    juce::Slider percMixKnob;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;

    int selectedEditEvent = -1;
    int lastDisplayedStep = -2;
    int lastCapo = -1;
    int lastRoot = -1;
    int lastType = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AcousticGuitarByJGKAudioProcessorEditor)
};
