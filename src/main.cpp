/*
 * CPS (CardPuter Synth) - v11
 * -------------------------------------------------------
 * A DIY synthesizer app for the M5Stack CardputerADV.
 *
 * Features:
 *   - Number keys "1234567890" mapped to musical notes (C4 and up)
 *   - Monophonic: last key pressed wins
 *   - '=' / '-' keys: octave shift (-2 to +2)
 *   - ',' / '.' keys: volume control (0-100%, 5% steps)
 *   - 'Z' key: bend down  /  'X' key: bend up
 *     Guitar-chording style: slow pitch rise on press,
 *     fast return on release (asymmetric attack/release).
 *   - ADSR envelope: Attack / Decay / Sustain / Release
 *     Retrigger-capable while in Release phase (monophonic).
 *   - Biquad filter (2nd-order IIR):
 *     LPF / HPF / BPF / Notch, configurable cutoff & Q.
 *     Signal path: oscillator -> bitcrusher -> filter -> ADSR
 *   - IMU (BMI270) tilt-to-parameter mapping (customizable):
 *       X axis default: timbre morphing (Sine->Tri->Saw->Square)
 *       Y axis default: vibrato depth
 *     Available targets:
 *       NONE / TIMBRE / VIBRATO_DEPTH / VIBRATO_RATE / TREMOLO /
 *       VOLUME / PITCH_BEND / BEND_UP / BEND_DOWN /
 *       BITCRUSH / FILTER_CUTOFF
 *     Switching targets auto-resets the previous parameter.
 *   - Tab key cycles: MAIN -> EDIT -> SETTING -> MAIN
 *       EDIT screen : ADSR graph + ADSR & filter settings
 *       SETTING screen: IMU axis mapping, bend width/speed
 *   - Auto-save / auto-load via SD card (/CPS/settings.json)
 *   - /CPS folder created automatically on first boot
 *
 * Required library: M5Cardputer (uses M5Unified / M5GFX internally)
 */

#include "M5Cardputer.h"
#include <math.h>
#include <SPI.h>
#include <SD.h>

// ---------------------------------------------------------
// SD card pin configuration (SPI, CardputerADV)
// ---------------------------------------------------------
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
static const char *CPS_FOLDER_PATH = "/CPS";

// ---------------------------------------------------------
// Audio settings
// ---------------------------------------------------------
static constexpr int SAMPLE_RATE     = 44100;
static constexpr int WAVE_TABLE_SIZE = 256;

int16_t sineTable[WAVE_TABLE_SIZE];
int16_t triangleTable[WAVE_TABLE_SIZE];
int16_t sawtoothTable[WAVE_TABLE_SIZE];
int16_t squareTable[WAVE_TABLE_SIZE];

// Per-waveform amplitude constants for loudness normalization
// (square/sawtooth have more harmonics and sound louder at the same peak)
constexpr float SINE_AMP     = 32000.0f;
constexpr float TRIANGLE_AMP = 30000.0f;
constexpr float SAWTOOTH_AMP = 22000.0f;
constexpr float SQUARE_AMP   = 18000.0f;

// ---------------------------------------------------------
// Note-key table
// 1=C4, 2=D4, 3=E4, 4=F4, 5=G4, 6=A4, 7=B4, 8=C5, 9=D5, 0=E5
// ---------------------------------------------------------
struct KeyNote { char key; float freq; };
KeyNote keyNotes[] = {
    {'1', 261.63f}, {'2', 293.66f}, {'3', 329.63f},
    {'4', 349.23f}, {'5', 392.00f}, {'6', 440.00f},
    {'7', 493.88f}, {'8', 523.25f}, {'9', 587.33f}, {'0', 659.25f},
};
const int NUM_KEYS = sizeof(keyNotes) / sizeof(keyNotes[0]);

// ---------------------------------------------------------
// IMU mapping
// ---------------------------------------------------------
// Each IMU axis can be assigned to one of these parameters.
// To add a new target: add an enum value here, then add
// cases to applyImuValue(), imuTargetName(), resetParamToDefault(),
// getImuParamNormalized(), and getImuParamValueStr().
enum class ImuTarget : uint8_t {
    NONE,           // Disabled - axis has no effect
    TIMBRE,         // Waveform morph (0.0=Sine .. 3.0=Square)
    VIBRATO_DEPTH,  // Vibrato depth (0.0 - 1.0)
    VIBRATO_RATE,   // Vibrato rate  (1 - 10 Hz)
    TREMOLO,        // Tremolo depth (0.0 - 1.0)
    VOLUME,         // Volume offset (-0.5 .. +0.5, added to key volume)
    PITCH_BEND,     // Pitch bend bipolar: tilt direction = bend direction
    BEND_UP,        // Bend up only: tilt amount = pitch raise (absolute)
    BEND_DOWN,      // Bend down only: tilt amount = pitch drop (absolute)
    BITCRUSH,       // Bit-crusher amount (0.0=clean .. 1.0=max crush)
    FILTER_CUTOFF,  // Filter cutoff frequency (100 - 8000 Hz)
    TARGET_COUNT    // Sentinel - not an actual target
};

// Forward declaration: defined later in the file, but called by
// imuXLabel()/imuYLabel() which appear before the definition.
const char *imuTargetName(ImuTarget t);

// Per-axis configuration
struct ImuAxisConfig {
    ImuTarget target;   // Which parameter this axis controls
    float sensitivity;  // Scale factor (1.0 = standard)
    bool bipolar;       // false = absolute value 0..1 (same effect both directions)
                        // true  = signed -1..+1 (direction matters)
                        // PITCH_BEND and VOLUME naturally want bipolar=true
};

// Default mapping (overridden by /CPS/settings.json if present)
ImuAxisConfig imuAxisX = { ImuTarget::TIMBRE,        1.0f, false };
ImuAxisConfig imuAxisY = { ImuTarget::VIBRATO_DEPTH,  1.0f, false };

// Tilt angle (degrees) that maps to maximum effect (1.0)
constexpr float TILT_MAX_DEGREES = 35.0f;

// ---------------------------------------------------------
// Synth parameters
// ---------------------------------------------------------
// "current" values are read by audioTask each sample.
// "target" values are written by loop() (IMU / key events).
// audioTask smoothly interpolates current -> target via
// exponential moving average, giving glitch-free transitions.
// Key-operated params (keyVolume, octaveShift) are immediate.
struct SynthParams {
    // --- Key-controlled ---
    float keyVolume   = 0.5f;  // Changed by ',' / '.' (0.0 - 1.0)
    int   octaveShift = 0;     // Changed by '=' / '-' (-2 to +2)

    // --- IMU-controlled: current values ---
    float timbreMorph        = 0.0f;  // Waveform morph    (0.0 - 3.0)
    float vibratoDepth       = 0.0f;  // Vibrato depth     (0.0 - 1.0)
    float vibratoRateHz      = 5.0f;  // Vibrato rate      (Hz)
    float tremoloDepth       = 0.0f;  // Tremolo depth     (0.0 - 1.0)
    float volumeOffset       = 0.0f;  // Volume offset     (-0.5 .. +0.5)
    float pitchBendCents     = 0.0f;  // Pitch bend        (cents)
    float bitcrush           = 0.0f;  // Bit-crush amount  (0.0 - 1.0)
    float filterCutoffOffset = 0.0f;  // IMU cutoff offset (0.0 - 1.0)

    // --- IMU-controlled: target values ---
    float timbreMorphTarget        = 0.0f;
    float vibratoDepthTarget       = 0.0f;
    float vibratoRateHzTarget      = 5.0f;
    float tremoloDepthTarget       = 0.0f;
    float volumeOffsetTarget       = 0.0f;
    float pitchBendCentsTarget     = 0.0f;
    float bitcrushTarget           = 0.0f;
    float filterCutoffOffsetTarget = 0.0f;
} params;

float currentFreq = 0.0f;  // 0.0 = silence

// Edge-detection flags for key-operated parameters
bool prevOctaveUpPressed   = false;
bool prevOctaveDownPressed = false;
bool prevVolumeUpPressed   = false;
bool prevVolumeDownPressed = false;

// Phase accumulators for oscillator and LFOs
double phase        = 0.0;
double vibratoPhase = 0.0;
double tremoloPhase = 0.0;

constexpr float VIBRATO_MAX_CENTS = 35.0f;  // Max vibrato pitch deviation (cents)

// ---------------------------------------------------------
// Key bend (Z / X keys)
// ---------------------------------------------------------
// keyBendGoal    : target set by key handler (0 or ±max)
// keyBendCurrent : value actually added to pitch each sample
//
// Attack and release use different smoothing coefficients to
// achieve a guitar-choke feel:
//   Attack  (press)   : slow rise  -> "choke" effect
//   Release (let go)  : fast snap  -> crisp release
//
// Coefficient meaning: each sample, current moves toward goal
// by (goal - current) * coeff. Larger = faster response.
//   ATTACK  = 0.0003 -> ~1.5 s to reach full bend
//   RELEASE = 0.003  -> ~0.15 s to return to zero
float keyBendMaxCents   = 200.0f;   // Max bend width (cents). 100c = 1 semitone
float keyBendGoal       = 0.0f;
float keyBendCurrent    = 0.0f;
constexpr float KEY_BEND_ATTACK_SMOOTH_DEFAULT  = 0.0003f;
constexpr float KEY_BEND_RELEASE_SMOOTH_DEFAULT = 0.003f;
float keyBendAttackSmooth  = KEY_BEND_ATTACK_SMOOTH_DEFAULT;
float keyBendReleaseSmooth = KEY_BEND_RELEASE_SMOOTH_DEFAULT;

// ---------------------------------------------------------
// ADSR envelope
// ---------------------------------------------------------
// Four-stage volume envelope: Attack / Decay / Sustain / Release
// Currently implemented as linear segments.
//
// Phase flow:
//   IDLE -> [key press] -> ATTACK -> DECAY -> SUSTAIN
//        -> [key release] -> RELEASE -> IDLE
//
// envLevel  : current envelope output (0.0 - 1.0), multiplied into audio
// envPhase  : current stage
// playingFreq: frequency the oscillator is actually producing.
//   Separate from currentFreq so "key released but still in RELEASE"
//   keeps the correct pitch until the note fades to silence.
enum class EnvPhase : uint8_t {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE,
};

struct AdsrParams {
    float attackTime   = 0.05f; // seconds (0 = immediate attack, percussive)
    float decayTime    = 0.15f; // seconds
    float sustainLevel = 0.7f;  // 0.0 - 1.0
    float releaseTime  = 0.3f;  // seconds
} adsr;

constexpr float ADSR_MIN_TIME = 0.0f;
constexpr float ADSR_MAX_TIME = 5.0f;

// ---------------------------------------------------------
// Biquad filter (2nd-order IIR)
// ---------------------------------------------------------
// LPF / HPF / BPF / Notch implemented by switching coefficients
// using the standard Audio EQ Cookbook formulas (R. Bristow-Johnson).
//
// Difference equation:
//   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2]
//         - a1*y[n-1] - a2*y[n-2]
// (coefficients stored pre-normalized by a0)
enum class FilterType : uint8_t {
    LPF,    // Low-pass  : attenuates above cutoff (warm/muffled)
    HPF,    // High-pass : attenuates below cutoff (thin/bright)
    BPF,    // Band-pass : passes only near cutoff (telephone/nasal)
    NOTCH,  // Band-reject: cuts near cutoff (distinctive hollow sound)
};

struct FilterParams {
    FilterType type      = FilterType::LPF;
    float cutoffHz       = 2000.0f; // Cutoff frequency (Hz)
    float resonanceQ     = 0.707f;  // Q factor (0.707 = Butterworth, maximally flat)
} filterParams;

constexpr float FILTER_CUTOFF_MIN = 100.0f;
constexpr float FILTER_CUTOFF_MAX = 8000.0f;
constexpr float FILTER_Q_MIN      = 0.5f;
constexpr float FILTER_Q_MAX      = 10.0f;

// Filter coefficients (recalculated by updateFilterCoefficients())
float filterB0 = 1.0f, filterB1 = 0.0f, filterB2 = 0.0f;
float filterA1 = 0.0f, filterA2 = 0.0f;

// Filter delay-line state (2 samples of input and output)
float filterX1 = 0.0f, filterX2 = 0.0f;
float filterY1 = 0.0f, filterY2 = 0.0f;

// ---------------------------------------------------------
// Compute Biquad coefficients from current filterParams.
// Must be called after any change to type, cutoffHz, or resonanceQ.
// ---------------------------------------------------------
void updateFilterCoefficients() {
    float cutoff = filterParams.cutoffHz;
    cutoff = constrain(cutoff, FILTER_CUTOFF_MIN, SAMPLE_RATE * 0.45f);

    float Q = constrain(filterParams.resonanceQ, FILTER_Q_MIN, FILTER_Q_MAX);

    float omega = 2.0f * PI * cutoff / SAMPLE_RATE;
    float sinW  = sinf(omega);
    float cosW  = cosf(omega);
    float alpha = sinW / (2.0f * Q);

    float b0, b1, b2, a0, a1, a2;

    switch (filterParams.type) {
        case FilterType::LPF:
            b0 = (1.0f - cosW) / 2.0f;
            b1 =  1.0f - cosW;
            b2 = (1.0f - cosW) / 2.0f;
            a0 =  1.0f + alpha; a1 = -2.0f * cosW; a2 = 1.0f - alpha;
            break;
        case FilterType::HPF:
            b0 =  (1.0f + cosW) / 2.0f;
            b1 = -(1.0f + cosW);
            b2 =  (1.0f + cosW) / 2.0f;
            a0 =  1.0f + alpha; a1 = -2.0f * cosW; a2 = 1.0f - alpha;
            break;
        case FilterType::BPF:  // constant 0 dB peak gain
            b0 =  alpha; b1 = 0.0f; b2 = -alpha;
            a0 =  1.0f + alpha; a1 = -2.0f * cosW; a2 = 1.0f - alpha;
            break;
        case FilterType::NOTCH:
            b0 =  1.0f; b1 = -2.0f * cosW; b2 = 1.0f;
            a0 =  1.0f + alpha; a1 = -2.0f * cosW; a2 = 1.0f - alpha;
            break;
        default:
            b0 = 1.0f; b1 = b2 = 0.0f;
            a0 = 1.0f; a1 = a2 = 0.0f;
            break;
    }

    filterB0 = b0 / a0;  filterB1 = b1 / a0;  filterB2 = b2 / a0;
    filterA1 = a1 / a0;  filterA2 = a2 / a0;
}

// Apply one sample through the Biquad filter
int16_t applyFilter(int16_t inSample) {
    float x0 = (float)inSample;
    float y0 = filterB0*x0 + filterB1*filterX1 + filterB2*filterX2
             - filterA1*filterY1 - filterA2*filterY2;
    filterX2 = filterX1;  filterX1 = x0;
    filterY2 = filterY1;  filterY1 = y0;
    y0 = constrain(y0, -32768.0f, 32767.0f);  // clip guard against resonance overdrive
    return (int16_t)y0;
}

// ---------------------------------------------------------
// App mode (screen selection)
// ---------------------------------------------------------
// Tab key cycles: PLAY -> EDIT -> SETTINGS -> PLAY
// Future: additional screens (e.g. PRESET) can be inserted here.
enum class AppMode : uint8_t { PLAY, EDIT, SETTINGS };
AppMode appMode = AppMode::PLAY;
bool prevTabPressed = false;

// ---------------------------------------------------------
// Setting menu item definition
// ---------------------------------------------------------
// Each item has a display name and three function pointers:
//   onIncrement / onDecrement : change the value
//   valueLabel                : return current value as a string
// Adding a new item = append one line to the array.
struct SettingItem {
    const char *name;
    void (*onIncrement)();
    void (*onDecrement)();
    const char *(*valueLabel)();
};

// Forward declaration needed before SettingItem arrays
bool saveSettings();

// ---- IMU axis X target ----
void imuXNext() {
    resetParamToDefault(imuAxisX.target);   // forward-declared below
    uint8_t v = (uint8_t)imuAxisX.target;
    imuAxisX.target = (ImuTarget)((v + 1) % (uint8_t)ImuTarget::TARGET_COUNT);
}
void imuXPrev() {
    resetParamToDefault(imuAxisX.target);
    uint8_t v = (uint8_t)imuAxisX.target;
    imuAxisX.target = (ImuTarget)(v == 0 ? (uint8_t)ImuTarget::TARGET_COUNT - 1 : v - 1);
}
const char *imuXLabel() { return imuTargetName(imuAxisX.target); }

// ---- IMU axis Y target ----
void imuYNext() {
    resetParamToDefault(imuAxisY.target);
    uint8_t v = (uint8_t)imuAxisY.target;
    imuAxisY.target = (ImuTarget)((v + 1) % (uint8_t)ImuTarget::TARGET_COUNT);
}
void imuYPrev() {
    resetParamToDefault(imuAxisY.target);
    uint8_t v = (uint8_t)imuAxisY.target;
    imuAxisY.target = (ImuTarget)(v == 0 ? (uint8_t)ImuTarget::TARGET_COUNT - 1 : v - 1);
}
const char *imuYLabel() { return imuTargetName(imuAxisY.target); }

// ---- Bend width (semitone steps) ----
void bendWidthInc() { keyBendMaxCents = min(keyBendMaxCents + 100.0f, 1200.0f); }
void bendWidthDec() { keyBendMaxCents = max(keyBendMaxCents - 100.0f,    0.0f); }
char bendWidthLabelBuf[16];
const char *bendWidthLabel() {
    snprintf(bendWidthLabelBuf, sizeof(bendWidthLabelBuf), "%.1f st", keyBendMaxCents / 100.0f);
    return bendWidthLabelBuf;
}

// ---- Bend attack speed ----
void bendAttackInc() { keyBendAttackSmooth = min(keyBendAttackSmooth * 1.3f, 0.01f); }
void bendAttackDec() { keyBendAttackSmooth = max(keyBendAttackSmooth / 1.3f, 0.00005f); }
char bendAttackLabelBuf[16];
const char *bendAttackLabel() {
    snprintf(bendAttackLabelBuf, sizeof(bendAttackLabelBuf), "%.4f", keyBendAttackSmooth);
    return bendAttackLabelBuf;
}

// ---- Bend release speed ----
void bendReleaseInc() { keyBendReleaseSmooth = min(keyBendReleaseSmooth * 1.3f, 0.02f); }
void bendReleaseDec() { keyBendReleaseSmooth = max(keyBendReleaseSmooth / 1.3f, 0.0005f); }
char bendReleaseLabelBuf[16];
const char *bendReleaseLabel() {
    snprintf(bendReleaseLabelBuf, sizeof(bendReleaseLabelBuf), "%.4f", keyBendReleaseSmooth);
    return bendReleaseLabelBuf;
}

// SETTINGS menu item list
SettingItem settingItems[] = {
    { "IMU X axis",   imuXNext,       imuXPrev,       imuXLabel },
    { "IMU Y axis",   imuYNext,       imuYPrev,       imuYLabel },
    { "Bend width",   bendWidthInc,   bendWidthDec,   bendWidthLabel },
    { "Bend attack",  bendAttackInc,  bendAttackDec,  bendAttackLabel },
    { "Bend release", bendReleaseInc, bendReleaseDec, bendReleaseLabel },
};
const int NUM_SETTING_ITEMS = sizeof(settingItems) / sizeof(settingItems[0]);
int selectedSettingIndex = 0;

// Edge-detection flags for menu navigation
bool prevMenuUpPressed   = false;
bool prevMenuDownPressed = false;
bool prevMenuIncPressed  = false;
bool prevMenuDecPressed  = false;

// ---------------------------------------------------------
// resetParamToDefault
// ---------------------------------------------------------
// Resets both the current value and the target value of the
// parameter associated with the given ImuTarget to its
// "no-tilt" default. Called when switching axis assignments
// so the old parameter doesn't stay stuck at its last value.
void resetParamToDefault(ImuTarget t) {
    switch (t) {
        case ImuTarget::TIMBRE:
            params.timbreMorph = params.timbreMorphTarget = 0.0f; break;
        case ImuTarget::VIBRATO_DEPTH:
            params.vibratoDepth = params.vibratoDepthTarget = 0.0f; break;
        case ImuTarget::VIBRATO_RATE:
            params.vibratoRateHz = params.vibratoRateHzTarget = 5.0f; break;
        case ImuTarget::TREMOLO:
            params.tremoloDepth = params.tremoloDepthTarget = 0.0f; break;
        case ImuTarget::VOLUME:
            params.volumeOffset = params.volumeOffsetTarget = 0.0f; break;
        case ImuTarget::PITCH_BEND:
        case ImuTarget::BEND_UP:
        case ImuTarget::BEND_DOWN:
            params.pitchBendCents = params.pitchBendCentsTarget = 0.0f; break;
        case ImuTarget::BITCRUSH:
            params.bitcrush = params.bitcrushTarget = 0.0f; break;
        case ImuTarget::FILTER_CUTOFF:
            params.filterCutoffOffset = params.filterCutoffOffsetTarget = 0.0f; break;
        default: break;
    }
}

// ---------------------------------------------------------
// Wavetable initialization
// ---------------------------------------------------------
void buildWaveTables() {
    for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
        float t   = (float)i / WAVE_TABLE_SIZE;
        float rad = 2.0f * PI * t;
        sineTable[i]     = (int16_t)(sinf(rad) * SINE_AMP);
        float tri = (t < 0.5f) ? (4.0f*t - 1.0f) : (3.0f - 4.0f*t);
        triangleTable[i] = (int16_t)(tri * TRIANGLE_AMP);
        sawtoothTable[i] = (int16_t)((2.0f*t - 1.0f) * SAWTOOTH_AMP);
        squareTable[i]   = (int16_t)(((t < 0.5f) ? 1.0f : -1.0f) * SQUARE_AMP);
    }
}

// Linear interpolation between adjacent wavetables based on morph (0.0 - 3.0)
int16_t getMorphedSample(int idx, float morph) {
    morph = constrain(morph, 0.0f, 3.0f);
    int16_t *tables[4] = {sineTable, triangleTable, sawtoothTable, squareTable};
    int lo = constrain((int)morph, 0, 2);
    float frac = morph - (float)lo;
    return (int16_t)(tables[lo][idx] * (1.0f - frac) + tables[lo+1][idx] * frac);
}

// ---------------------------------------------------------
// Bit-crusher effect
// ---------------------------------------------------------
// crush=0.0 : full 16-bit resolution (clean)
// crush=1.0 : ~3-bit resolution (retro/Lo-Fi)
int16_t applyBitcrush(int16_t sample, float crush) {
    if (crush <= 0.0f) return sample;
    float bits   = 16.0f - crush * 13.0f;   // 16 down to 3 bits
    float levels = powf(2.0f, bits);
    float norm   = sample / 32768.0f;
    return (int16_t)(roundf(norm * levels) / levels * 32768.0f);
}

// ---------------------------------------------------------
// ADSR envelope advancement (called once per audio buffer)
// ---------------------------------------------------------
// keyHeld : true while any note key is pressed
//
// The oscillator frequency (playingFreq) is updated here so
// that pitch stays correct during the Release tail even after
// the key is released.
float envLevel   = 0.0f;
EnvPhase envPhase = EnvPhase::IDLE;
float playingFreq = 0.0f;

void advanceEnvelope(bool keyHeld) {
    const float dt = (float)1024 / SAMPLE_RATE;  // time per buffer (seconds)

    switch (envPhase) {
        case EnvPhase::IDLE:
            envLevel = 0.0f;
            playingFreq = 0.0f;
            break;

        case EnvPhase::ATTACK:
            playingFreq = currentFreq;
            if (adsr.attackTime <= 0.0f) {
                envLevel  = 1.0f;
                envPhase  = EnvPhase::DECAY;
            } else {
                envLevel += dt / adsr.attackTime;
                if (envLevel >= 1.0f) { envLevel = 1.0f; envPhase = EnvPhase::DECAY; }
            }
            if (!keyHeld) envPhase = EnvPhase::RELEASE;
            break;

        case EnvPhase::DECAY:
            if (adsr.decayTime <= 0.0f) {
                envLevel = adsr.sustainLevel;
                envPhase = EnvPhase::SUSTAIN;
            } else {
                envLevel -= dt / adsr.decayTime * (1.0f - adsr.sustainLevel);
                if (envLevel <= adsr.sustainLevel) {
                    envLevel = adsr.sustainLevel;
                    envPhase = EnvPhase::SUSTAIN;
                }
            }
            if (!keyHeld) envPhase = EnvPhase::RELEASE;
            break;

        case EnvPhase::SUSTAIN:
            envLevel = adsr.sustainLevel;
            if (!keyHeld) envPhase = EnvPhase::RELEASE;
            break;

        case EnvPhase::RELEASE:
            if (adsr.releaseTime <= 0.0f) {
                envLevel  = 0.0f;
                envPhase  = EnvPhase::IDLE;
                playingFreq = 0.0f;
            } else {
                envLevel -= dt / adsr.releaseTime * adsr.sustainLevel;
                if (envLevel <= 0.0f) {
                    envLevel  = 0.0f;
                    envPhase  = EnvPhase::IDLE;
                    playingFreq = 0.0f;
                }
            }
            // Retrigger: new key pressed during Release -> restart from current level
            if (keyHeld && currentFreq > 0.0f) {
                envPhase = EnvPhase::ATTACK;
                playingFreq = currentFreq;
            }
            break;
    }
}

// ---------------------------------------------------------
// Audio task (runs on Core 1, priority 5)
// ---------------------------------------------------------
// Generates PCM audio and feeds it to the speaker via playRaw().
// Uses triple-buffering to minimise click noise at buffer boundaries.
// All IMU-driven parameters are smoothed per-sample (exponential
// moving average) so parameter changes cause no audio glitches.
void audioTask(void *pvParameters) {
    const int bufferSamples = 1024;
    static int16_t bufs[3][bufferSamples];
    int bufIndex = 0;
    constexpr int CH = 0;
    constexpr float SM = 0.0008f;  // Smoothing coefficient for IMU params

    double vibPhaseInc = (double)WAVE_TABLE_SIZE * 5.0f / SAMPLE_RATE;

    while (true) {
        int16_t *buf = bufs[bufIndex];
        bufIndex = (bufIndex + 1) % 3;

        bool keyHeld = (currentFreq > 0.0f);
        advanceEnvelope(keyHeld);

        if (envPhase == EnvPhase::IDLE) {
            vTaskDelay(5 / portTICK_PERIOD_MS);
            phase = 0.0;
            continue;
        }

        for (int i = 0; i < bufferSamples; i++) {
            // Smooth all IMU-driven parameters toward their targets
            params.timbreMorph        += (params.timbreMorphTarget        - params.timbreMorph)        * SM;
            params.vibratoDepth       += (params.vibratoDepthTarget       - params.vibratoDepth)       * SM;
            params.vibratoRateHz      += (params.vibratoRateHzTarget      - params.vibratoRateHz)      * SM;
            params.tremoloDepth       += (params.tremoloDepthTarget       - params.tremoloDepth)       * SM;
            params.volumeOffset       += (params.volumeOffsetTarget       - params.volumeOffset)       * SM;
            params.pitchBendCents     += (params.pitchBendCentsTarget     - params.pitchBendCents)     * SM;
            params.bitcrush           += (params.bitcrushTarget           - params.bitcrush)           * SM;
            params.filterCutoffOffset += (params.filterCutoffOffsetTarget - params.filterCutoffOffset) * SM;

            // Vibrato LFO
            int vibIdx = ((int)vibratoPhase) % WAVE_TABLE_SIZE;
            float vibratoLfo = sineTable[vibIdx < 0 ? vibIdx + WAVE_TABLE_SIZE : vibIdx] / 32000.0f;
            vibPhaseInc = (double)WAVE_TABLE_SIZE * params.vibratoRateHz / SAMPLE_RATE;
            vibratoPhase += vibPhaseInc;
            if (vibratoPhase >= WAVE_TABLE_SIZE) vibratoPhase -= WAVE_TABLE_SIZE;

            // Tremolo LFO
            int tremIdx = ((int)tremoloPhase) % WAVE_TABLE_SIZE;
            float tremoloLfo = (sineTable[tremIdx < 0 ? tremIdx + WAVE_TABLE_SIZE : tremIdx] / 32000.0f + 1.0f) * 0.5f;
            tremoloPhase += (double)WAVE_TABLE_SIZE * 5.0f / SAMPLE_RATE;
            if (tremoloPhase >= WAVE_TABLE_SIZE) tremoloPhase -= WAVE_TABLE_SIZE;

            // Key-bend smoothing (asymmetric attack/release)
            float bendDiff   = keyBendGoal - keyBendCurrent;
            float bendSmooth = (fabsf(keyBendGoal) < fabsf(keyBendCurrent) || keyBendGoal == 0.0f)
                               ? keyBendReleaseSmooth : keyBendAttackSmooth;
            keyBendCurrent += bendDiff * bendSmooth;

            // Total pitch offset: vibrato + IMU bend + key bend
            float totalCents = vibratoLfo * params.vibratoDepth * VIBRATO_MAX_CENTS
                             + params.pitchBendCents
                             + keyBendCurrent;
            float pitchRatio  = powf(2.0f, totalCents / 1200.0f);
            double phaseInc   = (double)WAVE_TABLE_SIZE * (playingFreq * pitchRatio) / SAMPLE_RATE;

            // Oscillator
            int idx = ((int)phase) % WAVE_TABLE_SIZE;
            if (idx < 0) idx += WAVE_TABLE_SIZE;
            int16_t sample = getMorphedSample(idx, params.timbreMorph);
            phase += phaseInc;
            if (phase >= WAVE_TABLE_SIZE) phase -= WAVE_TABLE_SIZE;

            // Bit-crusher
            sample = applyBitcrush(sample, params.bitcrush);

            // Biquad filter (applied before ADSR: filter -> amp)
            if (params.filterCutoffOffset > 0.0001f) {
                // IMU-driven dynamic cutoff: tilt = lower cutoff (more muffled)
                float dynCutoff = filterParams.cutoffHz * (1.0f - params.filterCutoffOffset * 0.9f);
                float saved     = filterParams.cutoffHz;
                filterParams.cutoffHz = constrain(dynCutoff, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX);
                updateFilterCoefficients();
                filterParams.cutoffHz = saved;  // don't alter the EDIT-screen display value
            }
            sample = applyFilter(sample);

            // Volume: key volume + IMU offset + tremolo + ADSR
            float totalVolume = constrain(params.keyVolume + params.volumeOffset, 0.0f, 1.0f);
            float tremoloGain = constrain(
                1.0f - params.tremoloDepth + params.tremoloDepth * tremoloLfo * 2.0f,
                0.0f, 2.0f);
            buf[i] = (int16_t)(sample * totalVolume * tremoloGain * envLevel);
        }

        M5Cardputer.Speaker.playRaw(buf, bufferSamples, SAMPLE_RATE, false, 1, CH, false);
    }
}

// ---------------------------------------------------------
// Note key -> frequency resolution (monophonic)
// ---------------------------------------------------------
float resolveFreqFromKeys() {
    auto status = M5Cardputer.Keyboard.keysState();
    for (auto it = status.word.rbegin(); it != status.word.rend(); ++it) {
        for (int i = 0; i < NUM_KEYS; i++) {
            if (keyNotes[i].key == *it)
                return keyNotes[i].freq * powf(2.0f, (float)params.octaveShift);
        }
    }
    return 0.0f;
}

// ---------------------------------------------------------
// Key handler: octave / volume / bend (PLAY mode only)
// ---------------------------------------------------------
void updateOctaveAndVolume() {
    if (appMode != AppMode::PLAY) return;

    auto status = M5Cardputer.Keyboard.keysState();
    bool octUp = false, octDn = false, volUp = false, volDn = false;
    bool bendDn = false, bendUp = false;

    for (char c : status.word) {
        if (c == '=') octUp  = true;
        if (c == '-') octDn  = true;
        if (c == '.') volUp  = true;
        if (c == ',') volDn  = true;
        if (c == 'z') bendDn = true;
        if (c == 'x') bendUp = true;
    }

    // Octave (edge-triggered)
    if (octUp && !prevOctaveUpPressed   && params.octaveShift < 2)  params.octaveShift++;
    if (octDn && !prevOctaveDownPressed && params.octaveShift > -2) params.octaveShift--;
    prevOctaveUpPressed   = octUp;
    prevOctaveDownPressed = octDn;

    // Volume (edge-triggered, 5% steps)
    if (volUp && !prevVolumeUpPressed)
        params.keyVolume = min(params.keyVolume + 0.05f, 1.0f);
    if (volDn && !prevVolumeDownPressed)
        params.keyVolume = max(params.keyVolume - 0.05f, 0.0f);
    prevVolumeUpPressed  = volUp;
    prevVolumeDownPressed = volDn;

    // Bend keys: goal set while held, cleared on release
    // Simultaneous Z+X cancels to zero (accidental press guard)
    if      (bendDn && !bendUp) keyBendGoal = -keyBendMaxCents;
    else if (bendUp && !bendDn) keyBendGoal = +keyBendMaxCents;
    else                        keyBendGoal =  0.0f;
}

// ---------------------------------------------------------
// IMU value -> synth parameter
// ---------------------------------------------------------
// value range:
//   bipolar  : -1.0 .. +1.0
//   absolute :  0.0 ..  1.0
void applyImuValue(ImuTarget target, float value) {
    switch (target) {
        case ImuTarget::TIMBRE:
            params.timbreMorphTarget = value * 3.0f; break;
        case ImuTarget::VIBRATO_DEPTH:
            params.vibratoDepthTarget = value; break;
        case ImuTarget::VIBRATO_RATE:
            params.vibratoRateHzTarget = 1.0f + value * 9.0f; break;
        case ImuTarget::TREMOLO:
            params.tremoloDepthTarget = value; break;
        case ImuTarget::VOLUME:
            params.volumeOffsetTarget = value * 0.5f; break;
        case ImuTarget::PITCH_BEND:
            // Bipolar: tilt direction controls bend direction
            params.pitchBendCentsTarget = value * keyBendMaxCents; break;
        case ImuTarget::BEND_UP:
            // Absolute: tilt amount raises pitch regardless of direction
            params.pitchBendCentsTarget = fabsf(value) * keyBendMaxCents; break;
        case ImuTarget::BEND_DOWN:
            // Absolute: tilt amount lowers pitch regardless of direction
            params.pitchBendCentsTarget = -fabsf(value) * keyBendMaxCents; break;
        case ImuTarget::BITCRUSH:
            params.bitcrushTarget = value; break;
        case ImuTarget::FILTER_CUTOFF:
            // Tilt = push cutoff downward (more muffled) from the EDIT baseline
            params.filterCutoffOffsetTarget = value; break;
        default: break;
    }
}

// ---------------------------------------------------------
// IMU read -> parameter update
// ---------------------------------------------------------
float lastAccelX = 0.0f;
float lastAccelY = 0.0f;

void updateImu() {
    if (!M5.Imu.update()) return;
    auto data = M5.Imu.getImuData();
    float ax = data.accel.x;
    float ay = data.accel.y;
    lastAccelX = ax;
    lastAccelY = ay;

    auto clamp1 = [](float v){ return constrain(v, -1.0f, 1.0f); };
    float angleX = asinf(clamp1(ax)) * 180.0f / PI;
    float angleY = asinf(clamp1(ay)) * 180.0f / PI;

    // Automatically force bipolar/absolute based on target type,
    // overriding the per-axis config where the target demands it.
    auto resolveBipolar = [](ImuTarget t, bool cfg) -> bool {
        if (t == ImuTarget::PITCH_BEND) return true;
        if (t == ImuTarget::BEND_UP || t == ImuTarget::BEND_DOWN) return false;
        return cfg;
    };

    if (imuAxisX.target != ImuTarget::NONE) {
        float norm = constrain((angleX / TILT_MAX_DEGREES) * imuAxisX.sensitivity, -1.0f, 1.0f);
        applyImuValue(imuAxisX.target, resolveBipolar(imuAxisX.target, imuAxisX.bipolar) ? norm : fabsf(norm));
    }
    if (imuAxisY.target != ImuTarget::NONE) {
        float norm = constrain((angleY / TILT_MAX_DEGREES) * imuAxisY.sensitivity, -1.0f, 1.0f);
        applyImuValue(imuAxisY.target, resolveBipolar(imuAxisY.target, imuAxisY.bipolar) ? norm : fabsf(norm));
    }
}

// Return a human-readable name for the given ImuTarget
const char *imuTargetName(ImuTarget t) {
    switch (t) {
        case ImuTarget::NONE:          return "None";
        case ImuTarget::TIMBRE:        return "Timbre";
        case ImuTarget::VIBRATO_DEPTH: return "Vib.Depth";
        case ImuTarget::VIBRATO_RATE:  return "Vib.Rate";
        case ImuTarget::TREMOLO:       return "Tremolo";
        case ImuTarget::VOLUME:        return "Volume";
        case ImuTarget::PITCH_BEND:    return "PitchBend";
        case ImuTarget::BEND_UP:       return "BendUp";
        case ImuTarget::BEND_DOWN:     return "BendDown";
        case ImuTarget::BITCRUSH:      return "Bitcrush";
        case ImuTarget::FILTER_CUTOFF: {
            static char buf[16];
            snprintf(buf, sizeof(buf), "Filter(%s)", filterTypeName(filterParams.type));
            return buf;
        }
        default: return "?";
    }
}

// ---------------------------------------------------------
// SD card helpers
// ---------------------------------------------------------
bool initSDCard() {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    return SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
}

bool ensureCpsFolder() {
    if (SD.exists(CPS_FOLDER_PATH)) return true;
    return SD.mkdir(CPS_FOLDER_PATH);
}

static const char *SETTINGS_FILE_PATH = "/CPS/settings.json";

// Save all configurable parameters to /CPS/settings.json
bool saveSettings() {
    File f = SD.open(SETTINGS_FILE_PATH, FILE_WRITE);
    if (!f) { Serial.println("[Settings] failed to open for write"); return false; }

    f.println("{");
    f.printf("  \"imu_x_target\": %u,\n",   (unsigned)imuAxisX.target);
    f.printf("  \"imu_y_target\": %u,\n",   (unsigned)imuAxisY.target);
    f.printf("  \"bend_max_cents\": %.2f,\n", keyBendMaxCents);
    f.printf("  \"bend_attack\": %.6f,\n",    keyBendAttackSmooth);
    f.printf("  \"bend_release\": %.6f,\n",   keyBendReleaseSmooth);
    f.printf("  \"adsr_attack\": %.3f,\n",    adsr.attackTime);
    f.printf("  \"adsr_decay\": %.3f,\n",     adsr.decayTime);
    f.printf("  \"adsr_sustain\": %.3f,\n",   adsr.sustainLevel);
    f.printf("  \"adsr_release\": %.3f,\n",   adsr.releaseTime);
    f.printf("  \"filter_type\": %u,\n",      (unsigned)filterParams.type);
    f.printf("  \"filter_cutoff\": %.1f,\n",  filterParams.cutoffHz);
    f.printf("  \"filter_q\": %.2f\n",        filterParams.resonanceQ);
    f.println("}");

    f.close();
    Serial.println("[Settings] saved -> " + String(SETTINGS_FILE_PATH));
    return true;
}

// Parse a single "key": value line from the settings JSON
void parseSettingLine(const String &line) {
    int q1 = line.indexOf('"');
    if (q1 < 0) return;
    int q2 = line.indexOf('"', q1 + 1);
    if (q2 < 0) return;
    String key = line.substring(q1 + 1, q2);

    int col = line.indexOf(':', q2);
    if (col < 0) return;
    String vs = line.substring(col + 1);
    vs.trim();
    if (vs.endsWith(",")) vs.remove(vs.length() - 1);
    vs.trim();
    if (vs.length() == 0) return;

    float v = vs.toFloat();
    if      (key == "imu_x_target")  { uint8_t u = (uint8_t)v; if (u < (uint8_t)ImuTarget::TARGET_COUNT) imuAxisX.target = (ImuTarget)u; }
    else if (key == "imu_y_target")  { uint8_t u = (uint8_t)v; if (u < (uint8_t)ImuTarget::TARGET_COUNT) imuAxisY.target = (ImuTarget)u; }
    else if (key == "bend_max_cents")  keyBendMaxCents       = v;
    else if (key == "bend_attack")     keyBendAttackSmooth   = v;
    else if (key == "bend_release")    keyBendReleaseSmooth  = v;
    else if (key == "adsr_attack")     adsr.attackTime       = v;
    else if (key == "adsr_decay")      adsr.decayTime        = v;
    else if (key == "adsr_sustain")    adsr.sustainLevel     = v;
    else if (key == "adsr_release")    adsr.releaseTime      = v;
    else if (key == "filter_type")   { uint8_t u = (uint8_t)v; if (u <= (uint8_t)FilterType::NOTCH) filterParams.type = (FilterType)u; }
    else if (key == "filter_cutoff")   filterParams.cutoffHz    = v;
    else if (key == "filter_q")        filterParams.resonanceQ  = v;
    // Unknown keys are silently ignored for forward compatibility
}

// Load settings from SD. If the file doesn't exist, defaults remain.
bool loadSettings() {
    if (!SD.exists(SETTINGS_FILE_PATH)) {
        Serial.println("[Settings] no file found - using defaults");
        return false;
    }
    File f = SD.open(SETTINGS_FILE_PATH, FILE_READ);
    if (!f) { Serial.println("[Settings] failed to open for read"); return false; }

    while (f.available()) parseSettingLine(f.readStringUntil('\n'));
    f.close();

    updateFilterCoefficients();  // apply loaded filter params
    Serial.println("[Settings] loaded from " + String(SETTINGS_FILE_PATH));
    Serial.printf("[Settings] X=%s Y=%s bend=%.0fc atk=%.5f rel=%.5f\n",
        imuTargetName(imuAxisX.target), imuTargetName(imuAxisY.target),
        keyBendMaxCents, keyBendAttackSmooth, keyBendReleaseSmooth);
    Serial.printf("[Settings] ADSR A=%.2f D=%.2f S=%.0f%% R=%.2f\n",
        adsr.attackTime, adsr.decayTime, adsr.sustainLevel*100, adsr.releaseTime);
    Serial.printf("[Settings] Filter type=%s cutoff=%.0fHz Q=%.2f\n",
        filterTypeName(filterParams.type), filterParams.cutoffHz, filterParams.resonanceQ);
    return true;
}

// ---------------------------------------------------------
// EDIT screen: ADSR + filter settings
// ---------------------------------------------------------

// ---- ADSR item helpers ----
char adsrLabelBuf[12];
void adsrAttackInc()  { adsr.attackTime   = min(adsr.attackTime  + 0.05f, ADSR_MAX_TIME); }
void adsrAttackDec()  { adsr.attackTime   = max(adsr.attackTime  - 0.05f, ADSR_MIN_TIME); }
void adsrDecayInc()   { adsr.decayTime    = min(adsr.decayTime   + 0.05f, ADSR_MAX_TIME); }
void adsrDecayDec()   { adsr.decayTime    = max(adsr.decayTime   - 0.05f, ADSR_MIN_TIME); }
void adsrSustainInc() { adsr.sustainLevel = min(adsr.sustainLevel + 0.05f, 1.0f); }
void adsrSustainDec() { adsr.sustainLevel = max(adsr.sustainLevel - 0.05f, 0.0f); }
void adsrReleaseInc() { adsr.releaseTime  = min(adsr.releaseTime  + 0.05f, ADSR_MAX_TIME); }
void adsrReleaseDec() { adsr.releaseTime  = max(adsr.releaseTime  - 0.05f, ADSR_MIN_TIME); }
const char *adsrAttackLabel()  { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.attackTime);   return adsrLabelBuf; }
const char *adsrDecayLabel()   { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.decayTime);    return adsrLabelBuf; }
const char *adsrSustainLabel() { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%d%%",  (int)(adsr.sustainLevel*100)); return adsrLabelBuf; }
const char *adsrReleaseLabel() { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.releaseTime);  return adsrLabelBuf; }

// ---- Filter item helpers ----
const char *filterTypeName(FilterType t) {
    switch (t) {
        case FilterType::LPF:   return "LPF";
        case FilterType::HPF:   return "HPF";
        case FilterType::BPF:   return "BPF";
        case FilterType::NOTCH: return "Notch";
        default:                return "?";
    }
}
void filterTypeNext() { uint8_t v = (uint8_t)filterParams.type; filterParams.type = (FilterType)((v+1)%4); updateFilterCoefficients(); }
void filterTypePrev() { uint8_t v = (uint8_t)filterParams.type; filterParams.type = (FilterType)(v==0?3:v-1); updateFilterCoefficients(); }
char filterTypeLabelBuf[8];
const char *filterTypeLabel() { snprintf(filterTypeLabelBuf, sizeof(filterTypeLabelBuf), "%s", filterTypeName(filterParams.type)); return filterTypeLabelBuf; }

constexpr float FILTER_CUTOFF_STEP = 100.0f;
void filterCutoffInc() { filterParams.cutoffHz = min(filterParams.cutoffHz + FILTER_CUTOFF_STEP, FILTER_CUTOFF_MAX); updateFilterCoefficients(); }
void filterCutoffDec() { filterParams.cutoffHz = max(filterParams.cutoffHz - FILTER_CUTOFF_STEP, FILTER_CUTOFF_MIN); updateFilterCoefficients(); }
char filterCutoffLabelBuf[16];
const char *filterCutoffLabel() { snprintf(filterCutoffLabelBuf, sizeof(filterCutoffLabelBuf), "%.0fHz", filterParams.cutoffHz); return filterCutoffLabelBuf; }

void filterResonanceInc() { filterParams.resonanceQ = min(filterParams.resonanceQ + 0.1f, FILTER_Q_MAX); updateFilterCoefficients(); }
void filterResonanceDec() { filterParams.resonanceQ = max(filterParams.resonanceQ - 0.1f, FILTER_Q_MIN); updateFilterCoefficients(); }
char filterResonanceLabelBuf[16];
const char *filterResonanceLabel() { snprintf(filterResonanceLabelBuf, sizeof(filterResonanceLabelBuf), "Q%.1f", filterParams.resonanceQ); return filterResonanceLabelBuf; }

// EDIT menu item list: 4 ADSR + 3 filter items
SettingItem editItems[] = {
    { "Attack",    adsrAttackInc,     adsrAttackDec,     adsrAttackLabel },
    { "Decay",     adsrDecayInc,      adsrDecayDec,      adsrDecayLabel },
    { "Sustain",   adsrSustainInc,    adsrSustainDec,    adsrSustainLabel },
    { "Release",   adsrReleaseInc,    adsrReleaseDec,    adsrReleaseLabel },
    { "Filter",    filterTypeNext,    filterTypePrev,    filterTypeLabel },
    { "Cutoff",    filterCutoffInc,   filterCutoffDec,   filterCutoffLabel },
    { "Resonance", filterResonanceInc,filterResonanceDec,filterResonanceLabel },
};
const int NUM_EDIT_ITEMS = sizeof(editItems) / sizeof(editItems[0]);
int selectedEditIndex = 0;

// ---------------------------------------------------------
// Tab / menu navigation
// ---------------------------------------------------------
void updateMenuNavigation() {
    // Note: Tab is NOT in status.word; it has its own bool field (status.tab).
    auto status = M5Cardputer.Keyboard.keysState();
    bool tabPressed   = status.tab;
    bool menuUp       = false;
    bool menuDown     = false;
    bool menuIncrease = false;
    bool menuDecrease = false;

    for (char c : status.word) {
        if (c == ';') menuUp       = true;
        if (c == '.') menuDown     = true;
        if (c == '/') menuIncrease = true;
        if (c == ',') menuDecrease = true;
    }

    // Tab: cycle PLAY -> EDIT -> SETTINGS -> PLAY (edge-triggered)
    if (tabPressed && !prevTabPressed) {
        AppMode prev = appMode;
        switch (appMode) {
            case AppMode::PLAY:     appMode = AppMode::EDIT;     currentFreq = 0.0f; break;
            case AppMode::EDIT:     appMode = AppMode::SETTINGS; break;
            case AppMode::SETTINGS: appMode = AppMode::PLAY;     break;
        }
        // Auto-save when leaving SETTINGS
        if (prev == AppMode::SETTINGS && appMode == AppMode::PLAY) saveSettings();
    }
    prevTabPressed = tabPressed;

    // SETTINGS navigation
    if (appMode == AppMode::SETTINGS) {
        if (menuUp   && !prevMenuUpPressed)   { selectedSettingIndex = (selectedSettingIndex - 1 + NUM_SETTING_ITEMS) % NUM_SETTING_ITEMS; }
        if (menuDown && !prevMenuDownPressed) { selectedSettingIndex = (selectedSettingIndex + 1) % NUM_SETTING_ITEMS; }
        if (menuIncrease && !prevMenuIncPressed) settingItems[selectedSettingIndex].onIncrement();
        if (menuDecrease && !prevMenuDecPressed) settingItems[selectedSettingIndex].onDecrement();
    }
    // EDIT navigation
    if (appMode == AppMode::EDIT) {
        if (menuUp   && !prevMenuUpPressed)   { selectedEditIndex = (selectedEditIndex - 1 + NUM_EDIT_ITEMS) % NUM_EDIT_ITEMS; }
        if (menuDown && !prevMenuDownPressed) { selectedEditIndex = (selectedEditIndex + 1) % NUM_EDIT_ITEMS; }
        if (menuIncrease && !prevMenuIncPressed) editItems[selectedEditIndex].onIncrement();
        if (menuDecrease && !prevMenuDecPressed) editItems[selectedEditIndex].onDecrement();
    }

    prevMenuUpPressed   = menuUp;
    prevMenuDownPressed = menuDown;
    prevMenuIncPressed  = menuIncrease;
    prevMenuDecPressed  = menuDecrease;
}

// ---------------------------------------------------------
// Display helpers
// ---------------------------------------------------------

// Draw the tab bar at the top of the screen (y=0..10)
// The active tab is highlighted: white background, black text
void drawTabBar(AppMode current) {
    M5Cardputer.Display.fillRect(0, 0, 240, 11, BLACK);

    struct { const char *label; AppMode mode; int x; int w; } tabs[] = {
        { "MAIN",    AppMode::PLAY,     0,   80 },
        { "EDIT",    AppMode::EDIT,     80,  80 },
        { "SETTING", AppMode::SETTINGS, 160, 80 },
    };

    for (auto &t : tabs) {
        bool active = (current == t.mode);
        M5Cardputer.Display.drawRect(t.x, 0, t.w, 11, GREEN);
        if (active) {
            M5Cardputer.Display.fillRect(t.x + 1, 1, t.w - 2, 9, WHITE);
            M5Cardputer.Display.setTextColor(BLACK, WHITE);
        } else {
            M5Cardputer.Display.setTextColor(GREEN, BLACK);
        }
        int lx = t.x + (t.w - (int)strlen(t.label) * 6) / 2;
        M5Cardputer.Display.setCursor(lx, 2);
        M5Cardputer.Display.print(t.label);
    }
    M5Cardputer.Display.setTextColor(GREEN, BLACK);
}

// Draw the ADSR envelope shape as a line graph (used in EDIT screen)
void drawAdsrGraph() {
    constexpr int GX = 0, GY = 12, GW = 240, GH = 60;
    M5Cardputer.Display.fillRect(GX, GY, GW, GH, BLACK);

    // Normalise ADSR times so the graph always fits the width
    float totalT = adsr.attackTime + adsr.decayTime + 0.3f + adsr.releaseTime;
    float scaleX = (float)GW / totalT;
    int top      = GY + 4;
    int bot      = GY + GH - 4;
    int susY     = bot - (int)((bot - top) * adsr.sustainLevel);

    int x0 = GX;
    int x1 = x0 + (int)(adsr.attackTime   * scaleX);
    int x2 = x1 + (int)(adsr.decayTime    * scaleX);
    int x3 = x2 + (int)(0.3f              * scaleX);
    int x4 = x3 + (int)(adsr.releaseTime  * scaleX);

    M5Cardputer.Display.drawLine(x0, bot,  x1, top,  GREEN);
    M5Cardputer.Display.drawLine(x1, top,  x2, susY, GREEN);
    M5Cardputer.Display.drawLine(x2, susY, x3, susY, GREEN);
    M5Cardputer.Display.drawLine(x3, susY, x4, bot,  GREEN);

    // Vertex markers
    uint16_t yellow = M5Cardputer.Display.color565(255, 255, 0);
    for (auto &pt : { std::pair<int,int>{x1,top}, {x2,susY}, {x3,susY} })
        M5Cardputer.Display.fillRect(pt.first-1, pt.second-1, 3, 3, yellow);

    // Segment labels
    M5Cardputer.Display.setCursor(x0 + 2,        GY + GH - 10); M5Cardputer.Display.print("A");
    M5Cardputer.Display.setCursor(x1 + 2,        GY + GH - 10); M5Cardputer.Display.print("D");
    M5Cardputer.Display.setCursor((x2+x3)/2 - 3, GY + GH - 10); M5Cardputer.Display.print("S");
    M5Cardputer.Display.setCursor(x3 + 2,        GY + GH - 10); M5Cardputer.Display.print("R");

    M5Cardputer.Display.drawFastHLine(GX, GY + GH, GW, GREEN);
}

// EDIT screen: ADSR graph (top) + 2-column item list (bottom)
void drawEditScreen() {
    M5Cardputer.Display.fillScreen(BLACK);
    drawTabBar(AppMode::EDIT);
    drawAdsrGraph();

    constexpr int LIST_Y = 76, ROW_H = 12, COL2_X = 125;
    for (int i = 0; i < NUM_EDIT_ITEMS; i++) {
        bool left = (i < 4);
        int x = left ? 5 : COL2_X;
        int y = LIST_Y + (left ? i : i - 4) * ROW_H;
        M5Cardputer.Display.setCursor(x, y);
        if (i == selectedEditIndex)
            M5Cardputer.Display.printf(">%-9s%s", editItems[i].name, editItems[i].valueLabel());
        else
            M5Cardputer.Display.printf(" %-9s%s", editItems[i].name, editItems[i].valueLabel());
    }
    M5Cardputer.Display.setCursor(5, 126);
    M5Cardputer.Display.print(";/. select  ,// change  Tab:next");
}

// SETTINGS screen: scrollable item list
void drawSettingsScreen() {
    M5Cardputer.Display.fillScreen(BLACK);
    drawTabBar(AppMode::SETTINGS);

    for (int i = 0; i < NUM_SETTING_ITEMS; i++) {
        int y = 18 + i * 15;
        M5Cardputer.Display.setCursor(5, y);
        if (i == selectedSettingIndex)
            M5Cardputer.Display.printf("> %-14s %s", settingItems[i].name, settingItems[i].valueLabel());
        else
            M5Cardputer.Display.printf("  %-14s %s", settingItems[i].name, settingItems[i].valueLabel());
    }
    M5Cardputer.Display.setCursor(5, 115);
    M5Cardputer.Display.println(";/. select  ,// change");
    M5Cardputer.Display.setCursor(5, 128);
    M5Cardputer.Display.println("Tab: save & return to play");
}

// ---------------------------------------------------------
// MAIN (PLAY) screen
// ---------------------------------------------------------

// Return note name string for a given frequency (e.g. "C4", "A#3")
// Returns "---" for silence (freq <= 0)
const char *getNoteName(float freq) {
    if (freq <= 0.0f) return "---";
    static const char *names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int noteNum = (int)roundf(12.0f * log2f(freq / 440.0f)) + 69;
    int name    = noteNum % 12;
    if (name < 0) name += 12;
    static char buf[8];
    snprintf(buf, sizeof(buf), "%s%d", names[name], noteNum / 12 - 1);
    return buf;
}

// Draw the real-time waveform graph (y=12..55)
// Shows 3 cycles of the current morphed waveform (static phase, updated each frame)
void drawWaveform() {
    constexpr int GX = 0, GY = 12, GW = 240, GH = 43;
    constexpr int CY = GY + GH / 2;
    constexpr int CYCLES = 3;

    M5Cardputer.Display.fillRect(GX, GY, GW, GH, BLACK);
    M5Cardputer.Display.drawFastHLine(GX, CY, GW, M5Cardputer.Display.color565(0, 64, 0));

    int prevY = CY;
    for (int px = 0; px < GW; px++) {
        int idx = (int)((float)px / GW * WAVE_TABLE_SIZE * CYCLES) % WAVE_TABLE_SIZE;
        int16_t s = getMorphedSample(idx, params.timbreMorph);
        int y = constrain(CY - (int)((float)s / 32768.0f * (GH / 2 - 2)), GY, GY + GH - 1);
        if (px > 0) M5Cardputer.Display.drawLine(px - 1, prevY, px, y, GREEN);
        prevY = y;
    }
    M5Cardputer.Display.drawFastHLine(GX, GY + GH, GW, GREEN);
}

// Draw the vertical bend meter (x=73..89, y=57..109)
void drawBendMeter(float bendCents, float maxCents) {
    constexpr int MX = 75, MY = 57, MW = 12, MH = 50;
    constexpr int MCY = MY + MH / 2;

    M5Cardputer.Display.fillRect(MX, MY, MW, MH, BLACK);
    M5Cardputer.Display.drawRect(MX, MY, MW, MH, GREEN);
    M5Cardputer.Display.drawFastHLine(MX, MCY, MW, GREEN);

    if (fabsf(bendCents) > 0.5f) {
        float ratio = constrain(bendCents / maxCents, -1.0f, 1.0f);
        int barH = (int)(fabsf(ratio) * (MH / 2 - 1));
        M5Cardputer.Display.fillRect(MX + 1, ratio > 0 ? MCY - barH : MCY, MW - 2, barH, GREEN);
    }
    M5Cardputer.Display.setCursor(MX - 2, MY - 7);  M5Cardputer.Display.print("UP");
    M5Cardputer.Display.setCursor(MX - 5, MY + MH + 1); M5Cardputer.Display.print("DWN");
}

// Draw the IMU tilt pad (x=90..133, y=57..100) with X/Y axis labels
void drawImuPad() {
    constexpr int PX = 90, PY = 57, PAD_SIZE = 44;
    constexpr int cx = PX + PAD_SIZE / 2;
    constexpr int cy = PY + PAD_SIZE / 2;

    M5Cardputer.Display.fillRect(PX - 1, PY - 9, PAD_SIZE + 16, 9, BLACK);
    M5Cardputer.Display.fillRect(PX, PY, PAD_SIZE, PAD_SIZE, BLACK);
    M5Cardputer.Display.drawRect(PX, PY, PAD_SIZE, PAD_SIZE, GREEN);
    M5Cardputer.Display.drawFastHLine(PX, cy, PAD_SIZE, M5Cardputer.Display.color565(0, 64, 0));
    M5Cardputer.Display.drawFastVLine(cx, PY, PAD_SIZE, M5Cardputer.Display.color565(0, 64, 0));

    // Plot current tilt position as a filled dot
    float normX = constrain(lastAccelX / (TILT_MAX_DEGREES / 57.3f), -1.0f, 1.0f);
    float normY = constrain(lastAccelY / (TILT_MAX_DEGREES / 57.3f), -1.0f, 1.0f);
    int dotX = constrain(cx + (int)(normX * (PAD_SIZE / 2 - 3)), PX + 2, PX + PAD_SIZE - 3);
    int dotY = constrain(cy + (int)(normY * (PAD_SIZE / 2 - 3)), PY + 2, PY + PAD_SIZE - 3);
    M5Cardputer.Display.fillCircle(dotX, dotY, 3, GREEN);

    M5Cardputer.Display.setCursor(cx - 3, PY - 8);         M5Cardputer.Display.print("Y");
    M5Cardputer.Display.setCursor(PX + PAD_SIZE + 2, cy - 4); M5Cardputer.Display.print("X");
}

// Return normalized 0..1 value of an IMU-driven parameter (for the mini-bar display)
float getImuParamNormalized(ImuTarget target) {
    switch (target) {
        case ImuTarget::TIMBRE:        return params.timbreMorph / 3.0f;
        case ImuTarget::VIBRATO_DEPTH: return params.vibratoDepth;
        case ImuTarget::VIBRATO_RATE:  return (params.vibratoRateHz - 1.0f) / 9.0f;
        case ImuTarget::TREMOLO:       return params.tremoloDepth;
        case ImuTarget::VOLUME:        return params.volumeOffset + 0.5f;
        case ImuTarget::PITCH_BEND:    return (params.pitchBendCents + keyBendMaxCents) / (keyBendMaxCents * 2.0f);
        case ImuTarget::BEND_UP:       return constrain(params.pitchBendCents / keyBendMaxCents, 0.0f, 1.0f);
        case ImuTarget::BEND_DOWN:     return constrain(-params.pitchBendCents / keyBendMaxCents, 0.0f, 1.0f);
        case ImuTarget::BITCRUSH:      return params.bitcrush;
        case ImuTarget::FILTER_CUTOFF: return params.filterCutoffOffset;
        default:                       return 0.0f;
    }
}

// Return current value of an IMU-driven parameter as a display string
String getImuParamValueStr(ImuTarget target) {
    switch (target) {
        case ImuTarget::TIMBRE: {
            const char *s[] = {"Sine","Tri","Saw","Sq"};
            return String(s[constrain((int)params.timbreMorph, 0, 3)])
                   + "(" + String(params.timbreMorph, 1) + ")";
        }
        case ImuTarget::VIBRATO_DEPTH: return String((int)(params.vibratoDepth * 100)) + "%";
        case ImuTarget::VIBRATO_RATE:  return String(params.vibratoRateHz, 1) + "Hz";
        case ImuTarget::TREMOLO:       return String((int)(params.tremoloDepth * 100)) + "%";
        case ImuTarget::VOLUME:        return String((int)(params.volumeOffset * 100)) + "%";
        case ImuTarget::PITCH_BEND:    return String((int)params.pitchBendCents) + "c";
        case ImuTarget::BEND_UP:       return "+" + String((int)params.pitchBendCents) + "c";
        case ImuTarget::BEND_DOWN:     return String((int)params.pitchBendCents) + "c";
        case ImuTarget::BITCRUSH:      return String((int)(params.bitcrush * 100)) + "%";
        case ImuTarget::FILTER_CUTOFF: {
            float c = filterParams.cutoffHz * (1.0f - params.filterCutoffOffset * 0.9f);
            return String((int)constrain(c, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX)) + "Hz";
        }
        default: return "---";
    }
}

// Full MAIN screen layout:
//   y: 0-10   tab bar
//   y:12-55   waveform graph
//   y:56-112  info area
//     x:0-72    note / freq block
//     x:73-89   bend meter
//     x:90-133  IMU tilt pad
//     x:140-239 IMU parameter readout
//   y:113     divider line
//   y:115-134 key guide
void drawPlayScreen(bool fullRedraw) {
    if (fullRedraw) {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.drawFastHLine(0, 113, 240, GREEN);
        M5Cardputer.Display.setCursor(3, 115);
        M5Cardputer.Display.print("1-0:NOTE Z/X:BEND =/-.OCT ,/.VOL");
        M5Cardputer.Display.setCursor(3, 125);
        M5Cardputer.Display.print("Tab:MENU");
    }

    drawTabBar(AppMode::PLAY);
    drawWaveform();

    // --- Note / frequency block (left) ---
    M5Cardputer.Display.fillRect(0, 56, 73, 57, BLACK);
    M5Cardputer.Display.drawRect(0, 56, 73, 57, GREEN);

    float dispFreq  = playingFreq > 0.0f ? playingFreq : currentFreq;
    const char *noteName = getNoteName(dispFreq);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(4, 60);
    M5Cardputer.Display.printf("%-4s", noteName);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setCursor(4, 84);
    if (dispFreq > 0.0f)
        M5Cardputer.Display.printf("%-9s", (String(dispFreq, 1) + "Hz").c_str());
    else
        M5Cardputer.Display.print("---      ");

    M5Cardputer.Display.setCursor(4, 95);
    M5Cardputer.Display.printf("OCT:%+d  ", params.octaveShift);

    // --- Bend meter (centre) ---
    drawBendMeter(params.pitchBendCents + keyBendCurrent, keyBendMaxCents);

    // --- IMU tilt pad ---
    drawImuPad();

    // --- IMU parameter readout (right, x=140..239) ---
    constexpr int TX = 140, TW = 100;
    M5Cardputer.Display.fillRect(TX, 56, TW, 57, BLACK);

    // X axis assignment + value + mini-bar
    M5Cardputer.Display.setCursor(TX, 57);
    M5Cardputer.Display.printf("X:%-10s", imuTargetName(imuAxisX.target));
    M5Cardputer.Display.setCursor(TX, 66);
    M5Cardputer.Display.printf(" %-11s", getImuParamValueStr(imuAxisX.target).c_str());
    if (imuAxisX.target != ImuTarget::NONE) {
        float n = constrain(getImuParamNormalized(imuAxisX.target), 0.0f, 1.0f);
        M5Cardputer.Display.fillRect(TX, 75, TW - 2, 4, M5Cardputer.Display.color565(0, 64, 0));
        if ((int)(n * (TW-4)) > 0) M5Cardputer.Display.fillRect(TX, 75, (int)(n*(TW-4)), 4, GREEN);
    }

    // Y axis assignment + value + mini-bar
    M5Cardputer.Display.setCursor(TX, 81);
    M5Cardputer.Display.printf("Y:%-10s", imuTargetName(imuAxisY.target));
    M5Cardputer.Display.setCursor(TX, 90);
    M5Cardputer.Display.printf(" %-11s", getImuParamValueStr(imuAxisY.target).c_str());
    if (imuAxisY.target != ImuTarget::NONE) {
        float n = constrain(getImuParamNormalized(imuAxisY.target), 0.0f, 1.0f);
        M5Cardputer.Display.fillRect(TX, 99, TW - 2, 4, M5Cardputer.Display.color565(0, 64, 0));
        if ((int)(n * (TW-4)) > 0) M5Cardputer.Display.fillRect(TX, 99, (int)(n*(TW-4)), 4, GREEN);
    }

    // Volume + bend width summary
    M5Cardputer.Display.setCursor(TX, 105);
    M5Cardputer.Display.printf("VOL:%d%% BND:%dst  ",
        (int)(params.keyVolume * 100),
        (int)(keyBendMaxCents / 100.0f));
}

// ---------------------------------------------------------
// setup
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard

    M5Cardputer.Display.setRotation(1);
    // Use opaque text (background = BLACK) to avoid character ghosting
    // when overwriting with fixed-width strings.
    M5Cardputer.Display.setTextColor(GREEN, BLACK);
    M5Cardputer.Display.setTextSize(1);

    // Startup message (briefly visible before first screen draw)
    M5Cardputer.Display.drawString("CPS - CardPuter Synth", 10, 10);
    M5Cardputer.Display.drawString("Keys 1-0: notes  =/- oct  ,/. vol", 10, 25);

    buildWaveTables();
    updateFilterCoefficients();

    // SD card: init, create /CPS folder, load saved settings
    bool sdOk = initSDCard();
    if (sdOk) {
        ensureCpsFolder();
        Serial.println("[SD] OK - /CPS ready");
        loadSettings();
    } else {
        Serial.println("[SD] not found - using defaults");
    }

    // IMU (BMI270 on CardputerADV only)
    // Access via M5.Imu, not M5Cardputer.Imu
    bool imuOk = M5.Imu.begin();
    Serial.println(imuOk ? "[IMU] OK" : "[IMU] not found - tilt disabled");

    // Speaker / I2S configuration
    auto spk_cfg             = M5Cardputer.Speaker.config();
    spk_cfg.sample_rate      = SAMPLE_RATE;
    spk_cfg.dma_buf_count    = 4;
    spk_cfg.dma_buf_len      = 512;
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    M5Cardputer.Speaker.config(spk_cfg);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(255);

    // Audio generation task on Core 1, priority 5
    xTaskCreatePinnedToCore(audioTask, "audioTask", 4096, nullptr, 5, nullptr, 1);
}

// ---------------------------------------------------------
// loop
// ---------------------------------------------------------
unsigned long lastDisplayUpdateMs = 0;
// Initialise to a value != PLAY so the first iteration triggers a full redraw
AppMode lastDrawnMode = AppMode::SETTINGS;

void loop() {
    M5Cardputer.update();
    updateImu();
    updateMenuNavigation();

    bool keyChanged = M5Cardputer.Keyboard.isChange();
    if (keyChanged) {
        updateOctaveAndVolume();
        float newFreq = resolveFreqFromKeys();
        if (newFreq > 0.0f && currentFreq == 0.0f) {
            if (envPhase == EnvPhase::IDLE) envLevel = 0.0f;
            envPhase = EnvPhase::ATTACK;
        }
        currentFreq = newFreq;
    }

    bool modeChanged = (appMode != lastDrawnMode);
    lastDrawnMode = appMode;

    if (appMode == AppMode::EDIT) {
        if (keyChanged || modeChanged) drawEditScreen();
        delay(5);
        return;
    }
    if (appMode == AppMode::SETTINGS) {
        if (keyChanged || modeChanged) drawSettingsScreen();
        delay(5);
        return;
    }

    // MAIN screen: refresh every 50 ms for real-time waveform / IMU display
    unsigned long now = millis();
    if (keyChanged || modeChanged || (now - lastDisplayUpdateMs) >= 50) {
        lastDisplayUpdateMs = now;
        drawPlayScreen(modeChanged);
    }

    delay(5);
}
