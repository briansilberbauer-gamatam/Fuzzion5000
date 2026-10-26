#include "TransferDisplay.h"

#include "Parameters.h"
#include "Theme.h"
#include "dsp/FuzzCore.h"

#include <cmath>

namespace studio::ui
{
namespace
{
const char* modeLabel (int modeIndex)
{
    switch (modeIndex)
    {
        case 1:  return "GERMANIUM";
        case 2:  return "VELCRO";
        case 3:  return "DOOM";
        default: return "SILICON";
    }
}

float evaluateTransfer (float input, float fuzz, float bias, int modeIndex)
{
    return juce::jlimit (-1.15f, 1.15f,
                         fuzzion::dsp::transferSample (input, fuzz, bias, modeIndex));
}
} // namespace

TransferDisplay::TransferDisplay (Fuzzion5000AudioProcessor& p)
    : processor (p),
      fuzzParameter (p.getValueTreeState().getRawParameterValue (fuzzion5000::params::fuzz)),
      biasParameter (p.getValueTreeState().getRawParameterValue (fuzzion5000::params::bias)),
      modeParameter (p.getValueTreeState().getRawParameterValue (fuzzion5000::params::mode))
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void TransferDisplay::timerCallback()
{
    const float targetFuzz = fuzzParameter != nullptr ? fuzzParameter->load() * 0.01f : 0.5f;
    const float targetBias = biasParameter != nullptr ? biasParameter->load() * 0.01f : 0.0f;
    const float targetProbe = juce::jlimit (0.0f, 1.0f, processor.getInputLevel() * 1.35f);

    fuzzValue += (targetFuzz - fuzzValue) * 0.35f;
    biasValue += (targetBias - biasValue) * 0.35f;
    inputProbe += (targetProbe - inputProbe) * (targetProbe > inputProbe ? 0.55f : 0.18f);
    mode = modeParameter != nullptr ? juce::roundToInt (modeParameter->load()) : 0;

    repaint();
}

void TransferDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    constexpr float corner = 4.0f;

    drawGlassWell (g, bounds, corner);

    const auto graph = bounds.reduced (2.0f)
                            .withTrimmedTop (13.0f)
                            .withTrimmedBottom (10.0f)
                            .withTrimmedLeft (28.0f)
                            .withTrimmedRight (82.0f);

    if (graph.getWidth() < 40.0f || graph.getHeight() < 20.0f)
        return;

    // Faint measurement grid with a centre crosshair.
    g.setColour (obsidianGrid);
    for (int i = 1; i < 4; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * static_cast<float> (i) / 4.0f;
        g.fillRect (x, graph.getY(), 1.0f, graph.getHeight());
    }

    for (int i = 1; i < 3; ++i)
    {
        const float y = graph.getY() + graph.getHeight() * static_cast<float> (i) / 3.0f;
        g.fillRect (graph.getX(), y, graph.getWidth(), 1.0f);
    }

    g.setColour (obsidianGrid.brighter (0.20f));
    g.fillRect (graph.getX(), graph.getCentreY(), graph.getWidth(), 1.0f);
    g.fillRect (graph.getCentreX(), graph.getY(), 1.0f, graph.getHeight());

    // Unity reference diagonal.
    g.setColour (textMuted.withAlpha (0.30f));
    g.drawLine (graph.getX(), graph.getBottom(), graph.getRight(), graph.getY(), 0.8f);

    const float midY = graph.getCentreY();
    const float halfH = graph.getHeight() * 0.44f;
    constexpr int points = 112;

    juce::Path curve;

    auto sampleToScreen = [&] (float input, float output)
    {
        return juce::Point<float> (
            graph.getX() + (input * 0.5f + 0.5f) * graph.getWidth(),
            midY - output * halfH);
    };

    for (int i = 0; i <= points; ++i)
    {
        const float input = -1.0f + 2.0f * static_cast<float> (i) / static_cast<float> (points);
        const auto point = sampleToScreen (input, evaluateTransfer (input, fuzzValue, biasValue, mode));

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    // Wide crimson bloom, muted body, bright core filament.
    g.setColour (crimsonGlow.withAlpha (0.30f));
    g.strokePath (curve, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour (crimsonAccent);
    g.strokePath (curve, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour (crimsonBright.withAlpha (0.85f));
    g.strokePath (curve, juce::PathStrokeType (0.7f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Live probe at the current input envelope.
    const float probe = juce::jlimit (0.0f, 1.0f, inputProbe);
    const auto probePoint = sampleToScreen (probe, evaluateTransfer (probe, fuzzValue, biasValue, mode));

    g.setColour (crimsonAccent.withAlpha (0.22f));
    g.drawLine (probePoint.x, graph.getY(), probePoint.x, graph.getBottom(), 0.7f);
    g.setColour (crimsonGlow.withAlpha (0.30f));
    g.fillEllipse (probePoint.x - 5.0f, probePoint.y - 5.0f, 10.0f, 10.0f);
    g.setColour (crimsonBright);
    g.fillEllipse (probePoint.x - 1.9f, probePoint.y - 1.9f, 3.8f, 3.8f);
    g.setColour (textPrimary.withAlpha (0.85f));
    g.fillEllipse (probePoint.x - 0.8f, probePoint.y - 0.8f, 1.6f, 1.6f);

    // Captions.
    g.setColour (textSecondary.withAlpha (0.85f));
    g.setFont (font (5.9f, Weight::semibold, 0.16f));
    g.drawText ("TRANSFER SHAPE / BIAS SCOPE", bounds.getX() + 7.0f, bounds.getY() + 3.0f,
                bounds.getWidth() - 96.0f, 8.0f, juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (font (5.4f, Weight::semibold, 0.10f));
    g.drawText ("OUT", bounds.getX() + 5.0f, bounds.getCentreY() - 5.0f, 20.0f, 9.0f,
                juce::Justification::centredLeft, false);
    g.drawText ("IN", graph.getX(), bounds.getBottom() - 9.0f, 40.0f, 8.0f,
                juce::Justification::centredLeft, false);

    const auto readoutArea = juce::Rectangle<float> (bounds.getRight() - 78.0f, bounds.getY(),
                                                     72.0f, bounds.getHeight());

    g.setColour (crimsonAccent);
    g.setFont (font (5.9f, Weight::semibold, 0.12f));
    g.drawText (modeLabel (mode), readoutArea.withTrimmedTop (16.0f).withHeight (8.0f),
                juce::Justification::centredRight, false);

    const auto biasText = (biasValue >= 0.0f ? "+" : "")
                          + juce::String (juce::roundToInt (biasValue * 100.0f)) + " %";
    g.setColour (std::abs (biasValue) > 0.01f ? textPrimary : textMuted);
    g.setFont (font (7.2f, Weight::semibold, 0.03f));
    g.drawText ("BIAS " + biasText, readoutArea.withTrimmedTop (27.0f).withHeight (11.0f),
                juce::Justification::centredRight, false);

    g.setColour (textMuted);
    g.setFont (font (5.2f, Weight::regular, 0.08f));
    g.drawText ("STARVE " + juce::String (juce::roundToInt (std::abs (biasValue) * 100.0f)) + "%",
                readoutArea.withTrimmedTop (39.0f).withHeight (8.0f),
                juce::Justification::centredRight, false);

    g.setColour (crimsonAccent.withAlpha (0.30f));
    g.drawRoundedRectangle (bounds.reduced (0.7f), corner, 0.7f);
}
} // namespace studio::ui
