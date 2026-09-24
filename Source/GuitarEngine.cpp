#include "GuitarEngine.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

float GuitarEngine::coefficientForSeconds(double sr, float seconds)
{
    const auto safe = juce::jmax(0.004f, seconds);
    return std::exp(std::log(0.001f) / (safe * (float) sr));
}

float GuitarEngine::nextNoise(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return ((float) (state & 0x00FFFFFFu) / 8388607.5f) - 1.0f;
}

void GuitarEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    loadSamples();
    reverb.setSampleRate(sampleRate);
    reset();
}

void GuitarEngine::reset()
{
    for (auto& v : voices)
        v = {};
    for (auto& v : transientVoices)
        v = {};
    for (auto& e : chokeEvents)
        e = {};
    reverb.reset();
}

void GuitarEngine::loadSamples()
{
    samples.clear();

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String resourceName(BinaryData::namedResourceList[i]);
        const int marker = resourceName.indexOf("MartinGM2_");
        if (marker < 0)
            continue;

        const auto midiText = resourceName.substring(marker + 10, marker + 13);
        const int root = midiText.getIntValue();
        if (root <= 0)
            continue;

        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
            continue;

        auto input = std::make_unique<juce::MemoryInputStream>(data, (size_t) dataSize, false);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(input)));
        if (reader == nullptr || reader->lengthInSamples < 2)
            continue;

        GuitarSample s;
        s.rootMidi = root;
        s.sampleRate = reader->sampleRate;
        s.audio.setSize(1, (int) reader->lengthInSamples);
        reader->read(&s.audio, 0, s.audio.getNumSamples(), 0, true, false);
        samples.push_back(std::move(s));
    }

    std::sort(samples.begin(), samples.end(), [] (const auto& a, const auto& b)
    {
        return a.rootMidi < b.rootMidi;
    });
}

const GuitarEngine::GuitarSample* GuitarEngine::nearestSample(int midiNote) const
{
    if (samples.empty())
        return nullptr;

    const GuitarSample* best = &samples.front();
    int distance = std::abs(midiNote - best->rootMidi);

    for (const auto& s : samples)
    {
        const int d = std::abs(midiNote - s.rootMidi);
        if (d < distance)
        {
            distance = d;
            best = &s;
        }
    }

    return best;
}

GuitarEngine::Voice& GuitarEngine::getVoice()
{
    for (auto& v : voices)
        if (!v.active)
            return v;

    auto* quietest = &voices.front();
    for (auto& v : voices)
        if (v.envelope < quietest->envelope)
            quietest = &v;
    return *quietest;
}

GuitarEngine::TransientVoice& GuitarEngine::getTransientVoice()
{
    for (auto& v : transientVoices)
        if (!v.active)
            return v;

    auto* shortest = &transientVoices.front();
    for (auto& v : transientVoices)
        if (v.remainingSamples < shortest->remainingSamples)
            shortest = &v;
    return *shortest;
}

void GuitarEngine::startVoice(int midiNote,
                              float velocity,
                              int delay,
                              float pan,
                              float palmAmount)
{
    const int targetMidi = juce::jlimit(24, 105, midiNote + capo);
    const auto* source = nearestSample(targetMidi);
    if (source == nullptr)
        return;

    auto& v = getVoice();
    v = {};
    v.sample = source;
    v.position = 0.0;
    v.step = (source->sampleRate / sampleRate)
           * std::pow(2.0, ((double) targetMidi - (double) source->rootMidi) / 12.0);

    std::uniform_real_distribution<float> variation(-1.0f, 1.0f);
    v.gain = std::pow(juce::jlimit(0.02f, 1.0f, velocity), 0.72f)
           * (1.0f + variation(rng) * 0.035f * humanize);
    v.pan = juce::jlimit(-0.65f, 0.65f, pan + variation(rng) * 0.05f * humanize);
    v.delaySamples = juce::jmax(0, delay);
    v.envelope = 1.0f;
    v.releaseMul = 1.0f;
    v.palmAmount = juce::jlimit(0.0f, 1.0f, palmAmount);
    v.palmMul = v.palmAmount < 0.01f
        ? 1.0f
        : coefficientForSeconds(sampleRate, juce::jmap(v.palmAmount, 0.0f, 1.0f, 1.25f, 0.055f));
    v.active = true;
}

void GuitarEngine::strumVoicing(const std::array<int, 6>& notes,
                                float velocity,
                                bool down,
                                int baseDelaySamples,
                                float palmOverride)
{
    std::array<int, 6> order { 0, 1, 2, 3, 4, 5 };
    if (!down)
        std::reverse(order.begin(), order.end());

    int activeCount = 0;
    for (auto n : notes)
        if (n >= 0)
            ++activeCount;

    if (activeCount == 0)
        return;

    const float totalSamples = strumMs * 0.001f * (float) sampleRate;
    const float gap = activeCount > 1 ? totalSamples / (float) (activeCount - 1) : 0.0f;
    const float usePalm = palmOverride >= 0.0f ? palmOverride : palmMute;

    std::uniform_real_distribution<float> jitter(-1.0f, 1.0f);
    int played = 0;
    for (int idx : order)
    {
        if (notes[(size_t) idx] < 0)
            continue;

        const float humanSamples = humanize * 0.0035f * (float) sampleRate * jitter(rng);
        const int delay = juce::jmax(0, baseDelaySamples + (int) std::round((float) played * gap + humanSamples));
        const float pan = juce::jmap((float) idx, 0.0f, 5.0f, -0.34f, 0.34f);
        const float stringVelocity = juce::jlimit(0.02f, 1.0f,
            velocity * (1.0f + jitter(rng) * 0.025f * humanize));
        startVoice(notes[(size_t) idx], stringVelocity, delay, pan, usePalm);
        ++played;
    }
}

void GuitarEngine::pickString(const std::array<int, 6>& notes,
                              int stringIndex,
                              float velocity,
                              int delaySamples,
                              float palmOverride)
{
    if (stringIndex < 0 || stringIndex >= 6)
        return;

    int idx = stringIndex;
    if (notes[(size_t) idx] < 0)
    {
        for (int distance = 1; distance < 6; ++distance)
        {
            const int up = idx + distance;
            const int down = idx - distance;
            if (up < 6 && notes[(size_t) up] >= 0) { idx = up; break; }
            if (down >= 0 && notes[(size_t) down] >= 0) { idx = down; break; }
        }
    }

    if (notes[(size_t) idx] < 0)
        return;

    const float usePalm = palmOverride >= 0.0f ? palmOverride : palmMute;
    const float pan = juce::jmap((float) idx, 0.0f, 5.0f, -0.30f, 0.30f);
    startVoice(notes[(size_t) idx], velocity, delaySamples, pan, usePalm);
}

void GuitarEngine::scratch(bool upward,
                           float intensity,
                           float lengthSeconds,
                           int delaySamples)
{
    auto& t = getTransientVoice();
    t = {};
    t.kind = upward ? TransientKind::scratchUp : TransientKind::scratchDown;
    t.delaySamples = juce::jmax(0, delaySamples);
    t.totalSamples = juce::jmax(64, (int) std::round(sampleRate * juce::jlimit(0.025f, 0.35f, lengthSeconds)));
    t.remainingSamples = t.totalSamples;
    t.gain = juce::jlimit(0.0f, 1.0f, intensity) * 0.50f;
    t.tone = 0.65f;
    t.noiseState ^= (uint32_t) (delaySamples + t.totalSamples * 131);
    t.active = true;
}

void GuitarEngine::percussion(PercussionType type,
                              float velocity,
                              float toneAmount,
                              int delaySamples)
{
    auto& t = getTransientVoice();
    t = {};
    switch (type)
    {
        case PercussionType::body:    t.kind = TransientKind::body; break;
        case PercussionType::thumb:   t.kind = TransientKind::thumb; break;
        case PercussionType::knuckle: t.kind = TransientKind::knuckle; break;
        case PercussionType::slap:    t.kind = TransientKind::slap; break;
    }

    t.delaySamples = juce::jmax(0, delaySamples);
    const float length = type == PercussionType::body ? 0.22f
                       : type == PercussionType::thumb ? 0.16f
                       : type == PercussionType::knuckle ? 0.11f : 0.085f;
    t.totalSamples = juce::jmax(64, (int) std::round(sampleRate * length));
    t.remainingSamples = t.totalSamples;
    t.gain = juce::jlimit(0.0f, 1.0f, velocity) * 0.65f;
    t.tone = juce::jlimit(0.0f, 1.0f, toneAmount);
    t.noiseState ^= (uint32_t) (0x9E3779B9u + delaySamples * 17 + (int) type * 911);
    t.active = true;
}

void GuitarEngine::scheduleChoke(int delaySamples, float releaseSeconds)
{
    for (auto& e : chokeEvents)
    {
        if (!e.active)
        {
            e.delaySamples = juce::jmax(0, delaySamples);
            e.seconds = juce::jlimit(0.004f, 0.25f, releaseSeconds);
            e.active = true;
            return;
        }
    }

    chokeEvents.front() = { juce::jmax(0, delaySamples), juce::jlimit(0.004f, 0.25f, releaseSeconds), true };
}

void GuitarEngine::chokeNow(float releaseSeconds)
{
    releaseAll(releaseSeconds);
}

void GuitarEngine::releaseAll(float seconds)
{
    const float mul = coefficientForSeconds(sampleRate, seconds);
    for (auto& v : voices)
        if (v.active)
            v.releaseMul = mul;
}

void GuitarEngine::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (buffer.getNumChannels() < 1 || numSamples <= 0)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    std::fill(left, left + numSamples, 0.0f);
    if (right != left)
        std::fill(right, right + numSamples, 0.0f);

    for (int n = 0; n < numSamples; ++n)
    {
        for (auto& e : chokeEvents)
        {
            if (!e.active)
                continue;
            if (e.delaySamples > 0)
            {
                --e.delaySamples;
            }
            else
            {
                releaseAll(e.seconds);
                e.active = false;
            }
        }

        float l = 0.0f, r = 0.0f;

        for (auto& v : voices)
        {
            if (!v.active || v.sample == nullptr)
                continue;

            if (v.delaySamples > 0)
            {
                --v.delaySamples;
                continue;
            }

            const auto& audio = v.sample->audio;
            const int i0 = (int) v.position;
            if (i0 < 0 || i0 >= audio.getNumSamples() - 1)
            {
                v.active = false;
                continue;
            }

            const int i1 = i0 + 1;
            const float frac = (float) (v.position - (double) i0);
            const float raw = audio.getSample(0, i0)
                            + (audio.getSample(0, i1) - audio.getSample(0, i0)) * frac;
            v.position += v.step;

            const float cutoff = juce::jlimit(850.0f, 17000.0f,
                1600.0f + 15000.0f * tone * (1.0f - 0.78f * v.palmAmount));
            const float pole = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            v.lowpass = (1.0f - pole) * raw + pole * v.lowpass;

            v.envelope *= v.releaseMul;
            v.envelope *= v.palmMul;
            if (v.envelope < 0.00012f)
            {
                v.active = false;
                continue;
            }

            const float s = v.lowpass * v.gain * v.envelope;
            const float pan01 = (v.pan + 1.0f) * 0.5f;
            l += s * std::sqrt(1.0f - pan01);
            r += s * std::sqrt(pan01);
        }

        for (auto& t : transientVoices)
        {
            if (!t.active)
                continue;

            if (t.delaySamples > 0)
            {
                --t.delaySamples;
                continue;
            }

            if (t.remainingSamples <= 0)
            {
                t.active = false;
                continue;
            }

            const float progress = 1.0f - (float) t.remainingSamples / (float) juce::jmax(1, t.totalSamples);
            float sample = 0.0f;

            if (t.kind == TransientKind::scratchDown || t.kind == TransientKind::scratchUp)
            {
                const float noise = nextNoise(t.noiseState);
                const float sweep = t.kind == TransientKind::scratchDown ? progress : (1.0f - progress);
                const float coeff = juce::jmap(sweep, 0.0f, 1.0f, 0.04f, 0.34f);
                t.filter += coeff * (noise - t.filter);
                const float env = std::sin(juce::MathConstants<float>::pi * progress);
                sample = (noise - 0.48f * t.filter) * env * t.gain;
            }
            else
            {
                float baseHz = 95.0f;
                float noiseAmount = 0.08f;
                switch (t.kind)
                {
                    case TransientKind::body:    baseHz = juce::jmap(t.tone, 0.0f, 1.0f, 72.0f, 125.0f); noiseAmount = 0.035f; break;
                    case TransientKind::thumb:   baseHz = juce::jmap(t.tone, 0.0f, 1.0f, 60.0f, 105.0f); noiseAmount = 0.06f; break;
                    case TransientKind::knuckle: baseHz = juce::jmap(t.tone, 0.0f, 1.0f, 145.0f, 260.0f); noiseAmount = 0.16f; break;
                    case TransientKind::slap:    baseHz = juce::jmap(t.tone, 0.0f, 1.0f, 190.0f, 360.0f); noiseAmount = 0.36f; break;
                    default: break;
                }

                t.phase += 2.0f * juce::MathConstants<float>::pi * baseHz / (float) sampleRate;
                if (t.phase > juce::MathConstants<float>::twoPi)
                    t.phase -= juce::MathConstants<float>::twoPi;
                const float env = std::exp(-7.5f * progress);
                const float click = nextNoise(t.noiseState) * noiseAmount * std::exp(-24.0f * progress);
                sample = (std::sin(t.phase) * env + click) * t.gain;
            }

            l += sample * 0.72f;
            r += sample * 0.72f;
            --t.remainingSamples;
        }

        left[n] += l;
        right[n] += r;
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.10f + room * 0.48f;
    rp.damping = 0.50f;
    rp.wetLevel = room * 0.22f;
    rp.dryLevel = 1.0f;
    rp.width = 0.82f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    if (buffer.getNumChannels() > 1)
        reverb.processStereo(left, right, numSamples);
    else
        reverb.processMono(left, numSamples);

    buffer.applyGain(juce::Decibels::decibelsToGain(outputDb));
}
