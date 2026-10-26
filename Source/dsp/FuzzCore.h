#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace fuzzion::dsp
{
    //==================================================================================
    // Transistor characters. Each mode changes the clipper knees/hardness, the
    // interstage coupling filters and the fixed mid contour of the tone stack.
    //==================================================================================
    enum class Mode
    {
        silicon = 0,
        germanium = 1,
        velcro = 2,
        doom = 3
    };

    inline constexpr int numModes = 4;

    struct ModeProfile
    {
        float kneePositive;   // Clipping ceiling above the bias point
        float kneeNegative;   // Clipping ceiling below the bias point
        float hardness;       // tanh slope: 1.5 soft germanium, 7 hard velcro
        float stage2Gain;     // Second-stage recovery gain
        float preHpHz;        // Pre-clip low cut: tightens the fuzz
        float interHpHz;      // Interstage coupling capacitor
        float postLpHz;       // Character low-pass ceiling
        float midHz;          // Fixed mid contour centre
        float midGainDb;      // Fixed mid contour gain
    };

    inline ModeProfile modeProfile (int modeIndex) noexcept
    {
        switch (static_cast<Mode> (juce::jlimit (0, numModes - 1, modeIndex)))
        {
            case Mode::germanium:
                return { 0.86f, 0.42f, 1.7f, 2.05f, 95.0f, 55.0f, 7000.0f, 720.0f, 4.5f };

            case Mode::velcro:
                return { 0.55f, 0.36f, 6.8f, 3.30f, 220.0f, 140.0f, 5200.0f, 1250.0f, -3.0f };

            case Mode::doom:
                return { 1.25f, 0.72f, 2.1f, 3.10f, 65.0f, 45.0f, 6000.0f, 480.0f, -8.0f };

            case Mode::silicon:
            default:
                return { 1.00f, 0.55f, 2.6f, 2.45f, 120.0f, 70.0f, 9000.0f, 900.0f, 2.5f };
        }
    }

    // Fuzz knob (0..1) mapped to linear pre-gain. 1.2x is a barely-breaking-up
    // transistor, 31x is a wall of saturation.
    inline float fuzzDriveGain (float fuzzNormalised) noexcept
    {
        return 1.2f * std::pow (26.0f, juce::jlimit (0.0f, 1.0f, fuzzNormalised));
    }

    // Asymmetric soft clipper: independent ceilings above and below the operating
    // point, with a mode-dependent tanh hardness. Continuous and smooth at zero.
    inline float asymmetricClip (float x, float kneePositive, float kneeNegative,
                                 float hardness) noexcept
    {
        const float positive = juce::jmax (0.05f, kneePositive);
        const float negative = juce::jmax (0.05f, kneeNegative);
        const float knee = x >= 0.0f ? positive : negative;
        const float magnitude = std::abs (x);
        const float shaped = knee * std::tanh (magnitude * hardness / knee);

        return x >= 0.0f ? shaped : -shaped;
    }

    // Static two-stage transfer function used by the on-screen transfer scope.
    // Filters are intentionally omitted: the scope shows the clipper shape only.
    inline float transferSample (float x, float fuzzNormalised, float biasNormalised,
                                 int modeIndex) noexcept
    {
        const auto profile = modeProfile (modeIndex);
        const float drive = fuzzDriveGain (fuzzNormalised);
        const float starve = std::abs (biasNormalised);
        const float preStage = x * drive + biasNormalised * 0.85f;

        float stage = asymmetricClip (preStage, profile.kneePositive, profile.kneeNegative,
                                       profile.hardness);
        stage *= profile.stage2Gain * (1.0f - 0.45f * starve);
        stage = asymmetricClip (stage, profile.kneePositive * 0.92f, profile.kneeNegative * 0.92f,
                                profile.hardness * 1.12f);

        return stage * 0.82f;
    }

    //==================================================================================
    // TPT one-pole low/high-pass. State is preserved across coefficient changes.
    //==================================================================================
    class OnePole
    {
    public:
        enum class Type { lowPass, highPass };

        void prepare (double sampleRateToUse, Type typeToUse, float cutoffHz)
        {
            sampleRate = juce::jmax (8000.0, sampleRateToUse);
            type = typeToUse;
            reset();
            cutoff = -1.0f;
            setCutoff (cutoffHz);
        }

        void reset() noexcept { state = 0.0f; }

        void setCutoff (float cutoffHz) noexcept
        {
            const float nyquistLimit = static_cast<float> (sampleRate) * 0.45f;
            const float clamped = juce::jlimit (4.0f, nyquistLimit, cutoffHz);

            if (std::abs (clamped - cutoff) < 0.01f)
                return;

            cutoff = clamped;
            const float g = std::tan (juce::MathConstants<float>::pi * cutoff
                                      / static_cast<float> (sampleRate));
            gCoefficient = g / (1.0f + g);
        }

        float process (float x) noexcept
        {
            const float v = (x - state) * gCoefficient;
            const float lowPass = v + state;
            state = lowPass + v;

            return type == Type::lowPass ? lowPass : x - lowPass;
        }

    private:
        double sampleRate = 48000.0;
        Type type = Type::lowPass;
        float cutoff = -1.0f;
        float gCoefficient = 0.0f;
        float state = 0.0f;
    };

    //==================================================================================
    // RBJ peaking equaliser for the fixed per-mode mid contour.
    //==================================================================================
    class PeakingFilter
    {
    public:
        void prepare (double sampleRateToUse) noexcept
        {
            sampleRate = juce::jmax (8000.0, sampleRateToUse);
            reset();
            setParameters (1000.0f, 1.0f, 0.0f);
        }

        void reset() noexcept { z1 = z2 = 0.0f; }

        void setParameters (float frequency, float q, float gainDb) noexcept
        {
            const double nyquistLimit = sampleRate * 0.45;
            const double f = juce::jlimit (20.0, nyquistLimit, static_cast<double> (frequency));
            const double resonance = juce::jmax (0.2, static_cast<double> (q));
            const double a = std::pow (10.0, static_cast<double> (gainDb) / 40.0);
            const double w0 = juce::MathConstants<double>::twoPi * f / sampleRate;
            const double cosW0 = std::cos (w0);
            const double alpha = std::sin (w0) / (2.0 * resonance);

            const double a0 = 1.0 + alpha / a;
            b0 = static_cast<float> ((1.0 + alpha * a) / a0);
            b1 = static_cast<float> ((-2.0 * cosW0) / a0);
            b2 = static_cast<float> ((1.0 - alpha * a) / a0);
            a1 = static_cast<float> ((-2.0 * cosW0) / a0);
            a2 = static_cast<float> ((1.0 - alpha / a) / a0);
        }

        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

    private:
        double sampleRate = 48000.0;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;
    };

    //==================================================================================
    // Exponential envelope follower used by the noise gate and the saturation meter.
    //==================================================================================
    class EnvelopeFollower
    {
    public:
        void prepare (double sampleRateToUse, float attackMs, float releaseMs) noexcept
        {
            sampleRate = juce::jmax (8000.0, sampleRateToUse);
            setTimes (attackMs, releaseMs);
            reset();
        }

        void setTimes (float attackMs, float releaseMs) noexcept
        {
            const double sr = sampleRate;
            attackCoefficient = static_cast<float> (
                std::exp (-1.0 / (juce::jmax (0.05, static_cast<double> (attackMs)) * 0.001 * sr)));
            releaseCoefficient = static_cast<float> (
                std::exp (-1.0 / (juce::jmax (0.05, static_cast<double> (releaseMs)) * 0.001 * sr)));
        }

        void reset() noexcept { envelope = 0.0f; }
        float get() const noexcept { return envelope; }

        float process (float x) noexcept
        {
            const float target = std::abs (x);
            const float coefficient = target > envelope ? attackCoefficient : releaseCoefficient;
            envelope = coefficient * envelope + (1.0f - coefficient) * target;
            return envelope;
        }

    private:
        double sampleRate = 48000.0;
        float attackCoefficient = 0.0f;
        float releaseCoefficient = 0.0f;
        float envelope = 0.0f;
    };

    //==================================================================================
    // Per-channel voice of the Fuzzion-5000.
    //
    //   input trim -> pre-emphasis HP -> biased asymmetric clipper
    //   -> interstage coupling -> octave rectifier blend -> second clipper
    //   -> tilt/mid/low-pass tone stack -> gate -> output transformer -> DC block
    //==================================================================================
    class FuzzVoice
    {
    public:
        struct Parameters
        {
            float inputGain = 1.0f;    // input trim only: keys the gate
            float drive = 1.2f;        // fuzz pre-gain, linear
            float bias = 0.0f;         // -1..1 starve
            float tone = 0.5f;         // 0..1
            float octave = 0.0f;       // 0..1
            float gate = 0.0f;         // 0..1
            bool gateEnabled = false;
            bool toneStack = true;
            bool iron = true;
            int mode = 0;
        };

        void prepare (double sampleRate)
        {
            hostSampleRate = juce::jmax (8000.0, sampleRate);

            preHighPass.prepare (hostSampleRate, OnePole::Type::highPass, 120.0f);
            interstageHighPass.prepare (hostSampleRate, OnePole::Type::highPass, 70.0f);
            octaveHighPass.prepare (hostSampleRate, OnePole::Type::highPass, 180.0f);
            tiltLowPass.prepare (hostSampleRate, OnePole::Type::lowPass, 700.0f);
            toneLowPassA.prepare (hostSampleRate, OnePole::Type::lowPass, 7000.0f);
            toneLowPassB.prepare (hostSampleRate, OnePole::Type::lowPass, 9500.0f);
            ironLowPassA.prepare (hostSampleRate, OnePole::Type::lowPass, 9500.0f);
            ironLowPassB.prepare (hostSampleRate, OnePole::Type::lowPass, 10500.0f);
            ironHighPass.prepare (hostSampleRate, OnePole::Type::highPass, 28.0f);
            outputHighPass.prepare (hostSampleRate, OnePole::Type::highPass, 16.0f);
            midPeak.prepare (hostSampleRate);

            inputGateEnvelope.prepare (hostSampleRate, 0.5f, 40.0f);
            saturationSmoother = static_cast<float> (
                std::exp (-1.0 / (0.025 * hostSampleRate)));

            const float gateAttackMs = 1.5f;
            const float gateReleaseMs = 45.0f;
            gateAttackCoefficient = static_cast<float> (
                std::exp (-1.0 / (static_cast<double> (gateAttackMs) * 0.001 * hostSampleRate)));
            gateReleaseCoefficient = static_cast<float> (
                std::exp (-1.0 / (static_cast<double> (gateReleaseMs) * 0.001 * hostSampleRate)));

            preGainGlide.reset (hostSampleRate, 0.025);
            biasGlide.reset (hostSampleRate, 0.030);
            octaveGlide.reset (hostSampleRate, 0.025);
            ironGlide.reset (hostSampleRate, 0.030);

            reset();
        }

        void reset()
        {
            preHighPass.reset();
            interstageHighPass.reset();
            octaveHighPass.reset();
            tiltLowPass.reset();
            toneLowPassA.reset();
            toneLowPassB.reset();
            ironLowPassA.reset();
            ironLowPassB.reset();
            ironHighPass.reset();
            outputHighPass.reset();
            midPeak.reset();
            inputGateEnvelope.reset();
            gateGain = 1.0f;
            saturation = 0.0f;
            toneValue = 0.5f;
            gateAmount = 0.0f;
            gateThresholdDb = -200.0f;
            profile = modeProfile (0);
            targetProfile = profile;

            inputGain = 1.0f;
            preGainGlide.setCurrentAndTargetValue (1.2f);
            biasGlide.setCurrentAndTargetValue (0.0f);
            octaveGlide.setCurrentAndTargetValue (0.0f);
            ironGlide.setCurrentAndTargetValue (1.0f);
            updateFilters();
        }

        // Called once per oversampled block, before processSample().
        void setParameters (const Parameters& parameters, int numSamples) noexcept
        {
            inputGain = juce::jmax (0.0f, parameters.inputGain);
            preGainGlide.setTargetValue (juce::jmax (0.0f, parameters.inputGain * parameters.drive));
            biasGlide.setTargetValue (juce::jlimit (-1.0f, 1.0f, parameters.bias));
            octaveGlide.setTargetValue (juce::jlimit (0.0f, 1.0f, parameters.octave));
            ironGlide.setTargetValue (parameters.iron ? 1.0f : 0.0f);

            gateAmount = juce::jlimit (0.0f, 1.0f, parameters.gate);
            gateEnabled = parameters.gateEnabled;
            toneStackEnabled = parameters.toneStack;

            // The gate is keyed from the un-driven input trim so it follows the
            // player, not the saturated fuzz output. This is what makes a gated
            // fuzz chop between notes instead of sustaining its own noise floor.
            gateThresholdDb = gateEnabled ? juce::jmap (gateAmount, 0.0f, 1.0f, -72.0f, -24.0f)
                                          : -200.0f;

            targetProfile = modeProfile (parameters.mode);

            // Mode transitions glide instead of snapping, so switching character
            // with the transport running never produces a discontinuity.
            constexpr float profileGlide = 0.42f;
            profile.kneePositive += (targetProfile.kneePositive - profile.kneePositive) * profileGlide;
            profile.kneeNegative += (targetProfile.kneeNegative - profile.kneeNegative) * profileGlide;
            profile.hardness += (targetProfile.hardness - profile.hardness) * profileGlide;
            profile.stage2Gain += (targetProfile.stage2Gain - profile.stage2Gain) * profileGlide;
            profile.preHpHz += (targetProfile.preHpHz - profile.preHpHz) * profileGlide;
            profile.interHpHz += (targetProfile.interHpHz - profile.interHpHz) * profileGlide;
            profile.postLpHz += (targetProfile.postLpHz - profile.postLpHz) * profileGlide;
            profile.midHz += (targetProfile.midHz - profile.midHz) * profileGlide;
            profile.midGainDb += (targetProfile.midGainDb - profile.midGainDb) * profileGlide;

            const float toneTarget = juce::jlimit (0.0f, 1.0f, parameters.tone);
            toneValue += (toneTarget - toneValue) * 0.42f;

            juce::ignoreUnused (numSamples);
            updateFilters();
        }

        float processSample (float input) noexcept
        {
            const float preGain = preGainGlide.getNextValue();
            const float bias = biasGlide.getNextValue();
            const float octaveAmount = octaveGlide.getNextValue();
            const float ironAmount = ironGlide.getNextValue();

            // 1. Input trim, fuzz drive and pre-emphasis low cut. The gate uses
            //    the input trim alone, before the fuzz gain, as its key signal.
            const float gateKey = inputGateEnvelope.process (std::abs (input * inputGain));
            float signal = preHighPass.process (input * preGain);

            // 2. Biased asymmetric transistor clipper.
            const float starve = std::abs (bias);
            const float biased = signal + bias * (0.72f + 0.35f * starve);
            float stage = asymmetricClip (biased, profile.kneePositive, profile.kneeNegative,
                                           profile.hardness);

            // Measure how hard the first clipper is working for the SAT readout.
            const float knee = biased >= 0.0f ? profile.kneePositive : profile.kneeNegative;
            const float instantaneousSaturation =
                1.0f - std::exp (-std::abs (biased) / juce::jmax (0.08f, knee * 1.8f));
            saturation = saturation * saturationSmoother
                         + instantaneousSaturation * (1.0f - saturationSmoother);

            // 3. Interstage coupling then the full-wave octave rectifier.
            stage = interstageHighPass.process (stage);

            const float rectified = octaveHighPass.process (std::abs (stage));
            stage += octaveAmount * (rectified * 1.7f - stage);

            // 4. Second gain stage with its own asymmetry.
            stage *= profile.stage2Gain * (1.0f - 0.45f * starve);
            stage = asymmetricClip (stage, profile.kneePositive * 0.92f,
                                    profile.kneeNegative * 0.92f,
                                    profile.hardness * 1.12f);

            // 5. Tone stack: tilt around a 700 Hz pivot, fixed mid contour and a
            //    character low-pass ceiling that the Tone knob can darken further.
            const float low = tiltLowPass.process (stage);
            const float high = stage - low;
            const float tilt = juce::jlimit (0.0f, 1.0f, toneValue);
            stage = low * (1.4f - 0.8f * tilt) + high * (0.6f + 0.8f * tilt);

            stage = midPeak.process (stage);
            stage = toneLowPassA.process (stage);
            stage = toneLowPassB.process (stage);

            // 6. Downward expander / noise gate, keyed from the input trim.
            const float envelopeDb = juce::Decibels::gainToDecibels (gateKey, -120.0f);
            const bool gateOpen = envelopeDb > gateThresholdDb;
            const float gateCoefficient = gateOpen ? gateAttackCoefficient : gateReleaseCoefficient;
            gateGain = gateCoefficient * gateGain + (1.0f - gateCoefficient) * (gateOpen ? 1.0f : 0.0f);
            stage *= gateGain;

            // 7. Output transformer: soft iron saturation and band limiting.
            if (ironAmount > 0.0001f)
            {
                const float saturated = std::tanh (stage * 1.35f) / 1.35f;
                const float iron = ironHighPass.process (ironLowPassB.process (
                    ironLowPassA.process (saturated)));
                stage += ironAmount * 1.055f * (iron - stage);
            }

            // 8. Final DC blocker: removes any bias-derived offset.
            return outputHighPass.process (stage);
        }

        float getSaturation() const noexcept { return juce::jlimit (0.0f, 1.0f, saturation); }
        float getGateReduction() const noexcept { return 1.0f - juce::jlimit (0.0f, 1.0f, gateGain); }

    private:
        void updateFilters() noexcept
        {
            preHighPass.setCutoff (profile.preHpHz);
            interstageHighPass.setCutoff (profile.interHpHz);

            const float toneCutoff = 600.0f * std::pow (24.0f, juce::jlimit (0.0f, 1.0f, toneValue));
            const float effectiveLowPass = toneStackEnabled
                                               ? juce::jmin (profile.postLpHz, toneCutoff)
                                               : profile.postLpHz;

            toneLowPassA.setCutoff (effectiveLowPass);
            toneLowPassB.setCutoff (juce::jmin (effectiveLowPass * 1.55f,
                                                static_cast<float> (hostSampleRate) * 0.45f));

            midPeak.setParameters (profile.midHz, 1.05f,
                                   toneStackEnabled ? profile.midGainDb : profile.midGainDb * 0.45f);
        }

        double hostSampleRate = 48000.0;

        OnePole preHighPass, interstageHighPass, octaveHighPass;
        OnePole tiltLowPass, toneLowPassA, toneLowPassB;
        OnePole ironLowPassA, ironLowPassB, ironHighPass, outputHighPass;
        PeakingFilter midPeak;

        EnvelopeFollower inputGateEnvelope;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preGainGlide, biasGlide,
                                                                       octaveGlide, ironGlide;

        ModeProfile profile, targetProfile;
        float inputGain = 1.0f;
        float toneValue = 0.5f;
        float gateAmount = 0.0f;
        bool gateEnabled = false;
        bool toneStackEnabled = true;
        float gateThresholdDb = -200.0f;
        float gateGain = 1.0f;
        float gateAttackCoefficient = 0.0f;
        float gateReleaseCoefficient = 0.0f;
        float saturation = 0.0f;
        float saturationSmoother = 0.0f;
    };
} // namespace fuzzion::dsp
