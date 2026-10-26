#include "PluginEditor.h"

#include "Parameters.h"
#include "Presets.h"
#include "ui/Controls.h"
#include "ui/Theme.h"
#include "ui/TransferDisplay.h"
#include "ui/WaveformDisplay.h"

#include <array>
#include <cmath>
#include <utility>

using namespace studio::ui;

namespace
{
constexpr int designWidth = 1180;
constexpr int designHeight = 720;

juce::String formatDb (float value)
{
    if (std::abs (value) < 0.05f)
        return "0.0 dB";

    return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
}

juce::String formatPercent (float value)
{
    return juce::String (juce::roundToInt (value)) + " %";
}

juce::String formatSignedPercent (float value)
{
    const auto rounded = juce::roundToInt (value);

    if (rounded == 0)
        return "CENTER";

    return (rounded > 0 ? "+" : "") + juce::String (rounded) + " %";
}
} // namespace

class Fuzzion5000EditorContent final : public juce::Component,
                                       private juce::Timer
{
public:
    explicit Fuzzion5000EditorContent (Fuzzion5000AudioProcessor& p)
        : processor (p),
          apvts (p.getValueTreeState())
    {
        setOpaque (true);

        fuzzKnob = std::make_unique<ParamKnob> (apvts, fuzzion5000::params::fuzz,
                                                "FUZZ DRIVE", formatPercent);
        biasKnob = std::make_unique<ParamKnob> (apvts, fuzzion5000::params::bias,
                                                "BIAS STARVE", formatSignedPercent);
        toneKnob = std::make_unique<ParamKnob> (apvts, fuzzion5000::params::tone,
                                                "TONE", formatPercent);
        gateKnob = std::make_unique<ParamKnob> (apvts, fuzzion5000::params::gate,
                                                "GATE", formatPercent);
        octaveKnob = std::make_unique<ParamKnob> (apvts, fuzzion5000::params::octave,
                                                  "OCTAVE UP", formatPercent);

        for (auto* knob : { fuzzKnob.get(), biasKnob.get(), toneKnob.get(),
                            gateKnob.get(), octaveKnob.get() })
        {
            addAndMakeVisible (*knob);
        }

        inputSlider = std::make_unique<GlassSlider> (apvts, fuzzion5000::params::input,
                                                     "INPUT", formatDb);
        mixSlider = std::make_unique<GlassSlider> (apvts, fuzzion5000::params::mix,
                                                   "PARALLEL MIX", formatPercent);
        outputSlider = std::make_unique<GlassSlider> (apvts, fuzzion5000::params::output,
                                                      "OUTPUT", formatDb);

        for (auto* slider : { inputSlider.get(), mixSlider.get(), outputSlider.get() })
            addAndMakeVisible (*slider);

        ironButton = std::make_unique<TactileButton> (apvts, fuzzion5000::params::iron, "IRON");
        gateButton = std::make_unique<TactileButton> (apvts, fuzzion5000::params::gateEnabled, "GATE");
        toneButton = std::make_unique<TactileButton> (apvts, fuzzion5000::params::toneStack, "TONE");
        bypassButton = std::make_unique<TactileButton> (apvts, fuzzion5000::params::bypass, "BYPASS");

        addAndMakeVisible (*ironButton);
        addAndMakeVisible (*gateButton);
        addAndMakeVisible (*toneButton);
        addAndMakeVisible (*bypassButton);

        siliconButton = std::make_unique<ModeButton> (apvts, fuzzion5000::params::mode, 0, "SILICON");
        germaniumButton = std::make_unique<ModeButton> (apvts, fuzzion5000::params::mode, 1, "GERMANIUM");
        velcroButton = std::make_unique<ModeButton> (apvts, fuzzion5000::params::mode, 2, "VELCRO");
        doomButton = std::make_unique<ModeButton> (apvts, fuzzion5000::params::mode, 3, "DOOM");

        addAndMakeVisible (*siliconButton);
        addAndMakeVisible (*germaniumButton);
        addAndMakeVisible (*velcroButton);
        addAndMakeVisible (*doomButton);

        waveformDisplay = std::make_unique<WaveformDisplay> (processor);
        addAndMakeVisible (*waveformDisplay);

        transferDisplay = std::make_unique<TransferDisplay> (processor);
        addAndMakeVisible (*transferDisplay);

        inputMeter = std::make_unique<InputMeter> (processor);
        addAndMakeVisible (*inputMeter);

        outputMeter = std::make_unique<OutputMeter> (processor);
        addAndMakeVisible (*outputMeter);

        pillBar = std::make_unique<HeaderPillBar> (processor);
        pillBar->onPresetChanged = [this] { repaint(); };
        addAndMakeVisible (*pillBar);

        lastBypassed = isBypassed();
        lastPreset = processor.getCurrentPresetIndex();
        lastDirty = processor.isPresetDirty();
        startTimerHz (12);
    }

    ~Fuzzion5000EditorContent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        drawDesktopHardwareChassis (g, bounds);

        paintBrandHeader (g);

        g.setColour (obsidianGrid.withAlpha (0.90f));
        g.fillRect (52.0f, 97.0f, 1076.0f, 1.0f);

        drawEngravedLabel (g, { 52.0f, 102.0f, 244.0f, 26.0f },
                           "FUZZ CORE", "TRANSISTOR BIAS / STARVE NETWORK");
        drawEngravedLabel (g, { 324.0f, 306.0f, 544.0f, 26.0f },
                           "CLIPPER CHARACTER", "BIASED ASYMMETRIC STAGES / OCTAVE RECTIFIER");
        drawEngravedLabel (g, { 884.0f, 102.0f, 244.0f, 26.0f },
                           "OUTPUT STAGE", "PARALLEL MIX / IRON COUPLING");

        paintAnnotations (g);
        paintFooter (g);
    }

    void resized() override
    {
        pillBar->setBounds (390, 44, 400, 36);

        ironButton->setBounds (802, 48, 74, 30);
        gateButton->setBounds (884, 48, 74, 30);
        toneButton->setBounds (966, 48, 74, 30);
        bypassButton->setBounds (1048, 48, 80, 30);

        waveformDisplay->setBounds (312, 110, 556, 190);

        fuzzKnob->setBounds (54, 128, 240, 196);
        biasKnob->setBounds (72, 338, 136, 152);
        inputMeter->setBounds (60, 502, 228, 26);

        toneKnob->setBounds (324, 346, 176, 118);
        gateKnob->setBounds (500, 346, 176, 118);
        octaveKnob->setBounds (676, 346, 176, 118);

        transferDisplay->setBounds (324, 474, 528, 70);

        siliconButton->setBounds (324, 558, 126, 28);
        germaniumButton->setBounds (458, 558, 126, 28);
        velcroButton->setBounds (592, 558, 126, 28);
        doomButton->setBounds (726, 558, 126, 28);

        inputSlider->setBounds (913, 130, 58, 404);
        mixSlider->setBounds (975, 130, 58, 404);
        outputSlider->setBounds (1037, 130, 58, 404);

        outputMeter->setBounds (884, 544, 244, 34);
    }

private:
    void timerCallback() override
    {
        const bool bypassed = isBypassed();
        const int preset = processor.getCurrentPresetIndex();
        const bool dirty = processor.isPresetDirty();

        if (bypassed != lastBypassed || preset != lastPreset || dirty != lastDirty)
        {
            lastBypassed = bypassed;
            lastPreset = preset;
            lastDirty = dirty;
            repaint();
        }
    }

    bool isBypassed() const noexcept
    {
        if (auto* value = apvts.getRawParameterValue (fuzzion5000::params::bypass))
            return value->load() > 0.5f;

        return false;
    }

    void paintBrandHeader (juce::Graphics& g)
    {
        g.setColour (textSecondary.withAlpha (0.85f));
        g.setFont (font (6.3f, Weight::semibold, 0.22f));
        g.drawText ("BLS AUDIO  /  FUZZ LAB", 52, 34, 280, 9,
                    juce::Justification::centredLeft, false);

        g.setColour (textPrimary);
        g.setFont (font (21.0f, Weight::semibold, 0.04f));
        g.drawText ("FUZZION-5000", 52, 43, 320, 27,
                    juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.6f, Weight::regular, 0.14f));
        g.drawText ("TRANSISTOR FUZZ DISTORTION  /  STARVE BIAS CORE",
                    52, 71, 330, 9, juce::Justification::centredLeft, false);

        g.setColour (crimsonAccent);
        g.fillRect (52.0f, 84.0f, 54.0f, 1.4f);
    }

    void paintAnnotations (juce::Graphics& g)
    {
        g.setFont (font (6.0f, Weight::regular, 0.10f));
        g.setColour (textMuted.brighter (0.22f));

        g.drawText ("NPN / PNP ASYMMETRIC CLIPPER",
                    54, 536, 236, 9, juce::Justification::centredLeft, false);
        g.drawText ("STARVE BIAS / DC-COUPLED STAGES",
                    54, 549, 236, 9, juce::Justification::centredLeft, false);
        g.drawText ("4x OVERSAMPLED / NO ALIASING",
                    54, 562, 236, 9, juce::Justification::centredLeft, false);

        g.setColour (textSecondary.withAlpha (0.85f));
        g.setFont (font (6.0f, Weight::semibold, 0.14f));
        g.drawText ("CHARACTER", 324, 544, 120, 9, juce::Justification::centredLeft, false);

        g.setColour (textMuted.brighter (0.22f));
        g.setFont (font (6.0f, Weight::regular, 0.10f));
        g.drawText ("TRANSFORMER-COUPLED / DC BLOCKED",
                    884, 584, 244, 9, juce::Justification::centredLeft, false);
        g.drawText ("4x OVERSAMPLED / CONSTANT-LATENCY BYPASS",
                    884, 595, 244, 9, juce::Justification::centredLeft, false);
    }

    void paintFooter (juce::Graphics& g)
    {
        g.setColour (obsidianGrid);
        g.fillRect (52.0f, 608.0f, 1076.0f, 1.0f);

        g.setColour (textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("PRESET CHARACTER", 52, 616, 240, 9,
                    juce::Justification::centredLeft, false);

        const int preset = processor.getCurrentPresetIndex();
        const bool dirty = processor.isPresetDirty();

        g.setColour (textSecondary);
        g.setFont (font (7.4f, Weight::regular, 0.08f));
        g.drawFittedText (fuzzion5000::getFactoryPresetCharacter (preset),
                          juce::Rectangle<int> (52, 628, 268, 22),
                          juce::Justification::centredLeft, 2);

        drawCrimsonDot (g, { 55.0f, 660.0f }, 2.0f);

        g.setColour (dirty ? crimsonBright : textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.14f));
        g.drawText (dirty ? "EDITED / UNSAVED" : "FACTORY / CLEAN",
                    64, 655, 240, 9, juce::Justification::centredLeft, false);

        // Signal-path chips.
        const std::array<const char*, 4> stages { "BIAS CLIP", "OCTAVE", "TONE STACK", "IRON OUT" };
        float chipX = 338.0f;
        constexpr float chipW = 82.0f;
        constexpr float chipGap = 12.0f;

        for (size_t i = 0; i < stages.size(); ++i)
        {
            const auto chip = juce::Rectangle<float> (chipX, 620.0f, chipW, 18.0f);
            drawSignalChip (g, chip, stages[i]);

            if (i + 1 < stages.size())
            {
                g.setColour (crimsonAccent.withAlpha (0.80f));
                g.setFont (font (7.5f, Weight::semibold, 0.0f));
                g.drawText (">", juce::Rectangle<float> (chip.getRight(), chip.getY(),
                                                          chipGap, chip.getHeight()),
                            juce::Justification::centred, false);
            }

            chipX += chipW + chipGap;
        }

        g.setColour (textMuted);
        g.setFont (font (5.8f, Weight::semibold, 0.12f));
        g.drawText ("STARVE BIAS  //  FULL-WAVE OCTAVE  //  4x OVERSAMPLED",
                    338, 648, 420, 9, juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("BLS AUDIO  /  VST3  /  64-BIT FLOAT",
                    792, 658, 336, 9, juce::Justification::centredRight, false);
    }

    Fuzzion5000AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::unique_ptr<ParamKnob> fuzzKnob;
    std::unique_ptr<ParamKnob> biasKnob;
    std::unique_ptr<ParamKnob> toneKnob;
    std::unique_ptr<ParamKnob> gateKnob;
    std::unique_ptr<ParamKnob> octaveKnob;

    std::unique_ptr<GlassSlider> inputSlider;
    std::unique_ptr<GlassSlider> mixSlider;
    std::unique_ptr<GlassSlider> outputSlider;

    std::unique_ptr<TactileButton> ironButton;
    std::unique_ptr<TactileButton> gateButton;
    std::unique_ptr<TactileButton> toneButton;
    std::unique_ptr<TactileButton> bypassButton;

    std::unique_ptr<ModeButton> siliconButton;
    std::unique_ptr<ModeButton> germaniumButton;
    std::unique_ptr<ModeButton> velcroButton;
    std::unique_ptr<ModeButton> doomButton;

    std::unique_ptr<WaveformDisplay> waveformDisplay;
    std::unique_ptr<TransferDisplay> transferDisplay;
    std::unique_ptr<InputMeter> inputMeter;
    std::unique_ptr<OutputMeter> outputMeter;
    std::unique_ptr<HeaderPillBar> pillBar;

    bool lastBypassed = false;
    int lastPreset = 0;
    bool lastDirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fuzzion5000EditorContent)
};

//======================================================================================
Fuzzion5000AudioProcessorEditor::Fuzzion5000AudioProcessorEditor (Fuzzion5000AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    content = std::make_unique<Fuzzion5000EditorContent> (processor);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (885, 540, 1770, 1080);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (static_cast<double> (designWidth)
                                          / static_cast<double> (designHeight));

    setSize (designWidth, designHeight);
}

Fuzzion5000AudioProcessorEditor::~Fuzzion5000AudioProcessorEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void Fuzzion5000AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (studioSurfaceDark);
}

void Fuzzion5000AudioProcessorEditor::resized()
{
    if (content == nullptr)
        return;

    const float scale = juce::jmin (static_cast<float> (getWidth()) / static_cast<float> (designWidth),
                                    static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const float offsetX = (static_cast<float> (getWidth()) - static_cast<float> (designWidth) * scale) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - static_cast<float> (designHeight) * scale) * 0.5f;

    content->setBounds (0, 0, designWidth, designHeight);
    content->setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
}
