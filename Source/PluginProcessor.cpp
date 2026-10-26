#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"
#include "Presets.h"

#include <algorithm>
#include <cmath>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id,
                                                     const char* name,
                                                     juce::NormalisableRange<float> range,
                                                     float defaultValue,
                                                     const char* label)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 },
        name,
        range,
        defaultValue,
        juce::AudioParameterFloatAttributes().withLabel (label));
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id,
                                                    const char* name,
                                                    bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue);
}

juce::StringArray modeNames()
{
    return { "SILICON", "GERMANIUM", "VELCRO", "DOOM" };
}
} // namespace

Fuzzion5000AudioProcessor::Fuzzion5000AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    inputParam = apvts.getRawParameterValue (fuzzion5000::params::input);
    fuzzParam = apvts.getRawParameterValue (fuzzion5000::params::fuzz);
    biasParam = apvts.getRawParameterValue (fuzzion5000::params::bias);
    toneParam = apvts.getRawParameterValue (fuzzion5000::params::tone);
    gateParam = apvts.getRawParameterValue (fuzzion5000::params::gate);
    octaveParam = apvts.getRawParameterValue (fuzzion5000::params::octave);
    mixParam = apvts.getRawParameterValue (fuzzion5000::params::mix);
    outputParam = apvts.getRawParameterValue (fuzzion5000::params::output);
    modeParam = apvts.getRawParameterValue (fuzzion5000::params::mode);
    toneStackParam = apvts.getRawParameterValue (fuzzion5000::params::toneStack);
    gateEnabledParam = apvts.getRawParameterValue (fuzzion5000::params::gateEnabled);
    ironParam = apvts.getRawParameterValue (fuzzion5000::params::iron);
    bypassParam = apvts.getRawParameterValue (fuzzion5000::params::bypass);

    for (const auto* parameterID : fuzzion5000::params::allParameterIDs())
        apvts.addParameterListener (parameterID, this);
}

Fuzzion5000AudioProcessor::~Fuzzion5000AudioProcessor()
{
    for (const auto* parameterID : fuzzion5000::params::allParameterIDs())
        apvts.removeParameterListener (parameterID, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
Fuzzion5000AudioProcessor::createParameterLayout()
{
    using namespace fuzzion5000::params;

    APVTS::ParameterLayout layout;

    auto fuzzGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("fuzz", "Fuzz Core", " | ");
    fuzzGroup->addChild (makeFloat (input, "Input Trim", { -18.0f, 18.0f, 0.01f }, defaultInput, "dB"));
    fuzzGroup->addChild (makeFloat (fuzz, "Fuzz", { 0.0f, 100.0f, 0.1f }, defaultFuzz, "%"));
    fuzzGroup->addChild (makeFloat (bias, "Bias Starve", { -100.0f, 100.0f, 0.1f }, defaultBias, "%"));
    fuzzGroup->addChild (makeFloat (octave, "Octave Up", { 0.0f, 100.0f, 0.1f }, defaultOctave, "%"));
    fuzzGroup->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { mode, 1 }, "Transistor Mode", modeNames(), defaultMode));
    layout.add (std::move (fuzzGroup));

    auto toneGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("tone", "Tone & Gate", " | ");
    toneGroup->addChild (makeFloat (tone, "Tone", { 0.0f, 100.0f, 0.1f }, defaultTone, "%"));
    toneGroup->addChild (makeFloat (gate, "Gate Threshold", { 0.0f, 100.0f, 0.1f }, defaultGate, "%"));
    toneGroup->addChild (makeBool (toneStack, "Tone Stack", defaultToneStack));
    toneGroup->addChild (makeBool (gateEnabled, "Gate", defaultGateEnabled));
    toneGroup->addChild (makeBool (iron, "Iron Output", defaultIron));
    layout.add (std::move (toneGroup));

    auto outputGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("output", "Output", " | ");
    outputGroup->addChild (makeFloat (mix, "Parallel Mix", { 0.0f, 100.0f, 0.1f }, defaultMix, "%"));
    outputGroup->addChild (makeFloat (output, "Output Level", { -24.0f, 12.0f, 0.01f }, defaultOutput, "dB"));
    outputGroup->addChild (makeBool (bypass, "Bypass", defaultBypass));
    layout.add (std::move (outputGroup));

    return layout;
}

void Fuzzion5000AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = std::max (8000.0, sampleRate);
    preparedBlockSize = std::max (1, samplesPerBlock);

    oversampler.initProcessing (static_cast<size_t> (preparedBlockSize));
    oversamplingFactor = static_cast<int> (oversampler.getOversamplingFactor());

    const double oversampledRate = currentSampleRate * static_cast<double> (oversamplingFactor);

    voiceLeft.prepare (oversampledRate);
    voiceRight.prepare (oversampledRate);

    setLatencySamples (juce::roundToInt (oversampler.getLatencyInSamples()));

    dryBuffer.setSize (processingChannels, preparedBlockSize * oversamplingFactor, false, false, true);

    outputGainSmoothed.reset (oversampledRate, 0.020);
    mixSmoothed.reset (oversampledRate, 0.025);

    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setCurrentAndTargetValue (bypassParam->load() > 0.5f ? 0.0f : mixParam->load() * 0.01f);

    inputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (0.0f, std::memory_order_relaxed);
    outputLevelRight.store (0.0f, std::memory_order_relaxed);
    saturation.store (0.0f, std::memory_order_relaxed);
    gateReduction.store (0.0f, std::memory_order_relaxed);

    // Keep roughly 750 waveform columns per second at any sample rate, which
    // gives the display a ~0.34 second window at its 256-column view.
    waveformSamplesPerColumn = juce::jmax (8, juce::roundToInt (currentSampleRate / 750.0));
    resetWaveformHistory();
}

void Fuzzion5000AudioProcessor::resetWaveformHistory() noexcept
{
    for (auto& value : waveformMin)
        value.store (0.0f, std::memory_order_relaxed);

    for (auto& value : waveformMax)
        value.store (0.0f, std::memory_order_relaxed);

    waveformWriteCount.store (0, std::memory_order_relaxed);
    waveformAccumulatedSamples = 0;
    waveformAccumulatedMin = 0.0f;
    waveformAccumulatedMax = 0.0f;
}

void Fuzzion5000AudioProcessor::captureWaveform (const juce::AudioBuffer<float>& buffer,
                                                 int numSamples) noexcept
{
    const int columnSize = juce::jmax (1, waveformSamplesPerColumn);

    auto sanitise = [] (float value)
    {
        return std::isfinite (value) ? juce::jlimit (-1.0f, 1.0f, value) : 0.0f;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const float left = sanitise (buffer.getSample (0, i));
        const float right = sanitise (buffer.getSample (1, i));
        const float columnMin = juce::jmin (left, right);
        const float columnMax = juce::jmax (left, right);

        if (waveformAccumulatedSamples == 0)
        {
            waveformAccumulatedMin = columnMin;
            waveformAccumulatedMax = columnMax;
        }
        else
        {
            waveformAccumulatedMin = juce::jmin (waveformAccumulatedMin, columnMin);
            waveformAccumulatedMax = juce::jmax (waveformAccumulatedMax, columnMax);
        }

        if (++waveformAccumulatedSamples < columnSize)
            continue;

        const auto writeIndex = waveformWriteCount.load (std::memory_order_relaxed);
        const auto slot = static_cast<size_t> (writeIndex % static_cast<juce::uint32> (waveformCapacity));
        waveformMin[slot].store (waveformAccumulatedMin, std::memory_order_relaxed);
        waveformMax[slot].store (waveformAccumulatedMax, std::memory_order_relaxed);
        waveformWriteCount.store (writeIndex + 1, std::memory_order_release);

        waveformAccumulatedSamples = 0;
    }
}

int Fuzzion5000AudioProcessor::copyWaveform (float* minColumns, float* maxColumns,
                                             int maxColumnCount) const noexcept
{
    if (minColumns == nullptr || maxColumns == nullptr || maxColumnCount <= 0)
        return 0;

    const auto written = waveformWriteCount.load (std::memory_order_acquire);
    const int available = static_cast<int> (
        juce::jmin<juce::uint32> (written, static_cast<juce::uint32> (waveformCapacity)));
    const int count = juce::jmin (available, maxColumnCount);
    const auto start = written - static_cast<juce::uint32> (count);

    for (int i = 0; i < count; ++i)
    {
        const auto slot = static_cast<size_t> ((start + static_cast<juce::uint32> (i))
                                               % static_cast<juce::uint32> (waveformCapacity));
        minColumns[i] = waveformMin[slot].load (std::memory_order_relaxed);
        maxColumns[i] = waveformMax[slot].load (std::memory_order_relaxed);
    }

    return count;
}

void Fuzzion5000AudioProcessor::releaseResources()
{
    voiceLeft.reset();
    voiceRight.reset();
    outputGainSmoothed.reset (currentSampleRate);
    mixSmoothed.reset (currentSampleRate);
}

void Fuzzion5000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    if (numSamples == 0)
        return;

    if (buffer.getNumChannels() < processingChannels || numSamples > preparedBlockSize)
    {
        buffer.clear();
        return;
    }

    for (int ch = processingChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float inputPeak = buffer.getMagnitude (0, numSamples);

    const juce::dsp::AudioBlock<const float> inputBlock (buffer.getArrayOfReadPointers(),
                                                         processingChannels,
                                                         static_cast<size_t> (numSamples));
    auto oversampledBlock = oversampler.processSamplesUp (inputBlock);

    const int osSamples = static_cast<int> (oversampledBlock.getNumSamples());

    if (osSamples > dryBuffer.getNumSamples())
    {
        buffer.clear();
        return;
    }

    for (int ch = 0; ch < processingChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, oversampledBlock.getChannelPointer (static_cast<size_t> (ch)), osSamples);

    auto* left = oversampledBlock.getChannelPointer (0);
    auto* right = oversampledBlock.getChannelPointer (1);

    const bool bypassed = bypassParam->load() > 0.5f;
    const int mode = juce::jlimit (0, fuzzion::dsp::numModes - 1, juce::roundToInt (modeParam->load()));

    fuzzion::dsp::FuzzVoice::Parameters coreParameters;
    coreParameters.inputGain = juce::Decibels::decibelsToGain (inputParam->load());
    coreParameters.drive = fuzzion::dsp::fuzzDriveGain (fuzzParam->load() * 0.01f);
    coreParameters.bias = biasParam->load() * 0.01f;
    coreParameters.tone = toneParam->load() * 0.01f;
    coreParameters.octave = octaveParam->load() * 0.01f;
    coreParameters.gate = gateParam->load() * 0.01f;
    coreParameters.gateEnabled = gateEnabledParam->load() > 0.5f;
    coreParameters.toneStack = toneStackParam->load() > 0.5f;
    coreParameters.iron = ironParam->load() > 0.5f;
    coreParameters.mode = mode;

    voiceLeft.setParameters (coreParameters, osSamples);
    voiceRight.setParameters (coreParameters, osSamples);

    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSmoothed.setTargetValue (bypassed ? 0.0f : mixParam->load() * 0.01f);

    float peakSaturation = 0.0f;
    float peakGateReduction = 0.0f;

    for (int i = 0; i < osSamples; ++i)
    {
        const float outputGain = outputGainSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        const float wetLeft = voiceLeft.processSample (left[i]) * outputGain;
        const float wetRight = voiceRight.processSample (right[i]) * outputGain;

        peakSaturation = juce::jmax (peakSaturation, voiceLeft.getSaturation(), voiceRight.getSaturation());
        peakGateReduction = juce::jmax (peakGateReduction,
                                        voiceLeft.getGateReduction(), voiceRight.getGateReduction());

        // Dry and wet are mixed at the oversampled rate, so the whole plugin
        // (including the bypass path) shares the converter latency exactly.
        const float dryLeft = dryBuffer.getSample (0, i);
        const float dryRight = dryBuffer.getSample (1, i);

        left[i] = dryLeft + mix * (wetLeft - dryLeft);
        right[i] = dryRight + mix * (wetRight - dryRight);
    }

    juce::dsp::AudioBlock<float> outputBlock (buffer.getArrayOfWritePointers(),
                                              processingChannels,
                                              static_cast<size_t> (numSamples));
    oversampler.processSamplesDown (outputBlock);

    const float outputLeftPeak = buffer.getMagnitude (0, 0, numSamples);
    const float outputRightPeak = buffer.getMagnitude (1, 0, numSamples);
    const float outputPeak = juce::jmax (outputLeftPeak, outputRightPeak);

    if (bypassed)
    {
        peakSaturation = 0.0f;
        peakGateReduction = 0.0f;
    }

    inputLevel.store (std::isfinite (inputPeak) ? inputPeak : 0.0f, std::memory_order_relaxed);
    outputLevel.store (std::isfinite (outputPeak) ? outputPeak : 0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (std::isfinite (outputLeftPeak) ? outputLeftPeak : 0.0f, std::memory_order_relaxed);
    outputLevelRight.store (std::isfinite (outputRightPeak) ? outputRightPeak : 0.0f, std::memory_order_relaxed);
    saturation.store (std::isfinite (peakSaturation) ? peakSaturation : 0.0f, std::memory_order_relaxed);
    gateReduction.store (std::isfinite (peakGateReduction) ? peakGateReduction : 0.0f,
                         std::memory_order_relaxed);

    captureWaveform (buffer, numSamples);
}

juce::AudioProcessorEditor* Fuzzion5000AudioProcessor::createEditor()
{
    return new Fuzzion5000AudioProcessorEditor (*this);
}

int Fuzzion5000AudioProcessor::getNumPrograms()
{
    return fuzzion5000::getNumFactoryPresets();
}

int Fuzzion5000AudioProcessor::getCurrentProgram()
{
    return currentPreset.load (std::memory_order_relaxed);
}

void Fuzzion5000AudioProcessor::setCurrentProgram (int index)
{
    loadFactoryPreset (index);
}

const juce::String Fuzzion5000AudioProcessor::getProgramName (int index)
{
    return fuzzion5000::getFactoryPresetName (index);
}

void Fuzzion5000AudioProcessor::changeProgramName (int, const juce::String&)
{
}

void Fuzzion5000AudioProcessor::loadFactoryPreset (int index)
{
    applyPreset (index);
}

void Fuzzion5000AudioProcessor::applyPreset (int index)
{
    const auto& presets = fuzzion5000::getFactoryPresets();

    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    applyingPreset.store (true, std::memory_order_relaxed);

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& value : presets[static_cast<size_t> (index)].overrides)
    {
        if (auto* parameter = apvts.getParameter (value.id))
        {
            const auto& range = parameter->getNormalisableRange();
            const float deNormalised = juce::jlimit (range.start, range.end, value.value);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (deNormalised));
        }
    }

    currentPreset.store (index, std::memory_order_relaxed);
    presetDirty.store (false, std::memory_order_relaxed);
    applyingPreset.store (false, std::memory_order_relaxed);
}

void Fuzzion5000AudioProcessor::parameterChanged (const juce::String&, float)
{
    if (! applyingPreset.load (std::memory_order_relaxed))
        presetDirty.store (true, std::memory_order_relaxed);
}

void Fuzzion5000AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPreset.load (std::memory_order_relaxed), nullptr);
    state.setProperty ("presetDirty", presetDirty.load (std::memory_order_relaxed), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Fuzzion5000AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    const int presetIndex = static_cast<int> (tree.getProperty ("presetIndex", 0));
    const bool dirty = static_cast<bool> (tree.getProperty ("presetDirty", false));

    applyingPreset.store (true, std::memory_order_relaxed);
    apvts.replaceState (tree);
    applyingPreset.store (false, std::memory_order_relaxed);

    currentPreset.store (juce::jlimit (0, fuzzion5000::getNumFactoryPresets() - 1, presetIndex),
                         std::memory_order_relaxed);
    presetDirty.store (dirty, std::memory_order_relaxed);
}

juce::AudioProcessorParameter* Fuzzion5000AudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (fuzzion5000::params::bypass);
}

bool Fuzzion5000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::stereo();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Fuzzion5000AudioProcessor();
}
