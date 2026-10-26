#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <iostream>
#include <memory>

class Fuzzion5000RenderApplication final : public juce::JUCEApplication,
                                           private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Fuzzion5000Screenshot"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override {}
    void shutdown() override {}

    void initialise (const juce::String& commandLine) override
    {
        outputFile = juce::File::getCurrentWorkingDirectory().getChildFile ("dist/fuzzion5000-ui.png");

        for (const auto& argument : juce::StringArray::fromTokens (commandLine, true))
        {
            if (argument.startsWith ("--out="))
            {
                const auto path = argument.substring (6);
                outputFile = juce::File::isAbsolutePath (path)
                                 ? juce::File (path)
                                 : juce::File::getCurrentWorkingDirectory().getChildFile (path);
            }
        }

        outputFile = outputFile.getFullPathName();
        outputFile.getParentDirectory().createDirectory();

        processor = std::make_unique<Fuzzion5000AudioProcessor>();
        processor->setPlayConfigDetails (2, 2, 48000.0, 1024);
        processor->prepareToPlay (48000.0, 1024);
        processor->setCurrentProgram (4); // Velcro Riot: starve bias, gate and iron all active.

        // Park the pointer away from the unit before it appears, so the captured
        // snapshot never carries a stale hover highlight.
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 5.0f, 5.0f });

        editor = std::unique_ptr<juce::AudioProcessorEditor> (processor->createEditor());

        if (editor == nullptr)
        {
            std::cerr << "Could not create the plugin editor.\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        editor->setSize (1180, 720);
        editor->addToDesktop (0);
        editor->setTopLeftPosition (30, 30);
        editor->setVisible (true);
        editor->toFront (true);

        testBuffer.setSize (2, 1024);
        startTimerHz (30);
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 2.0f, 2.0f });

        juce::Timer::callAfterDelay (1800, [this] { capture(); });
    }

private:
    void timerCallback() override
    {
        if (processor == nullptr)
            return;

        // Keep the real cursor away from the unit for the whole capture run, so
        // the snapshot never includes a live hover state or tooltip.
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 2.0f, 2.0f });

        for (int i = 0; i < testBuffer.getNumSamples(); ++i)
        {
            const auto n = sampleCounter + i;
            const double t = static_cast<double> (n) / 48000.0;

            // A repeating, decaying guitar-ish pluck: a sawtooth edge plus a little
            // pick noise, so the fuzz trace shows a dense, asymmetric waveform.
            const double period = 0.42;
            const double phase = std::fmod (t, period) / period;
            const float envelope = static_cast<float> (std::exp (-4.2 * phase) * (1.0 - std::exp (-70.0 * phase)));

            const double fundamental = juce::MathConstants<double>::twoPi * 110.0 * t;
            const double second = 2.0 * fundamental;
            const double third = 3.0 * fundamental;

            const float body = static_cast<float> (
                0.36 * std::sin (fundamental) + 0.20 * std::sin (second)
                + 0.12 * std::sin (third) + 0.05 * std::sin (5.0 * fundamental));

            const float pick = static_cast<float> (0.010 * std::sin (t * 12731.0)
                                                    + 0.008 * std::sin (t * 8191.0));

            const float sample = juce::jlimit (-1.0f, 1.0f, envelope * body + pick * (1.0f - envelope));

            testBuffer.setSample (0, i, sample);
            testBuffer.setSample (1, i, sample * 0.94f);
        }

        sampleCounter += testBuffer.getNumSamples();
        juce::MidiBuffer midi;
        processor->processBlock (testBuffer, midi);
    }

    void capture()
    {
        if (editor == nullptr)
            return;

        stopTimer();
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 2.0f, 2.0f });
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

        if (image.isValid())
        {
            outputFile.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (outputFile.createOutputStream()))
            {
                juce::PNGImageFormat png;
                const bool written = png.writeImageToStream (image, *stream);
                stream->flush();

                if (written)
                    std::cout << "Rendered " << outputFile.getFullPathName() << '\n';
                else
                    std::cerr << "Failed to encode PNG.\n";
            }
            else
            {
                std::cerr << "Could not open " << outputFile.getFullPathName() << '\n';
            }
        }
        else
        {
            std::cerr << "Editor snapshot was invalid.\n";
        }

        processor->releaseResources();
        editor->removeFromDesktop();
        editor.reset();
        processor.reset();
        quit();
    }

    std::unique_ptr<Fuzzion5000AudioProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outputFile;
    juce::AudioBuffer<float> testBuffer;
    juce::int64 sampleCounter = 0;
};

START_JUCE_APPLICATION (Fuzzion5000RenderApplication)
