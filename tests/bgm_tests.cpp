// No window or audio device is opened: exercise generation, timing and PCM in memory.
#define SDL_MAIN_HANDLED
#define main BuraiGameMain
#include "../src/main.cpp"
#undef main
#include <cstdlib>
#include <set>
#include <fstream>

void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}

bool SameNotes(const TrackData& a, const TrackData& b) {
    return std::equal(std::begin(a.melody), std::end(a.melody), std::begin(b.melody)) &&
           std::equal(std::begin(a.harmony), std::end(a.harmony), std::begin(b.harmony)) &&
           std::equal(std::begin(a.bass), std::end(a.bass), std::begin(b.bass));
}


void CheckStage1(const TrackData& track) {
    using namespace Stage1Notes;
    auto close = [](float a, float b) { return std::abs(a - b) < .01f; };
    Check(track.structuredScore && track.stepCount == 512, "Stage 1 must retain its 128-beat score");
    Check(close(track.melody[0], Frequency(A4)), "Opening tonic changed");
    // Anticipation: attack A on the previous bar's offbeat, tie across bar 2.
    Check(track.retrigMelody[14] && !track.retrigMelody[16] &&
          close(track.melody[14], track.melody[16]), "Missing cross-bar syncopation");
    int raisedSevenths = 0;
    for (int tick = 0; tick < track.stepCount; ++tick) {
        Check(track.melody[tick] >= 0 && track.harmony[tick] >= 0 && track.bass[tick] >= 0,
              "Tie sentinel leaked into playback");
        if (track.retrigMelody[tick] &&
            (close(track.melody[tick], Frequency(GS4)) || close(track.melody[tick], Frequency(GS5))))
            ++raisedSevenths;
        if (track.pulse2Mode[tick] == Pulse2Mode::Echo) {
            Check(close(track.harmony[tick], tick ? track.melody[tick - 1] : 0),
                  "Echo must repeat the identical note one sixteenth later");
            if (tick > 0 && track.pulse2Mode[tick - 1] == Pulse2Mode::Echo)
                Check(track.retrigHarmony[tick] == track.retrigMelody[tick - 1],
                      "Echo must delay articulation as well as pitch");
        }
    }
    Check(raisedSevenths >= 16, "Harmonic-minor leading tone is missing from the melody");
    for (int tick = 28 * 4 + 1; tick < 31 * 4; ++tick)
        Check(close(track.melody[tick], Frequency(A4)) && !track.retrigMelody[tick],
              "A answer needs its three-beat tonic tie");
    Check(track.melody[124] == 0 && track.harmony[124] > 0 &&
          track.harmony[125] == 0, "Echo tail must end one tick after the rest");
    Check(close(track.melody[128], Frequency(A5)), "A prime must raise the hook");
    Check(track.drumPattern[128] == Kick && track.drumPattern[134] == Snare &&
          track.drumPattern[140] == Snare, "A-prime 3+3+2 accents are missing");
    for (int tick = 256; tick < 384; ++tick) {
        Check(track.pulse2Mode[tick] == Pulse2Mode::Harmony, "B must leave echo mode");
        Check(track.retrigMelody[tick] == track.retrigHarmony[tick],
              "B voices must articulate simultaneously");
        if (track.melody[tick] > 0) {
            const float interval = 12 * std::log2(track.harmony[tick] / track.melody[tick]);
            Check(std::abs(interval - 3) < .001f || std::abs(interval - 4) < .001f,
                  "B must use the authored upper thirds");
        }
    }
    Check(std::count(track.retrigMelody + 256, track.retrigMelody + 384, true) >
          std::count(track.retrigMelody, track.retrigMelody + 128, true),
          "B must be denser than A");
    Check(track.bass[0] == 0 && close(track.bass[2], Frequency(A2)), "Bass must enter offbeat");
    for (int bar = 3; bar < 32; bar += 4) {
        const int tick = bar * 16;
        Check(track.bass[tick] > track.bass[tick + 2] &&
              track.bass[tick + 2] > track.bass[tick + 4], "Four-bar descending bass run is missing");
    }
    Check(close(track.melody[510], Frequency(GS4)) && close(track.bass[510], Frequency(E2)),
          "Final dominant pickup must return to A");
    int frames = 0;
    for (int i = 0; i < track.stepCount; ++i) frames += track.stepFrames[i];
    Check(frames == 2560, "32-bar duration changed");
}

void CheckStage1Macros() {
    using namespace Stage1Notes;
    TrackData track = g_playlist[1];
    GenerateTrackData(track);
    g_audioMuted = false;
    g_apu = APU{};
    ApplyBgmStep(track, 1);
    Check(g_apu.dutyPulse2 == .125f && g_apu.gainPulse2 == g_apu.gainPulse1 * .5f,
          "Echo needs narrow duty and half gain");
    ApplyBgmStep(track, 256);
    Check(g_apu.dutyPulse2 == .25f && g_apu.gainPulse2 > .06f,
          "Chorus must change pulse 2's timbre/gain");

    int16_t frame[735] = {};
    auto renderFrame = [&]() { AudioCallback(nullptr, reinterpret_cast<Uint8*>(frame), sizeof(frame)); };
    g_apu = APU{};
    track.bass[0] = Frequency(A2);
    ApplyBgmStep(track, 0);
    Check(!g_apu.bgmNoiseTrigger, "Kick must not start a separate noise voice");
    const float expectedKick[4] = {440,180,90,55};
    for (float hz : expectedKick) {
        Check(CurrentTriangleFrequency() == hz, "Kick pitch sweep timing is wrong");
        Check(g_apu.gainTriangle == track.gainTriangle, "Triangle drum must not modulate its gain");
        renderFrame();
    }
    Check(CurrentTriangleFrequency() == Frequency(A2), "Bass must resume after the exclusive kick");
    track.drumPattern[0] = Fill;
    ApplyBgmStep(track, 0);
    Check(CurrentTriangleFrequency() == 330 && !g_apu.bgmNoiseTrigger, "Fill must use triangle tom");

    g_apu = APU{};
    ApplyBgmStep(track, 11 * 16); // E7 arpeggio in bar 12.
    Check(g_apu.pulse2Mode == Pulse2Mode::Arpeggio, "Arpeggio mode missing");
    const int arpNotes[4] = {E4,GS4,B4,D5};
    for (int i = 0; i < 4; ++i) {
        Check(CurrentPulse2Frequency() == Frequency(arpNotes[i]),
              "Arpeggio must advance once per audio frame");
        renderFrame();
    }
    Check(CurrentPulse2Frequency() == Frequency(E4), "Arpeggio failed to wrap");

    for (int drum : {Snare, Hat}) {
        g_apu = APU{};
        track.melody[0] = track.harmony[0] = track.bass[0] = 0;
        track.retrigMelody[0] = track.retrigHarmony[0] = false;
        track.drumPattern[0] = static_cast<uint8_t>(drum);
        ApplyBgmStep(track, 0);
        const int samples = drum == Hat ? 735 : 2205;
        Check(g_apu.noiseSamplesLeft == samples, "Incorrect drum gate duration");
        Check(g_apu.noisePeriodCycles == (drum == Hat ? 4 : 380), "Noise rate order is reversed");
        std::vector<int16_t> tail(samples - 1);
        AudioCallback(nullptr, reinterpret_cast<Uint8*>(tail.data()), static_cast<int>(tail.size() * 2));
        Check(g_apu.envNoise > 0, "Noise stopped before its gate");
        int16_t last;
        AudioCallback(nullptr, reinterpret_cast<Uint8*>(&last), sizeof(last));
        Check(g_apu.envNoise == 0, "Noise gate did not expire at the exact sample");
        renderFrame();
        Check(std::all_of(std::begin(frame), std::end(frame), [](int16_t n) { return n == 0; }),
              "Noise continued after its gate");
    }

    // Sample-clock macros must be independent of SDL callback chunk size.
    GenerateTrackData(track);
    g_apu = APU{};
    ApplyBgmStep(track, 31 * 16 + 8);
    APU initial = g_apu;
    std::vector<int16_t> whole(4000), split(4000);
    AudioCallback(nullptr, reinterpret_cast<Uint8*>(whole.data()), 8000);
    g_apu = initial;
    int offset = 0;
    for (int size : {1024, 257, 1, 735, 1983}) {
        AudioCallback(nullptr, reinterpret_cast<Uint8*>(split.data() + offset), size * 2);
        offset += size;
    }
    Check(whole == split, "Macro timing depends on callback buffer size");

    TrackData legacy = g_playlist[8];
    GenerateTrackData(legacy);
    ApplyBgmStep(legacy, 0);
    Check(g_apu.pulse2Mode == Pulse2Mode::Harmony && g_apu.triangleDrumSamples == 0 &&
          !g_apu.structuredScore, "Score macros leaked into another track");
}

// Uses the same frame sequencer and callback as the game, but never opens a device.
void RenderStage1(const char* path) {
    TrackData track = g_playlist[1];
    GenerateTrackData(track);
    g_apu = APU{};
    g_audioMuted = false;
    BgmSequencer sequencer;
    std::vector<int16_t> pcm;
    const int frames = track.stepCount * STAGE1_FRAMES_PER_TICK * 2;
    pcm.resize(frames * 735);
    for (int frame = 0; frame < frames; ++frame) {
        int step = sequencer.Tick(track);
        if (step >= 0) ApplyBgmStep(track, step);
        AudioCallback(nullptr, reinterpret_cast<Uint8*>(pcm.data() + frame * 735), 735 * 2);
    }
    std::ofstream out(path, std::ios::binary);
    Check(static_cast<bool>(out), "Cannot open WAV destination");
    auto u16 = [&](uint16_t v) { out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8)); };
    auto u32 = [&](uint32_t v) { u16(static_cast<uint16_t>(v)); u16(static_cast<uint16_t>(v >> 16)); };
    const uint32_t bytes = static_cast<uint32_t>(pcm.size() * 2);
    out.write("RIFF", 4); u32(36 + bytes); out.write("WAVEfmt ", 8);
    u32(16); u16(1); u16(1); u32(44100); u32(88200); u16(2); u16(16);
    out.write("data", 4); u32(bytes);
    for (int16_t sample : pcm) u16(static_cast<uint16_t>(sample));
    Check(static_cast<bool>(out), "WAV write failed");
    std::cout << "Stage 1 WAV: 32 bars x 2 loops, 180 BPM at 60 FPS, " << pcm.size() / 44100.0
              << " seconds, " << path << '\n';
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--render-stage1") {
        RenderStage1(argv[2]);
        return 0;
    }
    CheckStage1Macros();
    std::set<uint64_t> audioSignatures;
    const int expectedSpeeds[10] = {7,5,6,10,3,5,4,6,12,5};
    // Recorded before the Stage 1 change, including the usable game-over track.
    const uint64_t legacyPcm[10] = {
        2391495139406345622ull, 0, 10604283163754102918ull, 3822046641297618904ull,
        10037951398179685957ull, 8803081222357933369ull, 5560620858990033312ull,
        856847152131695497ull, 17063788704197304734ull, 2233204514329364952ull
    };
    for (int id = 0; id < 10; ++id) {
        TrackData track = g_playlist[id];
        GenerateTrackData(track);
        Check(track.bgmSpeed == expectedSpeeds[id], "Track speed mismatch");
        TrackData same = track;
        GenerateTrackData(same);
        Check(SameNotes(track, same), "Same seed must reproduce the same notes");
        Check(std::equal(std::begin(track.drumPattern), std::end(track.drumPattern),
                         std::begin(same.drumPattern)), "Drums must be deterministic");
        TrackData reroll = track;
        reroll.seed += 77;
        GenerateTrackData(reroll);
        if (id == 1) {
            CheckStage1(track);
            Check(SameNotes(track, reroll), "Stage 1 score must not change with the seed");
        } else if (SameNotes(track, reroll)) {
            std::cerr << "Reroll did not vary track " << id + 1 << '\n';
            return 1;
        }
        Check(reroll.bgmSpeed == track.bgmSpeed && reroll.dutyPulse1 == track.dutyPulse1 &&
              reroll.dutyPulse2 == track.dutyPulse2, "Reroll lost track identity");
        if (id != 1) Check(!std::equal(track.melody, track.melody + 16, track.melody + 16),
              "A and B melodies must differ");
        if (id == 3) {
            Check(std::all_of(std::begin(track.drumPattern), std::end(track.drumPattern),
                             [](uint8_t d) { return d == 0; }), "Maze must have silent drums");
        } else if (id != 1) {
            Check(!std::equal(track.drumPattern, track.drumPattern + 16, track.drumPattern + 16),
                  "A and B drums must differ");
        }

        BgmSequencer sequencer;
        Check(sequencer.Tick(track) == 0, "Selection must start immediately at step zero");
        for (int event = 0; event < track.stepCount * 3; ++event) {
            const int step = event % track.stepCount;
            for (int frame = 1; frame < track.stepFrames[step]; ++frame)
                Check(sequencer.Tick(track) == -1, "Step fired before its frame interval");
            Check(sequencer.Tick(track) == (step + 1) % track.stepCount, "Step interval or wrap is wrong");
        }
        sequencer.Reset();
        Check(sequencer.Tick(reroll) == 0, "Switching must discard the old countdown");

        g_apu = APU{};
        g_audioMuted = false;
        uint64_t signature = 1469598103934665603ull;
        bool audible = false;
        for (int step = 0; step < track.stepCount; ++step) {
            ApplyBgmStep(track, step);
            const int drum = track.drumPattern[step];
            Check(g_apu.bgmNoiseTrigger == (id == 1 ? drum == 2 || drum == 3 : drum != 0), "Drum channel routing is wrong");
            const bool hit = track.retrigMelody[step];
            const float previousEnv = g_apu.envPulse1;
            std::vector<int16_t> pcm(track.stepFrames[step] * 735);
            AudioCallback(nullptr, reinterpret_cast<Uint8*>(pcm.data()), static_cast<int>(pcm.size() * 2));
            Check(!g_apu.retrigPulse1 && !g_apu.retrigPulse2 && !g_apu.retrigNoise,
                  "Callback did not consume triggers");
            Check(std::isfinite(g_apu.envPulse1) && g_apu.envPulse1 >= 0 && g_apu.envPulse1 <= 1,
                  "Invalid pulse envelope");
            if (!hit) Check(g_apu.envPulse1 <= previousEnv, "A sustained note was retriggered");
            for (int16_t sample : pcm) {
                Check(std::abs(static_cast<int>(sample)) < 32767, "PCM clipping");
                audible |= sample != 0;
                signature = (signature ^ static_cast<uint16_t>(sample)) * 1099511628211ull;
            }
        }
        Check(audible, "Track produced no audio samples");
        audioSignatures.insert(signature);
        if (id != 1) Check(signature == legacyPcm[id], "A non-Stage-1 track changed its PCM output");
        std::cout << "Track " << id + 1 << ": " << track.stepCount << " ticks, timing/score/PCM OK\n";

        const ScalePreset& scale = SCALES[TRACK_STYLES[id].scale];
        for (uint32_t seed = 0; seed < 128; ++seed) {
            track.seed = seed == 127 ? UINT32_MAX : seed;
            GenerateTrackData(track);
            if (id == 1) Check(SameNotes(track, same), "Stage 1 must be independent of all tested seeds");
            for (int i = 0; i < track.stepCount; ++i) {
                Check(track.stepFrames[i] >= 3 && track.stepFrames[i] <= 12, "Step duration out of range");
                Check(track.drumPattern[i] <= 4, "Invalid drum code");
                for (float freq : {track.melody[i], track.harmony[i], track.bass[i]})
                    Check(std::isfinite(freq) && freq >= 0 && freq < 20000, "Invalid note frequency");
                if (id == 1) continue;
                if (track.melody[i] == 0) continue;
                int midi = static_cast<int>(std::lround(69 + 12 * std::log2(track.melody[i] / 440.0f)));
                int note = ((midi - TRACK_STYLES[id].tonicMidi) % 12 + 12) % 12;
                Check(std::find(scale.notes, scale.notes + scale.count, note) != scale.notes + scale.count,
                      "Melody escaped its selected scale");
            }
        }
    }
    Check(audioSignatures.size() == 10, "Tracks produced duplicate PCM");

    for (float duty : {.125f, .25f, .5f}) {
        float phase = 0, sum = 0;
        int positive = 0;
        for (int i = 0; i < 44100; ++i) {
            float value = PulseSample(phase, 441.0f, duty);
            positive += value > 0;
            sum += value;
        }
        Check(std::abs(positive / 44100.0f - duty) < .011f, "Incorrect duty ratio");
        Check(std::abs(sum / 44100.0f) < .02f, "Pulse output has a DC offset");
    }

    srand(123);
    const int expectedRandom = rand();
    srand(123);
    GenerateTrackData(g_playlist[0]);
    Check(rand() == expectedRandom, "Generating music changed gameplay's random sequence");

    g_audioMuted = true;
    int16_t muted[64];
    std::fill(std::begin(muted), std::end(muted), 1234);
    AudioCallback(nullptr, reinterpret_cast<Uint8*>(muted), sizeof(muted));
    Check(std::all_of(std::begin(muted), std::end(muted), [](int16_t n) { return n == 0; }),
          "Muted callback must output silence");
    std::cout << "PASS: Stage 1 score/ties/512-tick loop, 9 unchanged PCM baselines, 128 seeds each, duties/mute\n";
}
