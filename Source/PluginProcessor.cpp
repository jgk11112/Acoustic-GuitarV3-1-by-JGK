#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr const char* stepIds[8] =
    {
        "step1", "step2", "step3", "step4", "step5", "step6", "step7", "step8"
    };
}

AcousticGuitarByJGKAudioProcessor::AcousticGuitarByJGKAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS_V3", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AcousticGuitarByJGKAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pid = [] (const char* id) { return juce::ParameterID(id, 1); };

    layout.add(std::make_unique<juce::AudioParameterInt>(pid("capo"), "Capo", 0, 12, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("strum"), "Strum Speed",
        juce::NormalisableRange<float>(5.0f, 160.0f, 0.1f), 28.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("humanize"), "Humanize", 0.0f, 1.0f, 0.18f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("mute"), "Palm Mute", 0.0f, 1.0f, 0.06f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("dynamics"), "Dynamics", 0.0f, 1.0f, 0.72f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("tone"), "Tone", 0.0f, 1.0f, 0.58f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("room"), "Room", 0.0f, 1.0f, 0.14f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("tightLoose"), "Tight / Loose", 0.0f, 1.0f, 0.18f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("output"), "Output",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -5.0f));

    layout.add(std::make_unique<juce::AudioParameterInt>(pid("root"), "Chord Root", 0, 11, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("chordType"), "Chord Type", 0, 7, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("voicing"), "Voicing", 0, 2, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("inversion"), "Inversion", 0, 2, 0));

    layout.add(std::make_unique<juce::AudioParameterInt>(pid("performanceMode"), "Performance Mode", 0, 2, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("patternLength"), "Pattern Length", 0, 2, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("patternSpeed"), "Pattern Speed", 0, 2, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("playMode"), "Play Mode", 0, 2, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("variation"), "Strum Variation", 0, 4, 2));

    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("scratchVolume"), "Scratch Volume", 0.0f, 1.0f, 0.38f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("scratchLength"), "Scratch Length",
        juce::NormalisableRange<float>(0.03f, 0.30f, 0.001f), 0.11f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("scratchTiming"), "Scratch Timing", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("scratchType"), "Scratch Type", 0, 3, 2));

    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("percVolume"), "Percussion Volume", 0.0f, 1.0f, 0.42f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("percTone"), "Percussion Tone", 0.0f, 1.0f, 0.50f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(pid("percMix"), "Percussion Mix", 0.0f, 1.0f, 0.55f));
    layout.add(std::make_unique<juce::AudioParameterInt>(pid("percType"), "Percussion Type", 0, 3, 0));

    const int defaults[8] = { Down, Rest, Up, Down, Scratch, Rest, Up, Choke };
    for (int i = 0; i < 8; ++i)
        layout.add(std::make_unique<juce::AudioParameterInt>(pid(stepIds[i]), "Pattern Step " + juce::String(i + 1), 0, 6, defaults[i]));

    return layout;
}

void AcousticGuitarByJGKAudioProcessor::prepareToPlay(double sr, int)
{
    currentSampleRate = sr;
    engine.prepare(sr);
    heldMidiNotes.clear();
    midiChordActive = false;
    wasPatternActive = false;
    samplesUntilNextStep = 0.0;
    patternStep = 0;
    displayPatternStep.store(-1);
}

bool AcousticGuitarByJGKAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto set = layouts.getMainOutputChannelSet();
    return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
}

int AcousticGuitarByJGKAudioProcessor::positiveMod12(int v)
{
    v %= 12;
    if (v < 0) v += 12;
    return v;
}

std::vector<int> AcousticGuitarByJGKAudioProcessor::chordIntervals(int type)
{
    switch (juce::jlimit(0, 7, type))
    {
        case 0: return { 0, 4, 7 };       // Major
        case 1: return { 0, 3, 7 };       // Minor
        case 2: return { 0, 4, 7, 10 };   // 7
        case 3: return { 0, 4, 7, 11 };   // Maj7
        case 4: return { 0, 3, 7, 10 };   // Min7
        case 5: return { 0, 2, 7 };       // Sus2
        case 6: return { 0, 5, 7 };       // Sus4
        case 7: return { 0, 2, 4, 7 };    // Add9
        default: break;
    }
    return { 0, 4, 7 };
}

AcousticGuitarByJGKAudioProcessor::ChordVoicing
AcousticGuitarByJGKAudioProcessor::buildVoicing(int root, int type, int inversion, int voicingStyle) const
{
    ChordVoicing result;
    const std::array<int, 6> tuning { 40, 45, 50, 55, 59, 64 }; // E2 A2 D3 G3 B3 E4
    const auto intervals = chordIntervals(type);

    int bassInterval = 0;
    if (inversion == 1 && intervals.size() > 1)
        bassInterval = intervals[1];
    else if (inversion >= 2 && intervals.size() > 2)
        bassInterval = intervals[2];

    const int desiredBassPc = positiveMod12(root + bassInterval);

    int bassString = 0;
    int bestBassScore = 999;
    for (int s = 0; s < 3; ++s)
    {
        const int fret = positiveMod12(desiredBassPc - positiveMod12(tuning[(size_t) s]));
        const int score = fret + s * 2;
        if (score < bestBassScore)
        {
            bestBassScore = score;
            bassString = s;
        }
    }

    int previous = -1;
    for (int s = bassString; s < 6; ++s)
    {
        const int low = tuning[(size_t) s];
        int bestMidi = -1;
        float bestScore = 100000.0f;

        for (int midi = low; midi <= low + 12; ++midi)
        {
            const int pc = positiveMod12(midi);
            bool allowed = false;
            for (auto interval : intervals)
                if (pc == positiveMod12(root + interval))
                    allowed = true;

            if (!allowed)
                continue;

            if (s == bassString && pc != desiredBassPc)
                continue;

            const int fret = midi - low;
            float score = (float) fret;
            if (previous >= 0)
            {
                if (midi < previous)
                    score += 8.0f + (float) (previous - midi);
                else
                    score += std::abs((float) midi - ((float) previous + 4.0f)) * 0.18f;
            }

            // Prefer open strings and the first few frets for an acoustic feel.
            if (fret == 0) score -= 1.7f;
            if (fret <= 3) score -= 0.6f;

            if (score < bestScore)
            {
                bestScore = score;
                bestMidi = midi;
            }
        }

        result.notes[(size_t) s] = bestMidi;
        if (bestMidi >= 0)
            previous = bestMidi;
    }

    // Higher voicing choices keep the bass grounded while opening the top strings.
    if (voicingStyle == 1)
    {
        for (int s = 3; s < 6; ++s)
            if (result.notes[(size_t) s] >= 0)
                result.notes[(size_t) s] += 12;
    }
    else if (voicingStyle == 2)
    {
        for (int s = 4; s < 6; ++s)
            if (result.notes[(size_t) s] >= 0)
                result.notes[(size_t) s] += 12;
    }

    return result;
}

std::pair<int, int> AcousticGuitarByJGKAudioProcessor::detectMidiChord() const
{
    if (heldMidiNotes.empty())
        return { (int) *apvts.getRawParameterValue("root"), (int) *apvts.getRawParameterValue("chordType") };

    std::array<bool, 12> pcs {};
    for (auto n : heldMidiNotes)
        pcs[(size_t) positiveMod12(n)] = true;

    const int lowestPc = positiveMod12(*heldMidiNotes.begin());
    float bestScore = -1000.0f;
    int bestRoot = lowestPc;
    int bestType = 0;

    for (int root = 0; root < 12; ++root)
    {
        for (int type = 0; type < 8; ++type)
        {
            const auto ints = chordIntervals(type);
            float score = 0.0f;
            int required = 0;
            for (auto interval : ints)
            {
                ++required;
                score += pcs[(size_t) positiveMod12(root + interval)] ? 2.0f : -2.3f;
            }

            int extras = 0;
            for (int pc = 0; pc < 12; ++pc)
            {
                if (!pcs[(size_t) pc]) continue;
                bool belongs = false;
                for (auto interval : ints)
                    if (pc == positiveMod12(root + interval)) belongs = true;
                if (!belongs) ++extras;
            }

            score -= (float) extras * 0.65f;
            if (root == lowestPc) score += 0.6f;
            if (type == (int) *apvts.getRawParameterValue("chordType")) score += 0.08f;

            if (score > bestScore)
            {
                bestScore = score;
                bestRoot = root;
                bestType = type;
            }
        }
    }

    return { bestRoot, bestType };
}

void AcousticGuitarByJGKAudioProcessor::triggerPatternStep(const ChordVoicing& voicing,
                                                            int step,
                                                            int eventOffset,
                                                            float velocity,
                                                            int performanceMode)
{
    if (performanceMode == 1) // Fingerpick
    {
        static constexpr int pickStrings[8] = { 0, 3, 4, 5, 1, 3, 4, 5 };
        engine.pickString(voicing.notes, pickStrings[step & 7], velocity * 0.90f, eventOffset);
        return;
    }

    if (performanceMode == 2) // Percussion performance
    {
        if ((step & 1) != 0)
            return;

        const float pVol = *apvts.getRawParameterValue("percVolume");
        const float pTone = *apvts.getRawParameterValue("percTone");
        GuitarEngine::PercussionType type = GuitarEngine::PercussionType::body;
        switch ((step / 2) & 3)
        {
            case 0: type = GuitarEngine::PercussionType::body; break;
            case 1: type = GuitarEngine::PercussionType::slap; break;
            case 2: type = GuitarEngine::PercussionType::thumb; break;
            case 3: type = GuitarEngine::PercussionType::knuckle; break;
        }
        engine.percussion(type, velocity * pVol, pTone, eventOffset);
        return;
    }

    const int eventType = (int) *apvts.getRawParameterValue(stepIds[step & 7]);
    switch (eventType)
    {
        case Down:
            engine.strumVoicing(voicing.notes, velocity, true, eventOffset);
            break;
        case Up:
            engine.strumVoicing(voicing.notes, velocity * 0.92f, false, eventOffset);
            break;
        case Palm:
            engine.strumVoicing(voicing.notes, velocity * 0.92f, true, eventOffset, 0.92f);
            break;
        case Choke:
            engine.scheduleChoke(eventOffset, 0.014f);
            break;
        case Scratch:
        {
            const float amount = *apvts.getRawParameterValue("scratchVolume");
            const float length = *apvts.getRawParameterValue("scratchLength");
            const float timing = *apvts.getRawParameterValue("scratchTiming");
            const int timingSamples = (int) std::round(timing * 0.018f * (float) currentSampleRate);
            const int delayed = juce::jmax(0, eventOffset + timingSamples);
            const int scratchType = (int) *apvts.getRawParameterValue("scratchType");
            const bool upward = scratchType == 1 || scratchType == 3;
            engine.scratch(upward, amount * velocity, length, delayed);
            break;
        }
        case Percussion:
        {
            const float pVol = *apvts.getRawParameterValue("percVolume");
            const float pTone = *apvts.getRawParameterValue("percTone");
            const float pMix = *apvts.getRawParameterValue("percMix");
            const int pType = (int) *apvts.getRawParameterValue("percType");
            engine.percussion((GuitarEngine::PercussionType) juce::jlimit(0, 3, pType), velocity * pVol * pMix, pTone, eventOffset);
            break;
        }
        case Rest:
        default:
            break;
    }
}

void AcousticGuitarByJGKAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    engine.setCapo((int) *apvts.getRawParameterValue("capo"));
    engine.setStrumMs(*apvts.getRawParameterValue("strum"));
    engine.setHumanize(*apvts.getRawParameterValue("humanize"));
    engine.setPalmMute(*apvts.getRawParameterValue("mute"));
    engine.setTone(*apvts.getRawParameterValue("tone"));
    engine.setRoom(*apvts.getRawParameterValue("room"));
    engine.setOutputDb(*apvts.getRawParameterValue("output"));

    bool midiChanged = false;
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            heldMidiNotes.insert(msg.getNoteNumber());
            midiChanged = true;
        }
        else if (msg.isNoteOff())
        {
            heldMidiNotes.erase(msg.getNoteNumber());
            midiChanged = true;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldMidiNotes.clear();
            midiChanged = true;
        }
    }
    midi.clear();

    if (midiChanged)
    {
        if (!heldMidiNotes.empty())
        {
            const auto detected = detectMidiChord();
            midiChordRoot = detected.first;
            midiChordType = detected.second;
            midiChordActive = true;
        }
        else
        {
            midiChordActive = false;
        }
    }

    const uint32_t serial = uiSerial.load();
    const bool uiChanged = serial != seenUiSerial;
    if (uiChanged)
        seenUiSerial = serial;

    bool active = false;
    int root = (int) *apvts.getRawParameterValue("root");
    int type = (int) *apvts.getRawParameterValue("chordType");

    if (midiChordActive)
    {
        active = true;
        root = midiChordRoot;
        type = midiChordType;
    }
    else if (uiActive.load())
    {
        active = true;
        root = uiRoot.load();
        type = uiType.load();
    }

    const bool chordChanged = (root != currentRoot || type != currentType || uiChanged || active != wasPatternActive);
    currentRoot = root;
    currentType = type;
    displayRoot.store(root);
    displayType.store(type);
    displayActive.store(active);

    if (!active)
    {
        if (wasPatternActive)
            engine.chokeNow(0.020f);
        wasPatternActive = false;
        samplesUntilNextStep = 0.0;
        patternStep = 0;
        displayPatternStep.store(-1);
        engine.process(buffer, buffer.getNumSamples());
        return;
    }

    if (chordChanged)
    {
        samplesUntilNextStep = 0.0;
        patternStep = 0;
    }
    wasPatternActive = true;

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = juce::jlimit(30.0, 300.0, *hostBpm);
    }

    const int speed = (int) *apvts.getRawParameterValue("patternSpeed");
    const double quarterSamples = currentSampleRate * 60.0 / bpm;
    const double stepSamples = quarterSamples * (speed == 0 ? 1.0 : speed == 1 ? 0.5 : 0.25);

    const int inversion = (int) *apvts.getRawParameterValue("inversion");
    const int voicingStyle = (int) *apvts.getRawParameterValue("voicing");
    const auto voicing = buildVoicing(root, type, inversion, voicingStyle);

    const float dynamics = *apvts.getRawParameterValue("dynamics");
    const float humanize = *apvts.getRawParameterValue("humanize");
    const float loose = *apvts.getRawParameterValue("tightLoose");
    const int variation = (int) *apvts.getRawParameterValue("variation");
    const int performanceMode = (int) *apvts.getRawParameterValue("performanceMode");

    std::uniform_real_distribution<float> rand(-1.0f, 1.0f);
    while (samplesUntilNextStep < (double) buffer.getNumSamples())
    {
        int offset = (int) std::round(samplesUntilNextStep);
        const float timingMs = humanize * 3.5f + loose * 12.0f;
        offset += (int) std::round(rand(timingRng) * timingMs * 0.001f * (float) currentSampleRate);
        offset = juce::jlimit(0, buffer.getNumSamples() - 1, offset);

        const float variationAmount = (float) variation / 4.0f;
        const float velocity = juce::jlimit(0.12f, 1.0f,
            (0.48f + dynamics * 0.48f) * (1.0f + rand(timingRng) * 0.08f * variationAmount));

        displayPatternStep.store(patternStep);
        triggerPatternStep(voicing, patternStep, offset, velocity, performanceMode);
        patternStep = (patternStep + 1) & 7;
        samplesUntilNextStep += stepSamples;
    }

    samplesUntilNextStep -= (double) buffer.getNumSamples();
    engine.process(buffer, buffer.getNumSamples());
}

void AcousticGuitarByJGKAudioProcessor::pressChordFromUI(int root, int type)
{
    root = juce::jlimit(0, 11, root);
    type = juce::jlimit(0, 7, type);
    uiRoot.store(root);
    uiType.store(type);
    uiActive.store(true);
    uiSerial.fetch_add(1);
    setSelectedChord(root, type, false);
}

void AcousticGuitarByJGKAudioProcessor::releaseChordFromUI()
{
    const int playMode = (int) *apvts.getRawParameterValue("playMode");
    if (playMode == 1) // Hold mode
        clearUIChord();
}

void AcousticGuitarByJGKAudioProcessor::clearUIChord()
{
    uiActive.store(false);
    uiSerial.fetch_add(1);
}

void AcousticGuitarByJGKAudioProcessor::setSelectedChord(int root, int type, bool retriggerIfPlaying)
{
    root = juce::jlimit(0, 11, root);
    type = juce::jlimit(0, 7, type);
    setIntParameter("root", root);
    setIntParameter("chordType", type);

    if (retriggerIfPlaying && uiActive.load())
    {
        uiRoot.store(root);
        uiType.store(type);
        uiSerial.fetch_add(1);
    }
}

void AcousticGuitarByJGKAudioProcessor::setPatternStep(int step, int eventType)
{
    if (step < 0 || step >= 8)
        return;
    setIntParameter(stepIds[step], juce::jlimit(0, 6, eventType));
}

void AcousticGuitarByJGKAudioProcessor::setPatternPreset(int presetIndex)
{
    static constexpr int patterns[][8] =
    {
        { Down, Up, Down, Up, Down, Up, Down, Up },
        { Down, Rest, Down, Up, Rest, Up, Down, Up },
        { Down, Rest, Up, Down, Scratch, Rest, Up, Choke },
        { Down, Rest, Up, Rest, Down, Up, Rest, Up },
        { Down, Down, Up, Down, Up, Down, Up, Up },
        { Palm, Palm, Palm, Up, Palm, Palm, Down, Up },
        { Down, Rest, Down, Up, Up, Down, Up, Rest },
        { Down, Up, Palm, Up, Down, Scratch, Up, Choke }
    };

    presetIndex = juce::jlimit(0, 7, presetIndex);
    for (int i = 0; i < 8; ++i)
        setPatternStep(i, patterns[presetIndex][i]);
}

void AcousticGuitarByJGKAudioProcessor::setIntParameter(const juce::String& id, int value)
{
    if (auto* parameter = apvts.getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
        parameter->endChangeGesture();
    }
}

juce::String AcousticGuitarByJGKAudioProcessor::rootName(int root)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[positiveMod12(root)];
}

juce::String AcousticGuitarByJGKAudioProcessor::typeName(int type)
{
    static const char* names[] = { "Major", "Minor", "7", "Maj7", "Min7", "Sus2", "Sus4", "Add9" };
    return names[juce::jlimit(0, 7, type)];
}

juce::String AcousticGuitarByJGKAudioProcessor::chordName(int root, int type)
{
    const auto r = rootName(root);
    switch (juce::jlimit(0, 7, type))
    {
        case 0: return r + " Major";
        case 1: return r + " Minor";
        case 2: return r + "7";
        case 3: return r + " Maj7";
        case 4: return r + " Min7";
        case 5: return r + " Sus2";
        case 6: return r + " Sus4";
        case 7: return r + " Add9";
        default: break;
    }
    return r;
}

juce::String AcousticGuitarByJGKAudioProcessor::getDisplayChordName() const
{
    return chordName(displayRoot.load(), displayType.load());
}

juce::String AcousticGuitarByJGKAudioProcessor::getActualPitchName() const
{
    const int capo = (int) *apvts.getRawParameterValue("capo");
    return chordName(displayRoot.load() + capo, displayType.load());
}

std::array<int, 6> AcousticGuitarByJGKAudioProcessor::getDisplayFrets() const
{
    static constexpr std::array<int, 6> tuning { 40, 45, 50, 55, 59, 64 };
    const int inversion = (int) *apvts.getRawParameterValue("inversion");
    const int voicingStyle = (int) *apvts.getRawParameterValue("voicing");
    const auto v = buildVoicing(displayRoot.load(), displayType.load(), inversion, voicingStyle);
    std::array<int, 6> frets {};
    for (int i = 0; i < 6; ++i)
        frets[(size_t) i] = v.notes[(size_t) i] < 0 ? -1 : juce::jmax(0, v.notes[(size_t) i] - tuning[(size_t) i]);
    return frets;
}

juce::AudioProcessorEditor* AcousticGuitarByJGKAudioProcessor::createEditor()
{
    return new AcousticGuitarByJGKAudioProcessorEditor(*this);
}

void AcousticGuitarByJGKAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("jgkStateVersion", 3, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AcousticGuitarByJGKAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    if (auto xml = getXmlFromBinary(data, size))
    {
        if (!xml->hasTagName(apvts.state.getType()))
            return;

        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid())
            return;

        // V3.1 only restores V3 state. Older V2/V3 wrapper presets are ignored
        // so FL Studio cannot feed the new engine an incompatible parameter tree.
        const int version = (int) state.getProperty("jgkStateVersion", 3);
        if (version != 3)
            return;

        apvts.replaceState(state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcousticGuitarByJGKAudioProcessor();
}
