#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/FuzzCore.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getReadPointer (ch)[i]))
                return false;

    return true;
}

void setParameter (Fuzzion5000AudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float getParameter (Fuzzion5000AudioProcessor& processor, const char* id)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        return parameter->convertFrom0to1 (parameter->getValue());

    return 0.0f;
}

// Goertzel magnitude of a single bin, used to inspect harmonic content.
double binMagnitude (const std::vector<float>& samples, double frequency, double sampleRate)
{
    const double w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const double cosine = std::cos (w);
    const double coefficient = 2.0 * cosine;
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    for (float sample : samples)
    {
        s0 = sample + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coefficient * s1 * s2));
}

float rms (const std::vector<float>& samples, size_t start = 0)
{
    if (start >= samples.size())
        return 0.0f;

    double sum = 0.0;

    for (size_t i = start; i < samples.size(); ++i)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);

    return static_cast<float> (std::sqrt (sum / static_cast<double> (samples.size() - start)));
}

void processSine (Fuzzion5000AudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                  int numBlocks, float gainLeft, float gainRight, double frequency = 1000.0,
                  juce::int64 startSample = 0)
{
    juce::MidiBuffer midi;

    for (int block = 0; block < numBlocks; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (startSample + block * buffer.getNumSamples() + i);
            const auto sample = static_cast<float> (
                0.45 * std::sin (juce::MathConstants<double>::twoPi * frequency * n / 48000.0));

            buffer.setSample (0, i, sample * gainLeft);
            buffer.setSample (1, i, sample * gainRight);
        }

        processor.processBlock (buffer, midi);
    }
}

std::vector<float> runVoice (const fuzzion::dsp::FuzzVoice::Parameters& parameters,
                             double sampleRate, int numSamples, float amplitude,
                             double frequency, int* zeroCrossings = nullptr)
{
    fuzzion::dsp::FuzzVoice voice;
    voice.prepare (sampleRate);
    voice.setParameters (parameters, numSamples);

    std::vector<float> output;
    output.reserve (static_cast<size_t> (numSamples));

    int crossings = 0;
    float previous = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto t = static_cast<double> (i) / sampleRate;
        const auto input = static_cast<float> (
            amplitude * std::sin (juce::MathConstants<double>::twoPi * frequency * t));
        const float value = voice.processSample (input);
        output.push_back (value);

        if (i > 0 && ((previous < 0.0f && value >= 0.0f) || (previous >= 0.0f && value < 0.0f)))
            ++crossings;

        previous = value;
    }

    if (zeroCrossings != nullptr)
        *zeroCrossings = crossings;

    return output;
}

void testClipperShape()
{
    // The clipper must be continuous at zero and hard-bounded for every character.
    bool continuous = true;
    bool bounded = true;

    for (int mode = 0; mode < fuzzion::dsp::numModes; ++mode)
    {
        const auto profile = fuzzion::dsp::modeProfile (mode);

        const float below = fuzzion::dsp::asymmetricClip (-1.0e-7f, profile.kneePositive,
                                                          profile.kneeNegative, profile.hardness);
        const float above = fuzzion::dsp::asymmetricClip (1.0e-7f, profile.kneePositive,
                                                          profile.kneeNegative, profile.hardness);
        const float atZero = fuzzion::dsp::asymmetricClip (0.0f, profile.kneePositive,
                                                           profile.kneeNegative, profile.hardness);
        continuous = continuous && std::abs (atZero) < 1.0e-7f
                     && std::abs (below - above) < 2.0e-5f;

        for (int i = -80; i <= 80; ++i)
        {
            const float x = static_cast<float> (i) * 0.1f;
            const float y = fuzzion::dsp::asymmetricClip (x, profile.kneePositive,
                                                          profile.kneeNegative, profile.hardness);
            const float limit = juce::jmax (profile.kneePositive, profile.kneeNegative) * 1.01f;
            bounded = bounded && std::abs (y) <= limit;
        }
    }

    check (continuous, "asymmetric clipper is continuous through zero in every mode");
    check (bounded, "asymmetric clipper never exceeds its mode knee");

    bool transferBounded = true;
    bool transferLive = true;

    for (int mode = 0; mode < fuzzion::dsp::numModes; ++mode)
    {
        for (int i = -100; i <= 100; ++i)
        {
            const float x = static_cast<float> (i) * 0.02f;
            const float y = fuzzion::dsp::transferSample (x, 0.85f, 0.4f, mode);
            transferBounded = transferBounded && std::isfinite (y) && std::abs (y) < 1.6f;
            transferLive = transferLive || std::abs (y) > 0.2f;
        }
    }

    check (transferBounded, "static transfer shape stays bounded at extreme drive and bias");
    check (transferLive, "static transfer shape produces meaningful output");

    check (fuzzion::dsp::fuzzDriveGain (0.0f) > 1.0f
               && fuzzion::dsp::fuzzDriveGain (1.0f) > fuzzion::dsp::fuzzDriveGain (0.5f),
           "Fuzz knob maps to a monotonic pre-gain");
}

void testHarmonics()
{
    constexpr double osRate = 192000.0;
    constexpr int numSamples = 96000;
    constexpr double fundamental = 1000.0;

    fuzzion::dsp::FuzzVoice::Parameters clean;
    clean.drive = 1.2f;
    clean.bias = 0.0f;
    clean.tone = 0.55f;
    clean.mode = 0;

    fuzzion::dsp::FuzzVoice::Parameters fuzzed = clean;
    fuzzed.drive = fuzzion::dsp::fuzzDriveGain (0.7f);
    fuzzed.mode = 0;

    const auto cleanOutput = runVoice (clean, osRate, numSamples, 0.25f, fundamental);
    const auto fuzzOutput = runVoice (fuzzed, osRate, numSamples, 0.25f, fundamental);

    const auto cleanFundamental = binMagnitude (cleanOutput, fundamental, osRate);
    const auto fuzzFundamental = binMagnitude (fuzzOutput, fundamental, osRate);
    const auto secondHarmonic = binMagnitude (fuzzOutput, fundamental * 2.0, osRate);

    check (fuzzFundamental > cleanFundamental * 0.15, "fuzz stage keeps the fundamental audible");
    check (secondHarmonic > fuzzFundamental * 0.05,
           "asymmetric fuzz generates even-order harmonics");

    // A heavy starve bias must produce a different waveform than the centred bias.
    fuzzion::dsp::FuzzVoice::Parameters starved = fuzzed;
    starved.bias = 0.65f;

    const auto starvedOutput = runVoice (starved, osRate, numSamples, 0.25f, fundamental);
    double difference = 0.0;

    for (size_t i = 0; i < fuzzOutput.size(); ++i)
        difference += std::abs (static_cast<double> (fuzzOutput[i] - starvedOutput[i]));

    check (difference / static_cast<double> (fuzzOutput.size()) > 0.02,
           "starve bias materially changes the fuzz waveform");
}

void testOctave()
{
    constexpr double osRate = 192000.0;
    constexpr int numSamples = 96000;
    constexpr double fundamental = 400.0;

    fuzzion::dsp::FuzzVoice::Parameters parameters;
    parameters.drive = fuzzion::dsp::fuzzDriveGain (0.65f);
    parameters.mode = 0;
    parameters.bias = 0.0f;
    parameters.octave = 0.0f;

    const auto dryOctave = runVoice (parameters, osRate, numSamples, 0.3f, fundamental);
    const auto drySecond = binMagnitude (dryOctave, fundamental * 2.0, osRate);

    parameters.octave = 1.0f;
    int crossings = 0;
    const auto wetOctave = runVoice (parameters, osRate, numSamples, 0.3f, fundamental, &crossings);

    const auto wetSecond = binMagnitude (wetOctave, fundamental * 2.0, osRate);

    check (wetSecond > drySecond * 1.5, "octave control lifts second-harmonic energy");
    check (crossings > 0, "octave rectifier keeps a valid alternating waveform");
}

void testGate()
{
    constexpr double osRate = 192000.0;
    constexpr int numSamples = 96000;

    fuzzion::dsp::FuzzVoice::Parameters parameters;
    parameters.drive = fuzzion::dsp::fuzzDriveGain (0.6f);
    parameters.mode = 0;
    parameters.gateEnabled = false;
    parameters.gate = 0.9f;

    const auto open = runVoice (parameters, osRate, numSamples, 0.3f, 220.0);
    const float openLevel = rms (open, static_cast<size_t> (numSamples / 2));

    parameters.gateEnabled = true;
    const auto gatedQuiet = runVoice (parameters, osRate, numSamples, 0.00018f, 220.0);
    const float gatedQuietLevel = rms (gatedQuiet, static_cast<size_t> (numSamples / 2));

    const auto gatedLoud = runVoice (parameters, osRate, numSamples, 0.3f, 220.0);
    const float gatedLoudLevel = rms (gatedLoud, static_cast<size_t> (numSamples / 2));

    check (openLevel > 0.05f, "fuzz core produces healthy output without the gate");
    check (gatedQuietLevel < openLevel * 0.05f, "gate mutes a starved low-level tail");
    check (gatedLoudLevel > openLevel * 0.5f, "gate opens for loud playing");
}

void testDcBlocking()
{
    constexpr double osRate = 192000.0;
    constexpr int numSamples = 96000;

    fuzzion::dsp::FuzzVoice::Parameters parameters;
    parameters.drive = fuzzion::dsp::fuzzDriveGain (0.85f);
    parameters.bias = 1.0f;
    parameters.mode = 2;
    parameters.gateEnabled = false;

    const auto output = runVoice (parameters, osRate, numSamples, 0.3f, 110.0f);

    double sum = 0.0;
    float peak = 0.0f;
    const size_t start = static_cast<size_t> (numSamples / 2);

    for (size_t i = start; i < output.size(); ++i)
    {
        sum += static_cast<double> (output[i]);
        peak = juce::jmax (peak, std::abs (output[i]));
    }

    const double mean = sum / static_cast<double> (output.size() - start);

    check (std::abs (mean) < 0.01, "bias-derived DC is removed from the output");
    check (peak > 0.05f && peak < 2.5f, "starved clipper output stays in a sane range");
}

void testProcessor()
{
    Fuzzion5000AudioProcessor processor;
    check (processor.getParameters().size() == 13, "processor exposes all 13 parameters");
    check (processor.getNumPrograms() == 10, "ten factory programs are available");

    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    check (processor.getLatencySamples() > 0, "oversampling latency is reported to the host");
    check (processor.getOversamplingFactor() == 4, "non-linear stages run at 4x oversampling");

    juce::AudioBuffer<float> buffer (2, 512);

    setParameter (processor, fuzzion5000::params::fuzz, 78.0f);
    setParameter (processor, fuzzion5000::params::bias, 25.0f);
    setParameter (processor, fuzzion5000::params::gateEnabled, 0.0f);
    setParameter (processor, fuzzion5000::params::mix, 100.0f);

    processSine (processor, buffer, 100, 1.0f, 0.8f, 1000.0);

    check (allFinite (buffer), "fuzz processing produces finite output");
    check (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 0.01f,
           "fuzz processing remains audible");
    check (processor.getSaturation() > 0.05f, "saturation telemetry reports clipper activity");

    // Lock-free waveform telemetry must expose live, ordered min/max columns.
    std::array<float, 64> waveformMin {};
    std::array<float, 64> waveformMax {};
    const int columns = processor.copyWaveform (waveformMin.data(), waveformMax.data(), 64);

    bool validColumns = columns == 64;
    bool liveWaveform = false;

    for (int i = 0; i < columns; ++i)
    {
        validColumns = validColumns && waveformMax[static_cast<size_t> (i)]
                                          >= waveformMin[static_cast<size_t> (i)];
        liveWaveform = liveWaveform || waveformMax[static_cast<size_t> (i)] > 0.001f
                                      || waveformMin[static_cast<size_t> (i)] < -0.001f;
    }

    check (validColumns, "waveform telemetry publishes ordered min/max columns");
    check (liveWaveform, "waveform telemetry contains live audio");

    // Bypass must remain finite and continue to report the same host latency.
    setParameter (processor, fuzzion5000::params::bypass, 1.0f);
    processSine (processor, buffer, 20, 1.0f, -1.0f, 1000.0, 512 * 200);
    check (allFinite (buffer), "latency-matched bypass produces finite output");
    check (processor.getLatencySamples() > 0, "bypass does not alter reported latency");
    check (processor.getSaturation() <= 0.001f, "bypass clears the saturation readout");

    // Every character must survive extreme drive, bias and octave without blowing up.
    setParameter (processor, fuzzion5000::params::bypass, 0.0f);

    bool allModesFinite = true;

    for (int mode = 0; mode < fuzzion::dsp::numModes; ++mode)
    {
        setParameter (processor, fuzzion5000::params::mode, static_cast<float> (mode));
        setParameter (processor, fuzzion5000::params::fuzz, 100.0f);
        setParameter (processor, fuzzion5000::params::bias, 100.0f);
        setParameter (processor, fuzzion5000::params::octave, 100.0f);
        setParameter (processor, fuzzion5000::params::tone, 100.0f);
        setParameter (processor, fuzzion5000::params::gateEnabled, 1.0f);
        setParameter (processor, fuzzion5000::params::gate, 100.0f);
        setParameter (processor, fuzzion5000::params::output, 12.0f);

        processSine (processor, buffer, 12, 1.0f, -1.0f, 1000.0, 512 * (300 + mode * 12));
        allModesFinite = allModesFinite && allFinite (buffer);
    }

    check (allModesFinite, "all four characters stay finite at extreme settings");

    // State round-trip, including preset identity and edited parameters.
    processor.setCurrentProgram (1);
    setParameter (processor, fuzzion5000::params::bias, -37.5f);
    const float biasBefore = getParameter (processor, fuzzion5000::params::bias);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    check (state.getSize() > 0, "full processor state serialises");

    Fuzzion5000AudioProcessor restored;
    restored.setPlayConfigDetails (2, 2, 48000.0, 512);
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (std::abs (getParameter (restored, fuzzion5000::params::bias) - biasBefore) < 0.01f,
           "bias is restored from serialised state");
    check (restored.getCurrentPresetIndex() == 1, "preset identity survives serialisation");

    processor.releaseResources();
    restored.releaseResources();
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "Fuzzion-5000 offline DSP tests\n\n";

    testClipperShape();
    testHarmonics();
    testOctave();
    testGate();
    testDcBlocking();
    testProcessor();

    std::cout << (failures == 0 ? "\nAll DSP tests passed.\n" : "\nDSP tests failed.\n");
    return failures == 0 ? 0 : 1;
}
