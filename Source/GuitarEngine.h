#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <set>
#include <vector>
#include <random>
#include <cstdint>

class GuitarEngine
{
public:
    enum class PercussionType
    {
        body = 0,
        thumb,
        knuckle,
        slap
    };

    void prepare(double newSampleRate);
    void reset();

    void setCapo(int value)       { capo = juce::jlimit(0, 12, value); }
    void setStrumMs(float value)  { strumMs = juce::jlimit(1.0f, 180.0f, value); }
    void setHumanize(float value) { humanize = juce::jlimit(0.0f, 1.0f, value); }
    void setTone(float value)     { tone = juce::jlimit(0.0f, 1.0f, value); }
    void setRoom(float value)     { room = juce::jlimit(0.0f, 1.0f, value); }
    void setPalmMute(float value) { palmMute = juce::jlimit(0.0f, 1.0f, value); }
    void setOutputDb(float value) { outputDb = juce::jlimit(-24.0f, 6.0f, value); }

    void strumVoicing(const std::array<int, 6>& notes,
                      float velocity,
                      bool down,
                      int baseDelaySamples = 0,
                      float palmOverride = -1.0f);

    void pickString(const std::array<int, 6>& notes,
                    int stringIndex,
                    float velocity,
                    int delaySamples = 0,
                    float palmOverride = -1.0f);

    void scratch(bool upward,
                 float intensity,
                 float lengthSeconds,
                 int delaySamples = 0);

    void percussion(PercussionType type,
                    float velocity,
                    float toneAmount,
                    int delaySamples = 0);

    void scheduleChoke(int delaySamples, float releaseSeconds = 0.018f);
    void chokeNow(float releaseSeconds = 0.018f);
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

private:
    struct GuitarSample
    {
        int rootMidi = 60;
        double sampleRate = 44100.0;
        juce::AudioBuffer<float> audio;
    };

    struct Voice
    {
        const GuitarSample* sample = nullptr;
        double position = 0.0;
        double step = 1.0;
        float gain = 0.0f;
        float envelope = 1.0f;
        float releaseMul = 1.0f;
        float palmMul = 1.0f;
        float palmAmount = 0.0f;
        float lowpass = 0.0f;
        float pan = 0.0f;
        int delaySamples = 0;
        bool active = false;
    };

    enum class TransientKind
    {
        scratchDown,
        scratchUp,
        body,
        thumb,
        knuckle,
        slap
    };

    struct TransientVoice
    {
        TransientKind kind = TransientKind::body;
        int delaySamples = 0;
        int remainingSamples = 0;
        int totalSamples = 0;
        float gain = 0.0f;
        float tone = 0.5f;
        float phase = 0.0f;
        float filter = 0.0f;
        uint32_t noiseState = 0x12345678u;
        bool active = false;
    };

    struct ChokeEvent
    {
        int delaySamples = 0;
        float seconds = 0.018f;
        bool active = false;
    };

    void loadSamples();
    const GuitarSample* nearestSample(int midiNote) const;
    Voice& getVoice();
    TransientVoice& getTransientVoice();
    void startVoice(int midiNote,
                    float velocity,
                    int delaySamples,
                    float pan,
                    float palmAmount);
    void releaseAll(float seconds);
    static float coefficientForSeconds(double sampleRate, float seconds);
    static float nextNoise(uint32_t& state);

    double sampleRate = 44100.0;
    int capo = 0;
    float strumMs = 28.0f;
    float humanize = 0.15f;
    float tone = 0.65f;
    float room = 0.12f;
    float palmMute = 0.0f;
    float outputDb = -6.0f;

    std::vector<GuitarSample> samples;
    std::array<Voice, 48> voices;
    std::array<TransientVoice, 16> transientVoices;
    std::array<ChokeEvent, 8> chokeEvents;
    std::mt19937 rng { 0x4A474B31u };
    juce::Reverb reverb;
};
