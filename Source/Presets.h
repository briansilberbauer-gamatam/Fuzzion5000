#pragma once

#include "Parameters.h"

#include <vector>

namespace fuzzion5000
{
    struct PresetValue
    {
        const char* id;
        float value;
    };

    struct FactoryPreset
    {
        const char* name;
        const char* character;
        std::vector<PresetValue> overrides;
    };

    inline const std::vector<FactoryPreset>& getFactoryPresets()
    {
        using namespace params;

        static const std::vector<FactoryPreset> presets
        {
            {
                "Init Fuzz", "CLEAN START / SILICON CORE",
                {
                    { input, 0.0f }, { fuzz, 55.0f }, { bias, 0.0f }, { tone, 52.0f },
                    { gate, 0.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -3.0f },
                    { mode, 0.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f },
                    { bypass, 0.0f }
                }
            },
            {
                "Voodoo Silicon", "CLASSIC FUZZ FACE / WARM BIAS",
                {
                    { input, 1.0f }, { fuzz, 72.0f }, { bias, -12.0f }, { tone, 58.0f },
                    { gate, 0.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -5.0f },
                    { mode, 0.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            },
            {
                "Germanium Rain", "NOS TRANSISTOR / COMPRESSED",
                {
                    { input, 0.0f }, { fuzz, 66.0f }, { bias, 14.0f }, { tone, 42.0f },
                    { gate, 0.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -4.0f },
                    { mode, 1.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            },
            {
                "Octave Frenzy", "FULL-WAVE OCTAVE / LEAD",
                {
                    { input, 2.0f }, { fuzz, 78.0f }, { bias, -6.0f }, { tone, 66.0f },
                    { gate, 0.0f }, { octave, 68.0f }, { mix, 100.0f }, { output, -6.0f },
                    { mode, 0.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            },
            {
                "Velcro Riot", "STARVED SPUTTER / GATED",
                {
                    { input, 3.0f }, { fuzz, 84.0f }, { bias, 46.0f }, { tone, 70.0f },
                    { gate, 55.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -7.0f },
                    { mode, 2.0f }, { toneStack, 1.0f }, { gateEnabled, 1.0f }, { iron, 1.0f }
                }
            },
            {
                "Doom Slug", "SCOOPED MIDS / SLOW SLUDGE",
                {
                    { input, 2.0f }, { fuzz, 92.0f }, { bias, 18.0f }, { tone, 38.0f },
                    { gate, 0.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -8.0f },
                    { mode, 3.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            },
            {
                "Bass Starve", "LOW-END FUZZ / TIGHT GATE",
                {
                    { input, 0.0f }, { fuzz, 74.0f }, { bias, 30.0f }, { tone, 30.0f },
                    { gate, 55.0f }, { octave, 0.0f }, { mix, 100.0f }, { output, -6.0f },
                    { mode, 1.0f }, { toneStack, 1.0f }, { gateEnabled, 1.0f }, { iron, 1.0f }
                }
            },
            {
                "Sonic Cathedral", "SUSTAIN WALL / IRON SATURATED",
                {
                    { input, 3.0f }, { fuzz, 88.0f }, { bias, -20.0f }, { tone, 60.0f },
                    { gate, 0.0f }, { octave, 24.0f }, { mix, 100.0f }, { output, -8.0f },
                    { mode, 3.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            },
            {
                "Broken Radio", "LO-FI CHOP / NOISE GATE",
                {
                    { input, 4.0f }, { fuzz, 96.0f }, { bias, 62.0f }, { tone, 78.0f },
                    { gate, 62.0f }, { octave, 12.0f }, { mix, 100.0f }, { output, -9.0f },
                    { mode, 2.0f }, { toneStack, 1.0f }, { gateEnabled, 1.0f }, { iron, 0.0f }
                }
            },
            {
                "Parallel Grit", "DRY BLEND / MIX 45",
                {
                    { input, 1.0f }, { fuzz, 80.0f }, { bias, -8.0f }, { tone, 55.0f },
                    { gate, 0.0f }, { octave, 0.0f }, { mix, 45.0f }, { output, -3.0f },
                    { mode, 0.0f }, { toneStack, 1.0f }, { gateEnabled, 0.0f }, { iron, 1.0f }
                }
            }
        };

        return presets;
    }

    inline int getNumFactoryPresets()
    {
        return static_cast<int> (getFactoryPresets().size());
    }

    inline juce::String getFactoryPresetName (int index)
    {
        const auto& presets = getFactoryPresets();

        if (index < 0 || index >= static_cast<int> (presets.size()))
            return {};

        return presets[static_cast<size_t> (index)].name;
    }

    inline juce::String getFactoryPresetCharacter (int index)
    {
        const auto& presets = getFactoryPresets();

        if (index < 0 || index >= static_cast<int> (presets.size()))
            return {};

        return presets[static_cast<size_t> (index)].character;
    }
} // namespace fuzzion5000
