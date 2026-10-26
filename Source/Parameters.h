#pragma once

#include <array>

namespace fuzzion5000::params
{
    // Stable parameter identifiers. Never rename a value: hosts persist these strings.
    inline constexpr const char* input       = "input";
    inline constexpr const char* fuzz        = "fuzz";
    inline constexpr const char* bias        = "bias";
    inline constexpr const char* tone        = "tone";
    inline constexpr const char* gate        = "gate";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* toneStack   = "toneStack";
    inline constexpr const char* gateEnabled = "gateEnabled";
    inline constexpr const char* iron        = "iron";
    inline constexpr const char* bypass      = "bypass";

    inline constexpr int numParameters = 13;

    inline const std::array<const char*, numParameters>& allParameterIDs()
    {
        static const std::array<const char*, numParameters> ids
        {
            input, fuzz, bias, tone, gate, octave, mix, output,
            mode, toneStack, gateEnabled, iron, bypass
        };

        return ids;
    }

    // --- Coarse defaults mirrored by the Init preset and the DSP reset path. ---
    inline constexpr float defaultInput       = 0.0f;   // dB
    inline constexpr float defaultFuzz        = 62.0f;  // %
    inline constexpr float defaultBias        = 0.0f;   // -100..+100
    inline constexpr float defaultTone        = 52.0f;  // %
    inline constexpr float defaultGate        = 0.0f;   // %
    inline constexpr float defaultOctave      = 0.0f;   // %
    inline constexpr float defaultMix         = 100.0f; // %
    inline constexpr float defaultOutput      = -3.0f;  // dB
    inline constexpr int   defaultMode        = 0;
    inline constexpr bool  defaultToneStack   = true;
    inline constexpr bool  defaultGateEnabled = false;
    inline constexpr bool  defaultIron        = true;
    inline constexpr bool  defaultBypass      = false;
} // namespace fuzzion5000::params
