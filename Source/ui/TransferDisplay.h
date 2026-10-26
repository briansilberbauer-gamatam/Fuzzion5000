#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <atomic>

namespace studio::ui
{
    // Slim inset oscilloscope that plots the live clipper transfer shape and the
    // bias operating point, so the starve control is visible as well as audible.
    class TransferDisplay final : public juce::Component,
                                  private juce::Timer
    {
    public:
        explicit TransferDisplay (Fuzzion5000AudioProcessor&);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        Fuzzion5000AudioProcessor& processor;
        const std::atomic<float>* fuzzParameter = nullptr;
        const std::atomic<float>* biasParameter = nullptr;
        const std::atomic<float>* modeParameter = nullptr;

        float fuzzValue = 0.55f;
        float biasValue = 0.0f;
        float inputProbe = 0.0f;
        int mode = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransferDisplay)
    };
} // namespace studio::ui
