#pragma once

#include "Engine/EngineParams.h"

#include <algorithm>
#include <cmath>

namespace lumen
{
// ADSR with curve shaping (SPEC section 7).
//
// Attack rises 0->1 over the attack time as x^p, p = 2 * 3^(-curve): the
// default (curve 0) is the classic convex x^2 ramp. Decay and release fall
// as remaining = (1-x)^q, q = 3^(-curve): linear at curve 0, exponential
// feel at curve -1. Retrigger starts the attack from the current level so
// voice stealing never clicks. A releasing envelope ends (goes idle) below
// -90 dB (SPEC section 7).
class Envelope
{
public:
    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setParameters (const EnvParams& p) noexcept
    {
        params = p;
        attackPow  = 2.0f * std::pow (3.0f, -p.curve);
        decayPow   = std::pow (3.0f, -p.curve);
    }

    void noteOn() noexcept
    {
        startLevel = level;
        stage = Stage::attack;
        x = 0.0;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::idle)
            return;
        releaseLevel = level;
        stage = Stage::release;
        x = 0.0;
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
        x = 0.0;
    }

    bool isActive() const noexcept    { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    float value() const noexcept      { return level; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
            {
                x += 1.0 / (std::max (0.0005f, params.attackSeconds) * sampleRate);
                if (x >= 1.0)
                {
                    level = 1.0f;
                    stage = Stage::decay;
                    x = 0.0;
                }
                else
                {
                    level = startLevel + (1.0f - startLevel)
                                         * std::pow (static_cast<float> (x), attackPow);
                }
                break;
            }
            case Stage::decay:
            {
                x += 1.0 / (std::max (0.0005f, params.decaySeconds) * sampleRate);
                if (x >= 1.0)
                {
                    level = params.sustain;
                    stage = Stage::sustain;
                }
                else
                {
                    const float remaining = std::pow (1.0f - static_cast<float> (x), decayPow);
                    level = params.sustain + (1.0f - params.sustain) * remaining;
                }
                break;
            }
            case Stage::sustain:
            {
                level = params.sustain;
                break;
            }
            case Stage::release:
            {
                x += 1.0 / (std::max (0.001f, params.releaseSeconds) * sampleRate);
                if (x >= 1.0)
                {
                    level = 0.0f;
                    stage = Stage::idle;
                }
                else
                {
                    level = releaseLevel * std::pow (1.0f - static_cast<float> (x), decayPow);
                    if (level < kEndThreshold)
                    {
                        level = 0.0f;
                        stage = Stage::idle;
                    }
                }
                break;
            }
            case Stage::idle:
                break;
        }
        return level;
    }

private:
    static constexpr float kEndThreshold = 3.162e-5f; // -90 dB

    enum class Stage { idle, attack, decay, sustain, release };

    EnvParams params {};
    Stage stage = Stage::idle;
    double sampleRate = 48000.0;
    double x = 0.0;          // normalized position within the current stage
    float level = 0.0f;
    float startLevel = 0.0f;
    float releaseLevel = 0.0f;
    float attackPow = 2.0f;
    float decayPow = 1.0f;
};
} // namespace lumen
