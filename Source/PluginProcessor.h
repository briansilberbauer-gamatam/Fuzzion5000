#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/FuzzCore.h"

#include <array>
#include <atomic>
#include <memory>

class Fuzzion5000AudioProcessor final : public juce::AudioProcessor,
                                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    Fuzzion5000AudioProcessor();
    ~Fuzzion5000AudioProcessor() override;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fuzzion-5000"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Let the host's own bypass control drive the front-panel BYPASS parameter
    // instead of the wrapper maintaining a second, unlinked bypass flag.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

    void loadFactoryPreset (int index);

    int getCurrentPresetIndex() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
    bool isPresetDirty() const noexcept { return presetDirty.load (std::memory_order_relaxed); }

    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }
    float getOutputLevelLeft() const noexcept { return outputLevelLeft.load (std::memory_order_relaxed); }
    float getOutputLevelRight() const noexcept { return outputLevelRight.load (std::memory_order_relaxed); }
    float getSaturation() const noexcept { return saturation.load (std::memory_order_relaxed); }
    float getGateReduction() const noexcept { return gateReduction.load (std::memory_order_relaxed); }
    int getOversamplingFactor() const noexcept { return oversamplingFactor; }

    // Lock-free display history for the central waveform screen. The audio thread
    // publishes min/max columns; the message thread copies them in chronological
    // order without locking or allocating.
    static constexpr int waveformCapacity = 1024;
    int copyWaveform (float* minColumns, float* maxColumns, int maxColumnCount) const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyPreset (int index);
    void resetWaveformHistory() noexcept;
    void captureWaveform (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    static constexpr int processingChannels = 2;
    static constexpr int oversamplingLog2 = 2; // 4x

    juce::AudioProcessorValueTreeState apvts;

    juce::dsp::Oversampling<float> oversampler {
        static_cast<size_t> (processingChannels),
        static_cast<size_t> (oversamplingLog2),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false,
        true
    };

    fuzzion::dsp::FuzzVoice voiceLeft;
    fuzzion::dsp::FuzzVoice voiceRight;

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* fuzzParam = nullptr;
    std::atomic<float>* biasParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* toneStackParam = nullptr;
    std::atomic<float>* gateEnabledParam = nullptr;
    std::atomic<float>* ironParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    std::atomic<int> currentPreset { 0 };
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> applyingPreset { false };

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };
    std::atomic<float> saturation { 0.0f };
    std::atomic<float> gateReduction { 0.0f };

    std::array<std::atomic<float>, waveformCapacity> waveformMin {};
    std::array<std::atomic<float>, waveformCapacity> waveformMax {};
    std::atomic<juce::uint32> waveformWriteCount { 0 };
    int waveformSamplesPerColumn = 64;
    int waveformAccumulatedSamples = 0;
    float waveformAccumulatedMin = 0.0f;
    float waveformAccumulatedMax = 0.0f;

    double currentSampleRate = 48000.0;
    int preparedBlockSize = 512;
    int oversamplingFactor = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fuzzion5000AudioProcessor)
};
