#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "ui/StudioLookAndFeel.h"

#include <memory>

class Fuzzion5000EditorContent;

class Fuzzion5000AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit Fuzzion5000AudioProcessorEditor (Fuzzion5000AudioProcessor&);
    ~Fuzzion5000AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Fuzzion5000AudioProcessor& processor;
    studio::ui::StudioLookAndFeel lookAndFeel;
    std::unique_ptr<Fuzzion5000EditorContent> content;

#if ! defined (FUZZION5000_SCREENSHOT)
    juce::TooltipWindow tooltipWindow { this, 650 };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fuzzion5000AudioProcessorEditor)
};
