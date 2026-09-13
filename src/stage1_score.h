#pragma once
#include <cmath>
#include <cstdint>

// One row is one quarter-note beat, not one sixteenth note.
// 32 bars x 4 beats = 128 rows. The player expands each row into four ticks.
constexpr int STAGE1_SCORE_STEPS = 128;
constexpr int STAGE1_TICKS_PER_BEAT = 4;
constexpr int STAGE1_FRAMES_PER_TICK = 5; // 180 BPM at 60 frames/second.

enum class Pulse2Mode : uint8_t { Echo, Harmony, Arpeggio };

struct NoteStep {
    float pulse1Freq;
    float pulse2Freq;
    float triFreq;
    uint8_t drumType;
    // An eighth-note response within this beat. -1 ties; 0 is an explicit rest.
    float pulse1OffbeatFreq;
    float pulse2OffbeatFreq;
    float triOffbeatFreq;
    uint8_t drumOffbeat;
    uint8_t drumSixteenth1;
    uint8_t drumSixteenth3;
    Pulse2Mode pulse2Mode = Pulse2Mode::Echo;
    float arpeggio[4] = {};
};

namespace Stage1Notes {
enum Pitch {
    H = -1, R = 0,
    E2 = 40, F2 = 41, G2 = 43, GS2 = 44, A2 = 45, B2 = 47,
    C3 = 48, D3 = 50, E3 = 52, F3 = 53, G3 = 55, GS3 = 56, A3 = 57, B3 = 59,
    C4 = 60, D4 = 62, E4 = 64, F4 = 65, G4 = 67, GS4 = 68, A4 = 69, B4 = 71,
    C5 = 72, D5 = 74, E5 = 76, F5 = 77, G5 = 79, GS5 = 80, A5 = 81, B5 = 83,
    C6 = 84, D6 = 86, E6 = 88, F6 = 89, G6 = 91, GS6 = 92
};
enum Drum { None = 0, Kick = 1, Snare = 2, Hat = 3, Fill = 4 };

inline float Frequency(int pitch) {
    return pitch <= 0 ? static_cast<float>(pitch) : 440.0f * std::pow(2.0f, (pitch - 69) / 12.0f);
}
inline NoteStep Q(int lead, int counter, int bass, uint8_t drum,
                  int leadAnd = H, int counterAnd = H, int bassAnd = H,
                  uint8_t drumAnd = Hat, uint8_t sixteenth1 = None, uint8_t sixteenth3 = None) {
    return {Frequency(lead), Frequency(counter), Frequency(bass), drum,
            Frequency(leadAnd), Frequency(counterAnd), Frequency(bassAnd),
            drumAnd, sixteenth1, sixteenth3};
}
inline NoteStep Harmonized(NoteStep beat) {
    beat.pulse2Mode = Pulse2Mode::Harmony;
    return beat;
}
inline NoteStep Arpeggiated(NoteStep beat, int root, int third, int fifth, int seventh) {
    beat.pulse2Mode = Pulse2Mode::Arpeggio;
    beat.arpeggio[0] = Frequency(root);
    beat.arpeggio[1] = Frequency(third);
    beat.arpeggio[2] = Frequency(fifth);
    beat.arpeggio[3] = Frequency(seventh);
    return beat;
}
} // namespace Stage1Notes

// Original fixed composition. Q defaults to the one-sixteenth echo macro.
// Harmonized rows contain explicit simultaneous harmony; Arpeggiated rows own P2.
namespace Stage1Composition {
using namespace Stage1Notes;
inline const NoteStep STAGE1_SCORE[STAGE1_SCORE_STEPS] = {

    // A: harmonic-minor question and answer / delayed echo
    // 01 Am: hook with a raised seventh and an anticipated A
    Q(A4,R,R,Kick,H,H,A2,Hat,None,None), Q(E5,R,R,Snare,D5,H,E3,Hat,None,None), Q(C5,R,R,Kick,GS4,H,A2,Hat,None,None), Q(A4,R,R,Snare,A5,H,E3,Hat,None,None),
    // 02 F -> E7: tie across the barline
    Q(H,R,R,Kick,H,H,F2,Hat,None,None), Q(F5,R,R,Snare,E5,H,C3,Hat,None,None), Q(E5,R,R,Kick,GS5,H,E2,Hat,None,None), Q(B4,R,R,Snare,F5,H,B2,Hat,None,None),
    // 03 Dm: answer begins off the previous bar
    Q(H,R,R,Kick,H,H,D3,Hat,None,None), Q(E5,R,R,Snare,D5,H,A2,Hat,None,None), Q(D5,R,R,Kick,H,H,D3,Hat,None,None), Q(E5,R,R,Snare,GS5,H,A2,Hat,None,None),
    // 04 E7: descending bass, early tonic pickup
    Q(H,R,E3,Kick,H,H,D3,Hat,None,None), Q(F5,R,C3,Snare,E5,H,B2,Hat,None,None), Q(D5,R,A2,Kick,B4,H,GS2,Hat,None,None), Q(GS4,R,F2,Snare,A4,H,E2,Hat,None,Fill),
    // 05 Am: answer inherits its first note by a tie
    Q(H,R,R,Kick,H,H,A2,Hat,None,None), Q(E5,R,R,Snare,D5,H,E3,Hat,None,None), Q(C5,R,R,Kick,B4,H,A2,Hat,None,None), Q(GS4,R,R,Snare,A4,H,E3,Hat,None,None),
    // 06 F / Dm: anticipation into the next bar
    Q(H,R,R,Kick,H,H,F2,Hat,None,None), Q(C5,R,R,Snare,E5,H,C3,Hat,None,None), Q(F5,R,R,Kick,E5,H,F2,Hat,None,None), Q(D5,R,R,Snare,F5,H,C3,Hat,None,None),
    // 07 Dm -> E7
    Q(H,R,R,Kick,E5,H,D3,Hat,None,None), Q(D5,R,R,Snare,F5,H,A2,Hat,None,None), Q(GS4,R,R,Kick,H,H,E2,Hat,None,None), Q(B4,R,R,Snare,H,H,B2,Hat,None,None),
    // 08 Am: three-beat tonic and a breath (echo trails one tick)
    Q(A4,R,E3,Kick,H,H,D3,None,None,None), Q(H,R,C3,None,H,H,B2,None,None,None), Q(H,R,A2,None,H,H,H,None,None,None), Q(R,R,R,None,H,H,H,None,None,None),

    // A prime: higher variation / grouped accents
    // 09 Am: high-register hook, 3+3+2 accents
    Q(A5,R,R,Kick,H,H,A2,Hat,None,None), Q(E6,R,R,None,D6,H,E3,Snare,None,None), Q(C6,R,R,None,GS5,H,A2,Hat,None,None), Q(A5,R,R,Snare,C6,H,E3,Hat,None,None),
    // 10 F -> E7
    Q(H,R,R,Kick,H,H,F2,Hat,None,None), Q(A5,R,R,None,GS5,H,C3,Snare,None,None), Q(F5,R,R,None,E5,H,E2,Hat,None,None), Q(GS5,R,R,Snare,F5,H,B2,Hat,None,None),
    // 11 Dm
    Q(H,R,R,Kick,H,H,D3,Hat,None,None), Q(A5,R,R,None,GS5,H,A2,Snare,None,None), Q(F5,R,R,None,E5,H,D3,Hat,None,None), Q(D5,R,R,Snare,GS5,H,A2,Hat,None,None),
    // 12 E7: one-frame chord macro and descending bass
    Arpeggiated(Q(H,R,E3,Kick,H,H,D3,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(B5,R,C3,None,D6,H,B2,Snare,None,None),E4,GS4,B4,D5), Arpeggiated(Q(E6,R,A2,None,D6,H,GS2,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(B5,R,F2,Snare,A5,H,E2,Hat,None,None),E4,GS4,B4,D5),
    // 13 Am: answering variation
    Q(H,R,R,Kick,H,H,A2,Hat,None,None), Q(E6,R,R,None,D6,H,E3,Snare,None,None), Q(C6,R,R,None,B5,H,A2,Hat,None,None), Q(GS5,R,R,Snare,A5,H,E3,Hat,None,None),
    // 14 F: raised-seventh tension
    Q(H,R,R,Kick,H,H,F2,Hat,None,None), Q(G5,R,R,None,F5,H,C3,Snare,None,None), Q(E5,R,R,None,F5,H,F2,Hat,None,None), Q(GS5,R,R,Snare,B5,H,C3,Hat,None,None),
    // 15 Dm -> E7
    Q(F5,R,R,Kick,E5,H,D3,Hat,None,None), Q(D5,R,R,None,F5,H,A2,Snare,None,None), Q(GS5,R,R,None,H,H,E2,Hat,None,None), Q(B5,R,R,Snare,H,H,B2,Hat,None,None),
    // 16 Am -> E7: turn toward F
    Q(A5,R,F3,Kick,H,H,E3,Hat,None,None), Q(H,R,D3,Snare,E5,H,C3,Hat,None,None), Q(GS5,R,B2,Kick,B5,H,A2,Hat,None,None), Q(E5,R,GS2,Snare,H,H,E2,Fill,Fill,Fill),

    // B: paired thirds / release and return to minor
    // 17 F -> E7: simultaneous upper thirds
    Harmonized(Q(F5,A5,R,Kick,A5,C6,F2,Hat,None,Hat)), Harmonized(Q(C6,E6,R,Snare,B5,D6,C3,Hat,None,None)), Harmonized(Q(A5,C6,R,Kick,F5,A5,F2,Hat,None,Hat)), Harmonized(Q(E5,GS5,R,Snare,GS5,B5,E2,Hat,None,None)),
    // 18 E7
    Harmonized(Q(B5,D6,R,Kick,A5,C6,E2,Hat,None,Hat)), Harmonized(Q(GS5,B5,R,Snare,E5,GS5,B2,Hat,None,None)), Harmonized(Q(B5,D6,R,Kick,D6,F6,E3,Hat,None,Hat)), Harmonized(Q(GS5,B5,R,Snare,B5,D6,B2,Hat,None,None)),
    // 19 Am
    Harmonized(Q(C6,E6,R,Kick,B5,D6,A2,Hat,None,Hat)), Harmonized(Q(A5,C6,R,Snare,GS5,B5,E3,Hat,None,None)), Harmonized(Q(E5,GS5,R,Kick,GS5,B5,A2,Hat,None,Hat)), Harmonized(Q(A5,C6,R,Snare,B5,D6,E3,Hat,None,None)),
    // 20 Dm -> E7: descending bass
    Harmonized(Q(F5,A5,E3,Kick,A5,C6,D3,Hat,None,Hat)), Harmonized(Q(D6,F6,C3,Snare,C6,E6,B2,Hat,None,None)), Harmonized(Q(B5,D6,A2,Kick,A5,C6,GS2,Hat,None,Hat)), Harmonized(Q(GS5,B5,F2,Snare,E5,GS5,E2,Hat,None,None)),
    // 21 C: brief relative-major summit
    Harmonized(Q(E5,G5,R,Kick,G5,B5,C3,Hat,None,Hat)), Harmonized(Q(C6,E6,R,Snare,D6,F6,G2,Hat,None,None)), Harmonized(Q(E6,G6,R,Kick,D6,F6,C3,Hat,None,Hat)), Harmonized(Q(C6,E6,R,Snare,G5,B5,G2,Hat,None,None)),
    // 22 F: harmonic-minor inflection
    Harmonized(Q(A5,C6,R,Kick,G5,B5,F2,Hat,None,Hat)), Harmonized(Q(F5,A5,R,Snare,E5,GS5,C3,Hat,None,None)), Harmonized(Q(F5,A5,R,Kick,GS5,B5,F2,Hat,None,Hat)), Harmonized(Q(A5,C6,R,Snare,C6,E6,C3,Hat,None,None)),
    // 23 Dm -> E7
    Harmonized(Q(A5,C6,R,Kick,F5,A5,D3,Hat,None,Hat)), Harmonized(Q(D5,F5,R,Snare,F5,A5,A2,Hat,None,None)), Harmonized(Q(B5,D6,R,Kick,D6,F6,E2,Hat,None,Hat)), Harmonized(Q(GS5,B5,R,Snare,B5,D6,B2,Hat,None,Fill)),
    // 24 Am: chorus resolution
    Harmonized(Q(A5,C6,E3,Kick,H,H,D3,Hat,None,None)), Harmonized(Q(H,H,C3,Snare,H,H,B2,Hat,None,None)), Harmonized(Q(E5,GS5,A2,Kick,C5,E5,H,Hat,None,None)), Harmonized(Q(A4,C5,H,None,H,H,H,None,None,None)),

    // Connection: echo and E7 arpeggios / loop pickup
    // 25 Am: offbeat bass and anticipations
    Q(A4,R,R,Kick,H,H,A3,Hat,None,None), Q(C5,R,R,Snare,E5,H,E3,Hat,None,None), Q(GS5,R,R,Kick,H,H,A3,Hat,None,None), Q(E5,R,R,Snare,F5,H,E3,Hat,None,None),
    // 26 Dm
    Q(H,R,R,Kick,E5,H,A3,Hat,None,None), Q(D5,R,R,Snare,H,H,A3,Hat,None,None), Q(F5,R,R,Kick,E5,H,A3,Hat,None,None), Q(D5,R,R,Snare,A5,H,A3,Hat,None,None),
    // 27 F -> E7
    Q(H,R,R,Kick,GS5,H,F3,Hat,None,None), Q(F5,R,R,Snare,H,H,F3,Hat,None,None), Q(E5,R,R,Kick,GS5,H,F3,Hat,None,None), Q(B4,R,R,Snare,GS5,H,F3,Hat,None,None),
    // 28 E7: fast chord macro
    Arpeggiated(Q(GS5,R,E3,Kick,H,H,D3,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(E5,R,C3,Snare,H,H,B2,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(D5,R,A2,Kick,H,H,GS2,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(B4,R,F2,Snare,H,H,E2,Fill,None,Fill),E4,GS4,B4,D5),
    // 29 Am
    Q(C5,R,R,Kick,E5,H,A3,Hat,None,None), Q(GS5,R,R,Snare,A5,H,E3,Hat,None,None), Q(H,R,R,Kick,E5,H,A3,Hat,None,None), Q(C5,R,R,Snare,F5,H,E3,Hat,None,None),
    // 30 F -> Dm
    Q(H,R,R,Kick,E5,H,F3,Hat,None,None), Q(D5,R,R,Snare,H,H,F3,Hat,None,None), Q(F5,R,R,Kick,E5,H,A3,Hat,None,None), Q(D5,R,R,Snare,B4,H,A3,Hat,None,None),
    // 31 E7: chord macro and drum build
    Arpeggiated(Q(B4,R,R,Kick,H,H,E3,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(D5,R,R,Snare,H,H,E3,Hat,None,Fill),E4,GS4,B4,D5), Arpeggiated(Q(E5,R,R,Kick,H,H,E3,Hat,None,None),E4,GS4,B4,D5), Arpeggiated(Q(GS5,R,R,Snare,H,H,E3,Fill,Fill,Fill),E4,GS4,B4,D5),
    // 32 E7: descending bass, G sharp pickup into A
    Arpeggiated(Q(B5,R,E3,Kick,H,H,D3,Hat,None,Fill),E4,GS4,B4,D5), Arpeggiated(Q(GS5,R,C3,Snare,H,H,B2,Fill,None,Fill),E4,GS4,B4,D5), Arpeggiated(Q(E5,R,A2,Fill,GS5,H,GS2,Fill,Fill,Fill),E4,GS4,B4,D5), Arpeggiated(Q(B4,R,F2,Fill,GS4,H,E2,Fill,Fill,Fill),E4,GS4,B4,D5),
};
} // namespace Stage1Composition
using Stage1Composition::STAGE1_SCORE;
