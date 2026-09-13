#include "bgm_engine.h"

#include <algorithm>
#include <cmath>

namespace {

// Channel levels. The sum of every voice at full level stays below 1.0 (no clipping).
constexpr float kLeadGain = 0.10f;
constexpr float kEchoGain = 0.045f;
constexpr float kHarmonyGain = 0.065f;
constexpr float kArpeggioGain = 0.045f;
constexpr float kTriangleGain = 0.12f;
constexpr float kSnareGain = 0.09f;
constexpr float kHatGain = 0.045f;
constexpr float kShotGain = 0.09f;
constexpr float kExplosionGain = 0.16f;

constexpr float kLeadDuty = 0.5f;
constexpr float kEchoDuty = 0.125f;
constexpr float kHarmonyDuty = 0.25f;

// Per-frame volume envelope: quick fall to a sustain level, then a slow fade so held notes keep sounding.
constexpr float kEnvAttackFall = 0.075f;
constexpr float kEnvSustainKnee = 0.55f;
constexpr float kEnvSustainFall = 0.008f;
constexpr float kEnvFloor = 0.30f;

constexpr float kKickSweep[4] = {440.0f, 180.0f, 90.0f, 55.0f};
constexpr float kTomSweep[4] = {330.0f, 247.0f, 196.0f, 165.0f};

// NTSC 2A03 noise timer periods in CPU cycles (hat = register 0, snare = register 0x0A).
constexpr int kHatPeriod = 4;
constexpr int kSnarePeriod = 380;
constexpr int kExplosionPeriod = 1016;
constexpr int kHatSamples = BGM_FRAME_SAMPLES;
constexpr int kSnareSamples = BGM_FRAME_SAMPLES * 3;
constexpr double kCpuCyclesPerSample = 1789773.0 / BGM_SAMPLE_RATE;

constexpr int kShotSamples = BGM_SAMPLE_RATE * 8 / 100;        // 0.08 s
constexpr int kExplosionSamples = BGM_SAMPLE_RATE * 25 / 100;  // 0.25 s

// >0 starts a note, 0 is a rest, <0 (H) ties the previous state.
void Resolve(float event, float& current, bool& retrig) {
    retrig = false;
    if (event > 0.0f) {
        current = event;
        retrig = true;
    } else if (event == 0.0f) {
        current = 0.0f;
    }
}

float AdvancePhase(float phase, float freq) {
    phase += freq / BGM_SAMPLE_RATE;
    return phase - std::floor(phase);
}

// 2A03 triangle: 32-step, 4-bit staircase.
float TriangleSample(float phase) {
    const int step = static_cast<int>(phase * 32.0f) & 31;
    const int level = step < 16 ? 15 - step : step - 16;
    return level / 7.5f - 1.0f;
}

void DecayEnvelope(float& env) {
    if (env <= 0.0f) return;
    if (env > kEnvSustainKnee) env = std::max(kEnvSustainKnee, env - kEnvAttackFall);
    else env = std::max(kEnvFloor, env - kEnvSustainFall);
}

} // namespace

BgmTrack CompileStage1Score() {
    BgmTrack t;
    float lead = 0.0f, counter = 0.0f, bass = 0.0f;
    for (int row = 0; row < STAGE1_SCORE_STEPS; ++row) {
        const NoteStep& s = STAGE1_SCORE[row];
        for (int k = 0; k < STAGE1_TICKS_PER_BEAT; ++k) {
            const int tick = row * STAGE1_TICKS_PER_BEAT + k;
            float leadEvent = -1.0f, counterEvent = -1.0f, bassEvent = -1.0f;
            uint8_t drum = Stage1Notes::None;
            if (k == 0) {
                leadEvent = s.pulse1Freq; counterEvent = s.pulse2Freq; bassEvent = s.triFreq; drum = s.drumType;
            } else if (k == 1) {
                drum = s.drumSixteenth1;
            } else if (k == 2) {
                leadEvent = s.pulse1OffbeatFreq; counterEvent = s.pulse2OffbeatFreq; bassEvent = s.triOffbeatFreq;
                drum = s.drumOffbeat;
            } else {
                drum = s.drumSixteenth3;
            }
            bool retrig = false;
            Resolve(leadEvent, lead, retrig);
            t.melody[tick] = lead;
            t.retrigMelody[tick] = retrig;
            Resolve(bassEvent, bass, retrig);
            t.bass[tick] = bass;
            t.drum[tick] = drum;
            t.pulse2Mode[tick] = s.pulse2Mode;
            t.arpeggio[tick] = {s.arpeggio[0], s.arpeggio[1], s.arpeggio[2], s.arpeggio[3]};
            if (s.pulse2Mode == Pulse2Mode::Harmony) {
                Resolve(counterEvent, counter, retrig);
                t.harmony[tick] = counter;
                t.retrigHarmony[tick] = retrig;
            }
        }
    }
    // Echo rows: Pulse 2 repeats the lead one tick later (pitch, articulation and rests).
    for (int tick = 0; tick < BGM_TICK_COUNT; ++tick) {
        if (t.pulse2Mode[tick] != Pulse2Mode::Echo) continue;
        const int prev = (tick + BGM_TICK_COUNT - 1) % BGM_TICK_COUNT;
        t.harmony[tick] = t.melody[prev];
        t.retrigHarmony[tick] = t.retrigMelody[prev];
    }
    return t;
}

float PulseSample(float phase, float duty) {
    return phase < duty ? 1.0f : -1.0f;
}

void BgmSynth::Start() {
    playing_ = true;
    tick_ = 0;
    samplesIntoTick_ = 0;
    samplesIntoFrame_ = 0;
    p1_ = PulseVoice{};
    p2_ = PulseVoice{};
    p2Mode_ = Pulse2Mode::Echo;
    arpIndex_ = 0;
    triBass_ = 0.0f;
    drumMacro_ = 0;
    drumFrame_ = 0;
    noiseLeft_ = 0;
}

void BgmSynth::Stop() {
    playing_ = false;
    p1_.env = 0.0f;
    p2_.env = 0.0f;
    triBass_ = 0.0f;
    drumMacro_ = 0;
    noiseLeft_ = 0;
}

void BgmSynth::TriggerShot() {
    shotLeft_ = kShotSamples;
    shotFreq_ = 880.0f;
}

void BgmSynth::TriggerExplosion() {
    explosionLeft_ = kExplosionSamples;
}

float BgmSynth::TriangleFrequency() const {
    if (drumMacro_ == 1) return kKickSweep[drumFrame_];
    if (drumMacro_ == 2) return kTomSweep[drumFrame_];
    return triBass_;
}

void BgmSynth::StartNoise(int periodCycles, int samples, float gain) {
    noisePeriod_ = periodCycles;
    noiseLeft_ = samples;
    noiseTotal_ = samples;
    noiseGain_ = gain;
}

void BgmSynth::OnFrameBoundary() {
    DecayEnvelope(p1_.env);
    if (p2Mode_ == Pulse2Mode::Arpeggio) {
        arpIndex_ = (arpIndex_ + 1) % 4;
        p2_.freq = arpChord_[arpIndex_];
    } else {
        DecayEnvelope(p2_.env);
    }
    if (drumMacro_ != 0 && ++drumFrame_ >= 4) drumMacro_ = 0;
}

void BgmSynth::EnterTick(int tick) {
    const BgmTrack& t = track_;

    if (t.retrigMelody[tick]) {
        p1_.freq = t.melody[tick];
        p1_.env = 1.0f;
    } else if (t.melody[tick] == 0.0f) {
        p1_.env = 0.0f;
    }
    p1_.duty = kLeadDuty;
    p1_.gain = kLeadGain;

    const Pulse2Mode mode = t.pulse2Mode[tick];
    if (mode == Pulse2Mode::Arpeggio) {
        arpChord_ = t.arpeggio[tick];
        if (p2Mode_ != Pulse2Mode::Arpeggio) {
            arpIndex_ = 0;
            p2_.freq = arpChord_[0];
        }
        p2_.env = 1.0f;
        p2_.duty = kHarmonyDuty;
        p2_.gain = kArpeggioGain;
    } else {
        const bool echo = mode == Pulse2Mode::Echo;
        p2_.duty = echo ? kEchoDuty : kHarmonyDuty;
        p2_.gain = echo ? kEchoGain : kHarmonyGain;
        if (t.retrigHarmony[tick]) {
            p2_.freq = t.harmony[tick];
            p2_.env = 1.0f;
        } else if (t.harmony[tick] == 0.0f) {
            p2_.env = 0.0f;
        } else if (p2Mode_ == Pulse2Mode::Arpeggio) {
            p2_.freq = t.harmony[tick];  // a tie out of the arpeggio holds the scored pitch, not a chord tone
        }
    }
    p2Mode_ = mode;

    triBass_ = t.bass[tick];

    switch (t.drum[tick]) {
        case Stage1Notes::Kick: drumMacro_ = 1; drumFrame_ = 0; break;
        case Stage1Notes::Fill: drumMacro_ = 2; drumFrame_ = 0; break;
        case Stage1Notes::Snare: StartNoise(kSnarePeriod, kSnareSamples, kSnareGain); break;
        case Stage1Notes::Hat: StartNoise(kHatPeriod, kHatSamples, kHatGain); break;
        default: break;
    }
}

int16_t BgmSynth::NextSample() {
    if (samplesIntoFrame_ == 0 && playing_) OnFrameBoundary();
    if (playing_ && samplesIntoTick_ == 0) EnterTick(tick_);

    float mix = 0.0f;

    // Pulse 1: lead
    if (p1_.env > 0.0f && p1_.freq > 0.0f) {
        p1_.phase = AdvancePhase(p1_.phase, p1_.freq);
        mix += PulseSample(p1_.phase, p1_.duty) * p1_.gain * p1_.env;
    }

    // Pulse 2: the shot effect takes the channel over while it plays
    if (shotLeft_ > 0) {
        shotPhase_ = AdvancePhase(shotPhase_, shotFreq_);
        mix += PulseSample(shotPhase_, 0.5f) * kShotGain;
        shotFreq_ *= 0.9995f;
        --shotLeft_;
    } else if (p2_.env > 0.0f && p2_.freq > 0.0f) {
        p2_.phase = AdvancePhase(p2_.phase, p2_.freq);
        mix += PulseSample(p2_.phase, p2_.duty) * p2_.gain * p2_.env;
    }

    // Triangle: bass, replaced by the kick/tom sweep while it runs. No volume envelope.
    const float triFreq = TriangleFrequency();
    if (triFreq > 0.0f) {
        triPhase_ = AdvancePhase(triPhase_, triFreq);
        mix += TriangleSample(triPhase_) * kTriangleGain;
    }

    // Noise: the explosion effect takes the channel over while it plays
    const bool explosion = explosionLeft_ > 0;
    if (explosion || noiseLeft_ > 0) {
        const int period = explosion ? kExplosionPeriod : noisePeriod_;
        noiseClock_ += kCpuCyclesPerSample;
        while (noiseClock_ >= period) {
            noiseClock_ -= period;
            const uint16_t feedback = (lfsr_ ^ (lfsr_ >> 1)) & 1;
            lfsr_ = static_cast<uint16_t>((lfsr_ >> 1) | (feedback << 14));
        }
        const float bit = (lfsr_ & 1) ? -1.0f : 1.0f;
        if (explosion) {
            mix += bit * kExplosionGain * (static_cast<float>(explosionLeft_) / kExplosionSamples);
            --explosionLeft_;
            if (noiseLeft_ > 0) --noiseLeft_;  // the BGM hit keeps its timing while muted
        } else {
            mix += bit * noiseGain_ * (static_cast<float>(noiseLeft_) / noiseTotal_);
            --noiseLeft_;
        }
    }

    if (++samplesIntoFrame_ == BGM_FRAME_SAMPLES) samplesIntoFrame_ = 0;
    if (playing_ && ++samplesIntoTick_ == BGM_TICK_SAMPLES) {
        samplesIntoTick_ = 0;
        tick_ = (tick_ + 1) % BGM_TICK_COUNT;
    }

    mix = std::clamp(mix, -1.0f, 1.0f);
    return static_cast<int16_t>(std::lround(mix * 32000.0f));
}

void BgmSynth::Render(int16_t* out, int samples) {
    for (int i = 0; i < samples; ++i) out[i] = NextSample();
}
