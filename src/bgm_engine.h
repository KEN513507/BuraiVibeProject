#pragma once

// Stage 1 BGM: expands STAGE1_SCORE into 512 sixteenth-note ticks and plays it on a
// 4-channel 2A03-style software synth (Pulse1 lead, Pulse2 echo/harmony/arpeggio,
// Triangle bass + kick/tom sweep, Noise snare/hat). No SDL dependency.
//
// Timing runs on the sample clock (735 samples = 1 frame, 5 frames = 1 tick), so the
// tempo stays at 180 BPM regardless of the game's frame rate and the output does not
// depend on how the audio device splits its callback buffers.

#include <array>
#include <cstdint>

#include "stage1_score.h"

constexpr int BGM_SAMPLE_RATE = 44100;
constexpr int BGM_FRAME_SAMPLES = 735;  // 44100 / 60
constexpr int BGM_TICK_SAMPLES = BGM_FRAME_SAMPLES * STAGE1_FRAMES_PER_TICK;
constexpr int BGM_TICK_COUNT = STAGE1_SCORE_STEPS * STAGE1_TICKS_PER_BEAT;  // 512

// Score expanded to ticks. Frequencies are Hz; 0 means silence. Tie sentinels are resolved.
struct BgmTrack {
    std::array<float, BGM_TICK_COUNT> melody{};
    std::array<float, BGM_TICK_COUNT> harmony{};
    std::array<float, BGM_TICK_COUNT> bass{};
    std::array<bool, BGM_TICK_COUNT> retrigMelody{};
    std::array<bool, BGM_TICK_COUNT> retrigHarmony{};
    std::array<uint8_t, BGM_TICK_COUNT> drum{};  // Stage1Notes::Drum
    std::array<Pulse2Mode, BGM_TICK_COUNT> pulse2Mode{};
    std::array<std::array<float, 4>, BGM_TICK_COUNT> arpeggio{};
};

BgmTrack CompileStage1Score();

// Square wave in [-1, 1]. phase is normalized to [0, 1).
float PulseSample(float phase, float duty);

class BgmSynth {
public:
    explicit BgmSynth(const BgmTrack& track) : track_(track) {}

    void Start();  // restart the song from tick 0
    void Stop();   // silence the BGM voices; sound effects keep playing
    bool IsPlaying() const { return playing_; }

    // Sound effects borrow a channel for their duration: shot -> Pulse2, explosion -> Noise.
    void TriggerShot();
    void TriggerExplosion();

    void Render(int16_t* out, int samples);

    // Inspection for tests
    int CurrentTick() const { return tick_; }
    float Pulse1Frequency() const { return p1_.env > 0.0f ? p1_.freq : 0.0f; }
    float Pulse2Frequency() const { return p2_.env > 0.0f ? p2_.freq : 0.0f; }
    float Pulse2Duty() const { return p2_.duty; }
    float Pulse2Gain() const { return p2_.gain; }
    float TriangleFrequency() const;
    int NoiseSamplesLeft() const { return noiseLeft_; }
    int NoisePeriodCycles() const { return noisePeriod_; }

private:
    struct PulseVoice {
        float freq = 0.0f;
        float phase = 0.0f;
        float duty = 0.5f;
        float gain = 0.0f;
        float env = 0.0f;
    };

    void OnFrameBoundary();
    void EnterTick(int tick);
    void StartNoise(int periodCycles, int samples, float gain);
    int16_t NextSample();

    const BgmTrack& track_;
    bool playing_ = false;
    int tick_ = 0;
    int samplesIntoTick_ = 0;
    int samplesIntoFrame_ = 0;

    PulseVoice p1_;
    PulseVoice p2_;
    Pulse2Mode p2Mode_ = Pulse2Mode::Echo;
    std::array<float, 4> arpChord_{};
    int arpIndex_ = 0;

    float triBass_ = 0.0f;
    float triPhase_ = 0.0f;
    int drumMacro_ = 0;  // 0 none, 1 kick, 2 tom fill
    int drumFrame_ = 0;

    uint16_t lfsr_ = 1;
    double noiseClock_ = 0.0;
    int noisePeriod_ = 4;
    int noiseLeft_ = 0;
    int noiseTotal_ = 1;
    float noiseGain_ = 0.0f;

    int shotLeft_ = 0;
    float shotFreq_ = 0.0f;
    float shotPhase_ = 0.0f;
    int explosionLeft_ = 0;
};
