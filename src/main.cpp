/*
 * C.P.S. (CardPuter Synth) - v0.7q
 * -------------------------------------------------------
 * A DIY synthesizer app for the M5Stack CardputerADV.
 *
 * Features:
 *   - Number keys "1234567890" mapped to musical notes (C4 and up)
 *   - Monophonic: last key pressed wins
 *   - '=' / '-' keys: octave shift (-2 to +2)
 *   - '[' / ']' keys: transpose (-12 to +12 semitones), independent
 *     of octave shift
 *   - ',' / '.' keys: volume control (0-100%, 5% steps)
 *   - 'Z' key: bend down  /  'X' key: bend up
 *     Guitar-chording style: slow pitch rise on press,
 *     fast return on release (asymmetric attack/release).
 *   - 'C' key: portamento ON/OFF toggle (PLAY mode)
 *   - 'A' key: IMU X axis hold toggle
 *   - 'S' key: IMU Y axis hold toggle
 *   - 'D' key: note hold toggle
 *   - 'H' key: hold to show HELP overlay on MAIN screen
 *   - MAIN screen shows "(HOLD)" next to an IMU axis readout when
 *     that axis is held
 *   - ADSR envelope with retrigger support
 *   - Biquad filter: LPF/HPF/BPF/Notch/None, with its own filter envelope
 *   - Portamento: glide speed & on/off in SETTING menu
 *   - Sub oscillator, noise blend, bit-crusher
 *   - General-purpose LFO (Sine/Triangle/Sawtooth/Square) that can
 *     modulate Pitch / Volume / Timbre / Filter cutoff / PWM.
 *     Fully independent from the existing Vibrato and Tremolo LFOs.
 *   - IMU (BMI270) tilt-to-parameter mapping (customizable, hold state
 *     and frozen value persist across save/load)
 *   - Patch Bank: save/load full synth state (incl. IMU mapping/hold
 *     state and portamento) as named patches under /CPS/Patch.
 *     Rename, duplicate and delete are available from the Patch Bank
 *     screen (SETTING menu). The app never navigates outside this
 *     dedicated folder.
 *   - Tab key cycles: MAIN -> VCO -> VCF -> VCA -> LFO -> SETTING
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
// SD card pin configuration
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

constexpr float SINE_AMP     = 32000.0f;
constexpr float TRIANGLE_AMP = 30000.0f;
constexpr float SAWTOOTH_AMP = 22000.0f;
constexpr float SQUARE_AMP   = 18000.0f;

// ---------------------------------------------------------
// Note-key table
// ---------------------------------------------------------
struct KeyNote { char key; float freq; };
KeyNote keyNotes[] = {
    {'1',261.63f},{'2',293.66f},{'3',329.63f},{'4',349.23f},{'5',392.00f},
    {'6',440.00f},{'7',493.88f},{'8',523.25f},{'9',587.33f},{'0',659.25f},
};
const int NUM_KEYS = sizeof(keyNotes)/sizeof(keyNotes[0]);

// ---------------------------------------------------------
// IMU mapping
// ---------------------------------------------------------
enum class ImuTarget : uint8_t {
    NONE, TIMBRE, VIBRATO_DEPTH, VIBRATO_RATE, TREMOLO,
    VOLUME, PITCH_BEND, BEND_UP, BEND_DOWN, BITCRUSH, FILTER_CUTOFF,
    TARGET_COUNT
};

// Forward declarations
const char *imuTargetName(ImuTarget t);
void resetParamToDefault(ImuTarget t);
void drawWaveform();
void drawAdsrGraph();

struct ImuAxisConfig {
    ImuTarget target;
    float sensitivity;
    bool bipolar;
};

ImuAxisConfig imuAxisX = { ImuTarget::TIMBRE,        1.0f, false };
ImuAxisConfig imuAxisY = { ImuTarget::VIBRATO_DEPTH,  1.0f, false };

constexpr float TILT_MAX_DEGREES = 35.0f;

// ---------------------------------------------------------
// Synth parameters
// ---------------------------------------------------------
struct SynthParams {
    float keyVolume   = 0.5f;
    int   octaveShift = 0;
    // VCO
    float pwmWidth      = 0.5f;
    float detuneCents   = 0.0f;
    float fineTuneCents = 0.0f;
    // Sub oscillator
    float subOscLevel  = 0.0f;   // 0.0 - 1.0
    int   subOscOctave = -1;     // -1 or -2
    // Noise blend
    float noiseLevel   = 0.0f;   // 0.0 - 1.0
    // IMU current
    float timbreMorph        = 0.0f;
    float vibratoDepth       = 0.0f;
    float vibratoRateHz      = 5.0f;
    float tremoloDepth       = 0.0f;
    float volumeOffset       = 0.0f;
    float pitchBendCents     = 0.0f;
    float bitcrush           = 0.0f;
    float filterCutoffOffset = 0.0f;
    // IMU targets
    float timbreMorphTarget        = 0.0f;
    float vibratoDepthTarget       = 0.0f;
    float vibratoRateHzTarget      = 5.0f;
    float tremoloDepthTarget       = 0.0f;
    float volumeOffsetTarget       = 0.0f;
    float pitchBendCentsTarget     = 0.0f;
    float bitcrushTarget           = 0.0f;
    float filterCutoffOffsetTarget = 0.0f;
} params;

float currentFreq = 0.0f;

// ---------------------------------------------------------
// Portamento
// ---------------------------------------------------------
bool  portaEnabled = false;
float portaFreq    = 0.0f;   // current smoothed frequency
float portaSpeed   = 0.005f; // smoothing coeff (higher = faster glide)

// ---------------------------------------------------------
// IMU / note hold
// ---------------------------------------------------------
bool imuXHeld = false, imuYHeld = false, noteHeld = false;
bool prevImuXHoldPressed = false, prevImuYHoldPressed = false;
bool prevNoteHoldPressed = false;
float heldFreq = 0.0f;
bool prevPortaPressed = false;

// Help overlay state
bool helpVisible     = false;
bool prevHelpPressed = false;

// Edge detection
bool prevOctaveUpPressed = false, prevOctaveDownPressed = false;
bool prevVolumeUpPressed = false, prevVolumeDownPressed = false;
bool prevTransposeUpPressed = false, prevTransposeDownPressed = false;

// Transpose: semitone-level key change, independent of octave shift
int transposeSemitones = 0;
constexpr int TRANSPOSE_MIN = -12, TRANSPOSE_MAX = 12;

// Phase accumulators
double phase        = 0.0;
double vibratoPhase = 0.0;
double tremoloPhase = 0.0;

constexpr float VIBRATO_MAX_CENTS = 35.0f;

// ---------------------------------------------------------
// Key bend
// ---------------------------------------------------------
float keyBendMaxCents   = 200.0f;
float keyBendGoal       = 0.0f;
float keyBendCurrent    = 0.0f;
constexpr float KEY_BEND_ATTACK_SMOOTH_DEFAULT  = 0.0003f;
constexpr float KEY_BEND_RELEASE_SMOOTH_DEFAULT = 0.003f;
float keyBendAttackSmooth  = KEY_BEND_ATTACK_SMOOTH_DEFAULT;
float keyBendReleaseSmooth = KEY_BEND_RELEASE_SMOOTH_DEFAULT;

// ---------------------------------------------------------
// ADSR
// ---------------------------------------------------------
enum class EnvPhase : uint8_t { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

struct AdsrParams {
    float attackTime   = 0.05f;
    float decayTime    = 0.15f;
    float sustainLevel = 0.7f;
    float releaseTime  = 0.3f;
} adsr;

constexpr float ADSR_MIN_TIME = 0.0f;
constexpr float ADSR_MAX_TIME = 5.0f;

float    envLevel    = 0.0f;
EnvPhase envPhase    = EnvPhase::IDLE;
float    playingFreq = 0.0f;

// ---------------------------------------------------------
// Filter
// ---------------------------------------------------------
enum class FilterType : uint8_t { LPF, HPF, BPF, NOTCH, NONE };

const char *filterTypeName(FilterType t);  // forward declaration

struct FilterParams {
    FilterType type      = FilterType::LPF;
    float cutoffHz       = 2000.0f;
    float resonanceQ     = 0.707f;
    float keyTracking    = 0.0f;  // 0=off, 1=full (cutoff tracks note pitch)
} filterParams;

constexpr float FILTER_CUTOFF_MIN = 100.0f;
constexpr float FILTER_CUTOFF_MAX = 8000.0f;
constexpr float FILTER_Q_MIN      = 0.5f;
constexpr float FILTER_Q_MAX      = 10.0f;

// Filter envelope: modulates cutoff with its own ADSR
struct FilterEnvParams {
    float depth      = 0.0f;    // Hz offset at peak (+/- up to 3900Hz)
    float attackTime = 0.1f;
    float decayTime  = 0.3f;
    float sustainLvl = 0.0f;
    float releaseTime= 0.3f;
} filterEnv;

float    filterEnvLevel = 0.0f;
EnvPhase filterEnvPhase = EnvPhase::IDLE;

float filterB0=1.f,filterB1=0.f,filterB2=0.f;
float filterA1=0.f,filterA2=0.f;
float filterX1=0.f,filterX2=0.f;
float filterY1=0.f,filterY2=0.f;

// ---------------------------------------------------------
// General-purpose LFO
// ---------------------------------------------------------
// This LFO is independent from the existing Vibrato and Tremolo LFOs
// (those remain hard-wired to pitch / volume via the IMU mapping system).
// It can be routed to one destination at a time.
enum class LfoWave : uint8_t { SINE, TRIANGLE, SAWTOOTH, SQUARE };
enum class LfoTarget : uint8_t { NONE, PITCH, VOLUME, TIMBRE, FILTER, PWM, TARGET_COUNT };

struct LfoParams {
    LfoWave   wave   = LfoWave::SINE;
    float     rateHz = 2.0f;   // 0.1 - 20 Hz
    float     depth  = 0.0f;   // 0.0 - 1.0
    LfoTarget target = LfoTarget::NONE;
} lfo;

double lfoPhase = 0.0;

constexpr float LFO_RATE_MIN = 0.1f;
constexpr float LFO_RATE_MAX = 20.0f;
constexpr float LFO_PITCH_MAX_CENTS = 1200.0f; // +/- 1 octave at full depth
constexpr float LFO_FILTER_MAX_HZ   = 3900.0f; // matches filter envelope range
constexpr float LFO_TIMBRE_MAX      = 3.0f;    // full morph range at full depth
constexpr float LFO_PWM_MAX         = 0.4f;    // +/- offset from current PWM width

const char *lfoWaveName(LfoWave w){
    switch(w){
        case LfoWave::SINE:     return "Sine";
        case LfoWave::TRIANGLE: return "Tri";
        case LfoWave::SAWTOOTH: return "Saw";
        case LfoWave::SQUARE:   return "Square";
        default:                return "?";
    }
}

const char *lfoTargetName(LfoTarget t){
    switch(t){
        case LfoTarget::NONE:   return "None";
        case LfoTarget::PITCH:  return "Pitch";
        case LfoTarget::VOLUME: return "Volume";
        case LfoTarget::TIMBRE: return "Timbre";
        case LfoTarget::FILTER: return "Filter";
        case LfoTarget::PWM:    return "PWM";
        default:                return "?";
    }
}

// Samples one of the existing wavetables, normalized to roughly -1..+1.
// Reusing the oscillator tables keeps the LFO shapes consistent with
// the VCO waveform morph and avoids allocating separate tables.
float lfoTableSample(LfoWave w,int idx){
    switch(w){
        case LfoWave::SINE:     return sineTable[idx]/SINE_AMP;
        case LfoWave::TRIANGLE: return triangleTable[idx]/TRIANGLE_AMP;
        case LfoWave::SAWTOOTH: return sawtoothTable[idx]/SAWTOOTH_AMP;
        case LfoWave::SQUARE:   return squareTable[idx]/SQUARE_AMP;
        default:                return 0.f;
    }
}

// ---------------------------------------------------------
// App mode
// ---------------------------------------------------------
enum class AppMode : uint8_t { PLAY, VCO, VCF, VCA, LFO, SETTINGS, PATCH };
AppMode appMode = AppMode::PLAY;
bool prevTabPressed = false;
bool prevMenuUpPressed=false,prevMenuDownPressed=false;
bool prevMenuIncPressed=false,prevMenuDecPressed=false;

// ---------------------------------------------------------
// Setting item
// ---------------------------------------------------------
struct SettingItem {
    const char *name;
    void (*onIncrement)();
    void (*onDecrement)();
    const char *(*valueLabel)();
};

bool saveSettings(); // forward declaration

// ==========================================================
// Biquad filter
// ==========================================================
void updateFilterCoefficients() {
    float cut=constrain(filterParams.cutoffHz,FILTER_CUTOFF_MIN,SAMPLE_RATE*0.45f);
    float Q=constrain(filterParams.resonanceQ,FILTER_Q_MIN,FILTER_Q_MAX);
    float omega=2.0f*PI*cut/SAMPLE_RATE;
    float sinW=sinf(omega),cosW=cosf(omega),alpha=sinW/(2.0f*Q);
    float b0,b1,b2,a0,a1,a2;
    switch(filterParams.type){
        case FilterType::LPF:  b0=(1-cosW)/2;b1=1-cosW;b2=(1-cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::HPF:  b0=(1+cosW)/2;b1=-(1+cosW);b2=(1+cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::BPF:  b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NOTCH:b0=1;b1=-2*cosW;b2=1;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NONE: b0=1;b1=b2=0;a0=1;a1=a2=0;break; // bypass
        default:               b0=1;b1=b2=0;a0=1;a1=a2=0;break;
    }
    filterB0=b0/a0;filterB1=b1/a0;filterB2=b2/a0;
    filterA1=a1/a0;filterA2=a2/a0;
}

int16_t applyFilter(int16_t in){
    float x0=(float)in;
    float y0=filterB0*x0+filterB1*filterX1+filterB2*filterX2-filterA1*filterY1-filterA2*filterY2;
    filterX2=filterX1;filterX1=x0;filterY2=filterY1;filterY1=y0;
    return (int16_t)constrain(y0,-32768.f,32767.f);
}

// ==========================================================
// Wavetable
// ==========================================================
void buildWaveTables(){
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float t=float(i)/WAVE_TABLE_SIZE,rad=2*PI*t;
        sineTable[i]    =(int16_t)(sinf(rad)*SINE_AMP);
        float tri=(t<0.5f)?(4*t-1):(3-4*t);
        triangleTable[i]=(int16_t)(tri*TRIANGLE_AMP);
        sawtoothTable[i]=(int16_t)((2*t-1)*SAWTOOTH_AMP);
        squareTable[i]  =(int16_t)(((t<0.5f)?1.f:-1.f)*SQUARE_AMP);
    }
}

int16_t getMorphedSample(int idx,float morph,float pwmWidth){
    morph=constrain(morph,0.f,3.f);
    float pwmBlend=constrain((morph-2.f)/1.f,0.f,1.f);
    int16_t *tbl[4]={sineTable,triangleTable,sawtoothTable,squareTable};
    int lo=constrain((int)morph,0,2);
    float frac=morph-(float)lo;
    int16_t base=(int16_t)(tbl[lo][idx]*(1-frac)+tbl[lo+1][idx]*frac);
    if(pwmBlend<0.001f) return base;
    float pw=constrain(pwmWidth,0.1f,0.9f);
    float t=(float)idx/WAVE_TABLE_SIZE;
    int16_t pwm=(int16_t)((t<pw?SQUARE_AMP:-SQUARE_AMP));
    return (int16_t)(base*(1-pwmBlend)+pwm*pwmBlend);
}

// Simple white noise via LCG (Linear Congruential Generator)
uint32_t noiseSeed = 12345;
int16_t nextNoise(){
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (int16_t)(noiseSeed >> 16);
}

int16_t applyBitcrush(int16_t s,float crush){
    if(crush<=0.f) return s;
    float bits=16.f-crush*13.f,levels=powf(2.f,bits);
    return (int16_t)(roundf(s/32768.f*levels)/levels*32768.f);
}

// ==========================================================
// ADSR
// ==========================================================
void advanceEnvelope(bool keyHeld){
    const float dt=(float)1024/SAMPLE_RATE;
    switch(envPhase){
        case EnvPhase::IDLE:    envLevel=0;playingFreq=0;break;
        case EnvPhase::ATTACK:
            playingFreq=(noteHeld&&heldFreq>0)?heldFreq:currentFreq;
            if(adsr.attackTime<=0){envLevel=1;envPhase=EnvPhase::DECAY;}
            else{envLevel+=dt/adsr.attackTime;if(envLevel>=1){envLevel=1;envPhase=EnvPhase::DECAY;}}
            if(!keyHeld)envPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::DECAY:
            if(adsr.decayTime<=0){envLevel=adsr.sustainLevel;envPhase=EnvPhase::SUSTAIN;}
            else{envLevel-=dt/adsr.decayTime*(1-adsr.sustainLevel);if(envLevel<=adsr.sustainLevel){envLevel=adsr.sustainLevel;envPhase=EnvPhase::SUSTAIN;}}
            if(!keyHeld)envPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::SUSTAIN:
            envLevel=adsr.sustainLevel;
            if(!keyHeld)envPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::RELEASE:
            if(adsr.releaseTime<=0){envLevel=0;envPhase=EnvPhase::IDLE;playingFreq=0;}
            else{envLevel-=dt/adsr.releaseTime*adsr.sustainLevel;if(envLevel<=0){envLevel=0;envPhase=EnvPhase::IDLE;playingFreq=0;}}
            if(keyHeld&&currentFreq>0){envPhase=EnvPhase::ATTACK;playingFreq=currentFreq;}
            break;
    }

    // Filter envelope: follows the same gate as the main envelope
    switch(filterEnvPhase){
        case EnvPhase::IDLE:    filterEnvLevel=0;break;
        case EnvPhase::ATTACK:
            if(filterEnv.attackTime<=0){filterEnvLevel=1;filterEnvPhase=EnvPhase::DECAY;}
            else{filterEnvLevel+=dt/filterEnv.attackTime;if(filterEnvLevel>=1){filterEnvLevel=1;filterEnvPhase=EnvPhase::DECAY;}}
            if(!keyHeld)filterEnvPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::DECAY:
            if(filterEnv.decayTime<=0){filterEnvLevel=filterEnv.sustainLvl;filterEnvPhase=EnvPhase::SUSTAIN;}
            else{filterEnvLevel-=dt/filterEnv.decayTime*(1-filterEnv.sustainLvl);if(filterEnvLevel<=filterEnv.sustainLvl){filterEnvLevel=filterEnv.sustainLvl;filterEnvPhase=EnvPhase::SUSTAIN;}}
            if(!keyHeld)filterEnvPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::SUSTAIN:
            filterEnvLevel=filterEnv.sustainLvl;
            if(!keyHeld)filterEnvPhase=EnvPhase::RELEASE;
            break;
        case EnvPhase::RELEASE:
            if(filterEnv.releaseTime<=0){filterEnvLevel=0;filterEnvPhase=EnvPhase::IDLE;}
            else{filterEnvLevel-=dt/filterEnv.releaseTime*filterEnv.sustainLvl;if(filterEnvLevel<=0){filterEnvLevel=0;filterEnvPhase=EnvPhase::IDLE;}}
            if(keyHeld&&currentFreq>0)filterEnvPhase=EnvPhase::ATTACK;
            break;
    }
}

// ==========================================================
// Audio task (Core 1)
// ==========================================================
void audioTask(void *pvParameters){
    const int BUF=1024;
    static int16_t bufs[3][1024];
    int bi=0;
    constexpr int CH=0;
    constexpr float SM=0.0008f;

    while(true){
        int16_t *buf=bufs[bi];bi=(bi+1)%3;
        bool keyHeld=(currentFreq>0)||noteHeld;
        advanceEnvelope(keyHeld);

        if(envPhase==EnvPhase::IDLE){
            vTaskDelay(5/portTICK_PERIOD_MS);
            phase=0;continue;
        }

        // Portamento: smoothly move portaFreq toward target
        float targetFreq=(noteHeld&&heldFreq>0)?heldFreq:currentFreq;
        if(portaEnabled&&portaFreq>0&&targetFreq>0){
            portaFreq+=(targetFreq-portaFreq)*portaSpeed;
            if(fabsf(portaFreq-targetFreq)<0.1f)portaFreq=targetFreq;
        } else {
            portaFreq=targetFreq;
        }

        // Dynamic filter cutoff: base + key tracking + filter env + IMU offset + LFO
        {
            float playF=(portaEnabled&&portaFreq>0)?portaFreq:playingFreq;
            // Key tracking: higher notes raise cutoff
            float trackHz = (playF>0)
                ? filterParams.keyTracking * (12.0f*log2f(playF/261.63f)) * 100.0f
                : 0.0f;
            // Filter envelope modulation
            float envHz = filterEnvLevel * filterEnv.depth;
            // IMU offset (multiplicative, lowers cutoff)
            float imuScale = (params.filterCutoffOffset>0.0001f)
                ? (1.0f - params.filterCutoffOffset*0.9f)
                : 1.0f;
            // General LFO -> filter cutoff (sampled once per buffer; the
            // biquad coefficients are only recalculated at this rate anyway)
            float lfoHz = 0.0f;
            if(lfo.target==LfoTarget::FILTER){
                int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
                lfoHz = lfoTableSample(lfo.wave,li)*lfo.depth*LFO_FILTER_MAX_HZ;
            }
            float dynCutoff = constrain(
                (filterParams.cutoffHz + trackHz + envHz + lfoHz) * imuScale,
                FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX);
            float saved=filterParams.cutoffHz;
            filterParams.cutoffHz=dynCutoff;
            updateFilterCoefficients();
            filterParams.cutoffHz=saved;
        }

        for(int i=0;i<BUF;i++){
            params.timbreMorph        +=(params.timbreMorphTarget       -params.timbreMorph)       *SM;
            params.vibratoDepth       +=(params.vibratoDepthTarget      -params.vibratoDepth)      *SM;
            params.vibratoRateHz      +=(params.vibratoRateHzTarget     -params.vibratoRateHz)     *SM;
            params.tremoloDepth       +=(params.tremoloDepthTarget      -params.tremoloDepth)      *SM;
            params.volumeOffset       +=(params.volumeOffsetTarget      -params.volumeOffset)      *SM;
            params.pitchBendCents     +=(params.pitchBendCentsTarget    -params.pitchBendCents)    *SM;
            params.bitcrush           +=(params.bitcrushTarget          -params.bitcrush)          *SM;
            params.filterCutoffOffset +=(params.filterCutoffOffsetTarget-params.filterCutoffOffset)*SM;

            // Vibrato LFO
            int vi=((int)vibratoPhase)%WAVE_TABLE_SIZE;if(vi<0)vi+=WAVE_TABLE_SIZE;
            float vlfo=sineTable[vi]/32000.f;
            vibratoPhase+=(double)WAVE_TABLE_SIZE*params.vibratoRateHz/SAMPLE_RATE;
            if(vibratoPhase>=WAVE_TABLE_SIZE)vibratoPhase-=WAVE_TABLE_SIZE;

            // Tremolo LFO
            int ti=((int)tremoloPhase)%WAVE_TABLE_SIZE;if(ti<0)ti+=WAVE_TABLE_SIZE;
            float tlfo=(sineTable[ti]/32000.f+1.f)*0.5f;
            tremoloPhase+=(double)WAVE_TABLE_SIZE*5.f/SAMPLE_RATE;
            if(tremoloPhase>=WAVE_TABLE_SIZE)tremoloPhase-=WAVE_TABLE_SIZE;

            // General-purpose LFO (independent from vibrato/tremolo above).
            // Always runs so the LFO tab's live phase marker stays accurate,
            // but only affects audio when routed to a target below.
            int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
            float lfoRaw=lfoTableSample(lfo.wave,li);
            lfoPhase+=(double)WAVE_TABLE_SIZE*lfo.rateHz/SAMPLE_RATE;
            if(lfoPhase>=WAVE_TABLE_SIZE)lfoPhase-=WAVE_TABLE_SIZE;
            float lfoVal=lfoRaw*lfo.depth;

            // Key bend smoothing
            float bd=keyBendGoal-keyBendCurrent;
            float bs=(fabsf(keyBendGoal)<fabsf(keyBendCurrent)||keyBendGoal==0)?keyBendReleaseSmooth:keyBendAttackSmooth;
            keyBendCurrent+=bd*bs;

            // Total pitch
            float lfoPitchCents=(lfo.target==LfoTarget::PITCH)?lfoVal*LFO_PITCH_MAX_CENTS:0.f;
            float totalCents=vlfo*params.vibratoDepth*VIBRATO_MAX_CENTS
                            +params.pitchBendCents+keyBendCurrent
                            +params.detuneCents+params.fineTuneCents
                            +lfoPitchCents;
            float pr=powf(2.f,totalCents/1200.f);
            float playF=(portaEnabled&&portaFreq>0)?portaFreq:playingFreq;
            double phInc=(double)WAVE_TABLE_SIZE*(playF*pr)/SAMPLE_RATE;

            int idx=((int)phase)%WAVE_TABLE_SIZE;if(idx<0)idx+=WAVE_TABLE_SIZE;

            // General LFO -> Timbre / PWM (applied locally, doesn't touch
            // the stored params so the VCO menu values stay untouched)
            float modMorph=params.timbreMorph;
            if(lfo.target==LfoTarget::TIMBRE)
                modMorph=constrain(modMorph+lfoVal*LFO_TIMBRE_MAX,0.f,3.f);
            float modPwm=params.pwmWidth;
            if(lfo.target==LfoTarget::PWM)
                modPwm=constrain(modPwm+lfoVal*LFO_PWM_MAX,0.1f,0.9f);

            // Main oscillator
            int16_t sample=getMorphedSample(idx,modMorph,modPwm);

            // Sub oscillator (sine wave, 1 or 2 octaves below)
            if(params.subOscLevel>0.001f && playF>0){
                float subFreq=playF*powf(2.f,(float)params.subOscOctave);
                int subIdx=(int)((double)idx*(subFreq/(playF*pr)))%WAVE_TABLE_SIZE;
                if(subIdx<0)subIdx+=WAVE_TABLE_SIZE;
                int16_t sub=sineTable[subIdx];
                sample=(int16_t)(sample*(1.f-params.subOscLevel)+sub*params.subOscLevel);
            }

            // Noise blend
            if(params.noiseLevel>0.001f){
                int16_t noise=nextNoise();
                sample=(int16_t)(sample*(1.f-params.noiseLevel)+noise*params.noiseLevel);
            }

            phase+=phInc;
            if(phase>=WAVE_TABLE_SIZE)phase-=WAVE_TABLE_SIZE;

            sample=applyBitcrush(sample,params.bitcrush);
            sample=applyFilter(sample);

            float vol=constrain(params.keyVolume+params.volumeOffset,0.f,1.f);
            float tg=constrain(1-params.tremoloDepth+params.tremoloDepth*tlfo*2,0.f,2.f);
            float lfoVolMult=(lfo.target==LfoTarget::VOLUME)?constrain(1.0f+lfoVal,0.f,2.f):1.0f;
            buf[i]=(int16_t)(sample*vol*tg*envLevel*lfoVolMult);
        }
        M5Cardputer.Speaker.playRaw(buf,BUF,SAMPLE_RATE,false,1,CH,false);
    }
}

// ==========================================================
// IMU
// ==========================================================
float lastAccelX=0,lastAccelY=0;
float imuXLastNorm=0.f, imuYLastNorm=0.f; // last normalized value applied per axis (for save/restore while held)

void applyImuValue(ImuTarget target,float value){
    switch(target){
        case ImuTarget::TIMBRE:        params.timbreMorphTarget=value*3.f;break;
        case ImuTarget::VIBRATO_DEPTH: params.vibratoDepthTarget=value;break;
        case ImuTarget::VIBRATO_RATE:  params.vibratoRateHzTarget=1+value*9;break;
        case ImuTarget::TREMOLO:       params.tremoloDepthTarget=value;break;
        case ImuTarget::VOLUME:        params.volumeOffsetTarget=value*0.5f;break;
        case ImuTarget::PITCH_BEND:    params.pitchBendCentsTarget=value*keyBendMaxCents;break;
        case ImuTarget::BEND_UP:       params.pitchBendCentsTarget=fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BEND_DOWN:     params.pitchBendCentsTarget=-fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BITCRUSH:      params.bitcrushTarget=value;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffsetTarget=value;break;
        default:break;
    }
}

void resetParamToDefault(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        params.timbreMorph=params.timbreMorphTarget=0;break;
        case ImuTarget::VIBRATO_DEPTH: params.vibratoDepth=params.vibratoDepthTarget=0;break;
        case ImuTarget::VIBRATO_RATE:  params.vibratoRateHz=params.vibratoRateHzTarget=5;break;
        case ImuTarget::TREMOLO:       params.tremoloDepth=params.tremoloDepthTarget=0;break;
        case ImuTarget::VOLUME:        params.volumeOffset=params.volumeOffsetTarget=0;break;
        case ImuTarget::PITCH_BEND:
        case ImuTarget::BEND_UP:
        case ImuTarget::BEND_DOWN:     params.pitchBendCents=params.pitchBendCentsTarget=0;break;
        case ImuTarget::BITCRUSH:      params.bitcrush=params.bitcrushTarget=0;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffset=params.filterCutoffOffsetTarget=0;break;
        default:break;
    }
}

void updateImu(){
    if(!M5.Imu.update())return;
    auto data=M5.Imu.getImuData();
    lastAccelX=data.accel.x;lastAccelY=data.accel.y;
    auto clamp1=[](float v){return constrain(v,-1.f,1.f);};
    float aX=asinf(clamp1(lastAccelX))*180/PI;
    float aY=asinf(clamp1(lastAccelY))*180/PI;
    auto biAuto=[](ImuTarget t,bool cfg){
        if(t==ImuTarget::PITCH_BEND)return true;
        if(t==ImuTarget::BEND_UP||t==ImuTarget::BEND_DOWN)return false;
        return cfg;
    };
    if(imuAxisX.target!=ImuTarget::NONE&&!imuXHeld){
        float n=constrain((aX/TILT_MAX_DEGREES)*imuAxisX.sensitivity,-1.f,1.f);
        float applied=biAuto(imuAxisX.target,imuAxisX.bipolar)?n:fabsf(n);
        imuXLastNorm=applied;
        applyImuValue(imuAxisX.target,applied);
    }
    if(imuAxisY.target!=ImuTarget::NONE&&!imuYHeld){
        float n=constrain((aY/TILT_MAX_DEGREES)*imuAxisY.sensitivity,-1.f,1.f);
        float applied=biAuto(imuAxisY.target,imuAxisY.bipolar)?n:fabsf(n);
        imuYLastNorm=applied;
        applyImuValue(imuAxisY.target,applied);
    }
}

const char *imuTargetName(ImuTarget t){
    switch(t){
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
        case ImuTarget::FILTER_CUTOFF:{
            static char buf[16];
            snprintf(buf,sizeof(buf),"Flt(%s)",filterTypeName(filterParams.type));
            return buf;
        }
        default:return "?";
    }
}

// ==========================================================
// Key helpers
// ==========================================================
float resolveFreqFromKeys(){
    auto s=M5Cardputer.Keyboard.keysState();
    for(auto it=s.word.rbegin();it!=s.word.rend();++it)
        for(int i=0;i<NUM_KEYS;i++)
            if(keyNotes[i].key==*it)
                return keyNotes[i].freq*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    return 0.f;
}

void updateOctaveAndVolume(){
    if(appMode!=AppMode::PLAY)return;
    auto s=M5Cardputer.Keyboard.keysState();
    bool oU=false,oD=false,vU=false,vD=false,bD=false,bU=false;
    bool iXH=false,iYH=false,nH=false,pOn=false,hKey=false;
    bool trU=false,trD=false;
    for(char c:s.word){
        if(c=='=')oU=true;  if(c=='-')oD=true;
        if(c==']')trU=true; if(c=='[')trD=true;
        if(c=='.')vU=true;  if(c==',')vD=true;
        if(c=='z')bD=true;  if(c=='x')bU=true;
        if(c=='a')iXH=true; if(c=='s')iYH=true;
        if(c=='d')nH=true;  if(c=='c')pOn=true;
        if(c=='h')hKey=true;
    }

    // Octave (edge-triggered)
    if(oU&&!prevOctaveUpPressed   &&params.octaveShift<2) params.octaveShift++;
    if(oD&&!prevOctaveDownPressed &&params.octaveShift>-2)params.octaveShift--;
    prevOctaveUpPressed=oU;prevOctaveDownPressed=oD;

    // Transpose (edge-triggered), keys directly below the octave keys
    if(trU&&!prevTransposeUpPressed  &&transposeSemitones<TRANSPOSE_MAX)transposeSemitones++;
    if(trD&&!prevTransposeDownPressed&&transposeSemitones>TRANSPOSE_MIN)transposeSemitones--;
    prevTransposeUpPressed=trU;prevTransposeDownPressed=trD;


    // Volume (edge-triggered)
    if(vU&&!prevVolumeUpPressed)  params.keyVolume=min(params.keyVolume+0.05f,1.f);
    if(vD&&!prevVolumeDownPressed)params.keyVolume=max(params.keyVolume-0.05f,0.f);
    prevVolumeUpPressed=vU;prevVolumeDownPressed=vD;

    // Bend
    if(bD&&!bU)keyBendGoal=-keyBendMaxCents;
    else if(bU&&!bD)keyBendGoal=+keyBendMaxCents;
    else keyBendGoal=0;

    // IMU X hold
    if(iXH&&!prevImuXHoldPressed)imuXHeld=!imuXHeld;
    prevImuXHoldPressed=iXH;

    // IMU Y hold
    if(iYH&&!prevImuYHoldPressed)imuYHeld=!imuYHeld;
    prevImuYHoldPressed=iYH;

    // Note hold
    if(nH&&!prevNoteHoldPressed){
        noteHeld=!noteHeld;
        if(noteHeld)heldFreq=(playingFreq>0)?playingFreq:currentFreq;
        else{heldFreq=0;if(currentFreq==0)envPhase=EnvPhase::RELEASE;}
    }
    prevNoteHoldPressed=nH;

    // Portamento toggle (C key)
    if(pOn&&!prevPortaPressed){
        portaEnabled=!portaEnabled;
        if(!portaEnabled)portaFreq=0;
    }
    prevPortaPressed=pOn;

    // Help overlay (H key: show while held, hide on release)
    helpVisible=hKey;
}

// ==========================================================
// SD card
// ==========================================================
bool initSDCard(){
    SPI.begin(SD_SPI_SCK_PIN,SD_SPI_MISO_PIN,SD_SPI_MOSI_PIN,SD_SPI_CS_PIN);
    return SD.begin(SD_SPI_CS_PIN,SPI,25000000);
}
bool ensureCpsFolder(){return SD.exists(CPS_FOLDER_PATH)||SD.mkdir(CPS_FOLDER_PATH);}

static const char *SETTINGS_FILE_PATH="/CPS/settings.json";

const char *filterTypeName(FilterType t){
    switch(t){
        case FilterType::LPF:  return "LPF";
        case FilterType::HPF:  return "HPF";
        case FilterType::BPF:  return "BPF";
        case FilterType::NOTCH:return "Notch";
        case FilterType::NONE: return "None";
        default:               return "?";
    }
}

bool saveSettingsToFile(const char *path){
    File f=SD.open(path,FILE_WRITE);
    if(!f){Serial.println("[Settings] open failed");return false;}
    f.println("{");
    f.printf("  \"imu_x_target\": %u,\n",(unsigned)imuAxisX.target);
    f.printf("  \"imu_y_target\": %u,\n",(unsigned)imuAxisY.target);
    f.printf("  \"imu_x_held\": %d,\n",(int)imuXHeld);
    f.printf("  \"imu_x_held_norm\": %.4f,\n",imuXLastNorm);
    f.printf("  \"imu_y_held\": %d,\n",(int)imuYHeld);
    f.printf("  \"imu_y_held_norm\": %.4f,\n",imuYLastNorm);
    f.printf("  \"bend_max_cents\": %.2f,\n",keyBendMaxCents);
    f.printf("  \"bend_attack\": %.6f,\n",keyBendAttackSmooth);
    f.printf("  \"bend_release\": %.6f,\n",keyBendReleaseSmooth);
    f.printf("  \"porta_enabled\": %d,\n",(int)portaEnabled);
    f.printf("  \"porta_speed\": %.6f,\n",portaSpeed);
    f.printf("  \"adsr_attack\": %.3f,\n",adsr.attackTime);
    f.printf("  \"adsr_decay\": %.3f,\n",adsr.decayTime);
    f.printf("  \"adsr_sustain\": %.3f,\n",adsr.sustainLevel);
    f.printf("  \"adsr_release\": %.3f,\n",adsr.releaseTime);
    f.printf("  \"filter_type\": %u,\n",(unsigned)filterParams.type);
    f.printf("  \"filter_cutoff\": %.1f,\n",filterParams.cutoffHz);
    f.printf("  \"filter_q\": %.2f,\n",filterParams.resonanceQ);
    f.printf("  \"filter_tracking\": %.2f,\n",filterParams.keyTracking);
    f.printf("  \"fenv_depth\": %.1f,\n",filterEnv.depth);
    f.printf("  \"fenv_attack\": %.3f,\n",filterEnv.attackTime);
    f.printf("  \"fenv_decay\": %.3f,\n",filterEnv.decayTime);
    f.printf("  \"fenv_sustain\": %.3f,\n",filterEnv.sustainLvl);
    f.printf("  \"fenv_release\": %.3f,\n",filterEnv.releaseTime);
    f.printf("  \"vco_timbre\": %.2f,\n",params.timbreMorph);
    f.printf("  \"vco_pwm\": %.2f,\n",params.pwmWidth);
    f.printf("  \"vco_detune\": %.1f,\n",params.detuneCents);
    f.printf("  \"vco_fine\": %.1f,\n",params.fineTuneCents);
    f.printf("  \"vco_sub_level\": %.2f,\n",params.subOscLevel);
    f.printf("  \"vco_sub_oct\": %d,\n",params.subOscOctave);
    f.printf("  \"vco_noise\": %.2f,\n",params.noiseLevel);
    f.printf("  \"lfo_wave\": %u,\n",(unsigned)lfo.wave);
    f.printf("  \"lfo_rate\": %.3f,\n",lfo.rateHz);
    f.printf("  \"lfo_depth\": %.3f,\n",lfo.depth);
    f.printf("  \"lfo_target\": %u\n",(unsigned)lfo.target);
    f.println("}");
    f.close();
    Serial.printf("[Settings] saved to %s\n",path);
    return true;
}
bool saveSettings(){return saveSettingsToFile(SETTINGS_FILE_PATH);}

void parseSettingLine(const String &line){
    int q1=line.indexOf('"');if(q1<0)return;
    int q2=line.indexOf('"',q1+1);if(q2<0)return;
    String key=line.substring(q1+1,q2);
    int col=line.indexOf(':',q2);if(col<0)return;
    String vs=line.substring(col+1);vs.trim();
    if(vs.endsWith(","))vs.remove(vs.length()-1);vs.trim();
    if(!vs.length())return;
    float v=vs.toFloat();
    if(key=="imu_x_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)imuAxisX.target=(ImuTarget)u;}
    else if(key=="imu_y_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)imuAxisY.target=(ImuTarget)u;}
    else if(key=="imu_x_held")imuXHeld=(bool)(int)v;
    else if(key=="imu_x_held_norm")imuXLastNorm=v;
    else if(key=="imu_y_held")imuYHeld=(bool)(int)v;
    else if(key=="imu_y_held_norm")imuYLastNorm=v;
    else if(key=="bend_max_cents")keyBendMaxCents=v;
    else if(key=="bend_attack")keyBendAttackSmooth=v;
    else if(key=="bend_release")keyBendReleaseSmooth=v;
    else if(key=="porta_enabled")portaEnabled=(bool)(int)v;
    else if(key=="porta_speed")portaSpeed=v;
    else if(key=="adsr_attack")adsr.attackTime=v;
    else if(key=="adsr_decay")adsr.decayTime=v;
    else if(key=="adsr_sustain")adsr.sustainLevel=v;
    else if(key=="adsr_release")adsr.releaseTime=v;
    else if(key=="filter_type"){uint8_t u=(uint8_t)v;if(u<=(uint8_t)FilterType::NONE)filterParams.type=(FilterType)u;}
    else if(key=="filter_cutoff")filterParams.cutoffHz=v;
    else if(key=="filter_q")filterParams.resonanceQ=v;
    else if(key=="filter_tracking")filterParams.keyTracking=constrain(v,0.f,1.f);
    else if(key=="fenv_depth")filterEnv.depth=v;
    else if(key=="fenv_attack")filterEnv.attackTime=v;
    else if(key=="fenv_decay")filterEnv.decayTime=v;
    else if(key=="fenv_sustain")filterEnv.sustainLvl=v;
    else if(key=="fenv_release")filterEnv.releaseTime=v;
    else if(key=="vco_timbre"){params.timbreMorph=params.timbreMorphTarget=v;}
    else if(key=="vco_pwm")params.pwmWidth=constrain(v,0.1f,0.9f);
    else if(key=="vco_detune")params.detuneCents=constrain(v,-50.f,50.f);
    else if(key=="vco_fine")params.fineTuneCents=constrain(v,-100.f,100.f);
    else if(key=="vco_sub_level")params.subOscLevel=constrain(v,0.f,1.f);
    else if(key=="vco_sub_oct")params.subOscOctave=constrain((int)v,-2,-1);
    else if(key=="vco_noise")params.noiseLevel=constrain(v,0.f,1.f);
    else if(key=="lfo_wave"){uint8_t u=(uint8_t)v;if(u<4)lfo.wave=(LfoWave)u;}
    else if(key=="lfo_rate")lfo.rateHz=constrain(v,LFO_RATE_MIN,LFO_RATE_MAX);
    else if(key=="lfo_depth")lfo.depth=constrain(v,0.f,1.f);
    else if(key=="lfo_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)LfoTarget::TARGET_COUNT)lfo.target=(LfoTarget)u;}
}

bool loadSettingsFromFile(const char *path){
    if(!SD.exists(path)){Serial.println("[Settings] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    while(f.available())parseSettingLine(f.readStringUntil('\n'));
    f.close();
    // Held IMU axes don't get live tilt updates, so reconstruct whatever
    // target value was frozen at save time from the stored normalized input.
    if(imuXHeld&&imuAxisX.target!=ImuTarget::NONE)applyImuValue(imuAxisX.target,imuXLastNorm);
    if(imuYHeld&&imuAxisY.target!=ImuTarget::NONE)applyImuValue(imuAxisY.target,imuYLastNorm);
    updateFilterCoefficients();
    Serial.printf("[Settings] loaded from %s\n",path);
    return true;
}
bool loadSettings(){return loadSettingsFromFile(SETTINGS_FILE_PATH);}

// ==========================================================
// Patch Bank
// ==========================================================
// Patches are stored as individual settings.json-style files under
// /CPS/Patch/<name>.json. The app never lets the user browse outside
// this folder, by design (see PATCH_FOLDER_PATH usage below).
static const char *PATCH_FOLDER_PATH = "/CPS/Patch";
constexpr int PATCH_NAME_MAX_LEN = 20;
constexpr int MAX_PATCHES = 32;

bool ensurePatchFolder(){return SD.exists(PATCH_FOLDER_PATH)||SD.mkdir(PATCH_FOLDER_PATH);}

enum class PatchMode   : uint8_t { LOAD, SAVE };
enum class PatchUiState: uint8_t { BROWSE, NAME_ENTRY, CONFIRM_DELETE, CONFIRM_OVERWRITE };

PatchMode    patchMode      = PatchMode::LOAD;
PatchUiState patchUiState   = PatchUiState::BROWSE;
String       patchNames[MAX_PATCHES];
int          patchCount     = 0;
int          selectedPatchIndex = 0;
int          patchActionIndex   = -1;   // index into patchNames[] being renamed/duplicated/deleted/overwritten
String       patchNameBuffer;
bool         patchRenaming    = false;
bool         patchDuplicating = false;

bool prevPatchUpPressed=false, prevPatchDownPressed=false;
bool prevPatchConfirmPressed=false, prevPatchTabPressed=false;
bool prevPatchRenamePressed=false, prevPatchDupPressed=false, prevPatchDeleteKeyPressed=false;
bool prevPatchEnterPressed=false, prevPatchDelPressed=false;
std::vector<char> prevPatchTypedWord;

bool isValidPatchChar(char c){
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==' ';
}

String patchFilePath(const String &name){return String(PATCH_FOLDER_PATH)+"/"+name+".json";}

void scanPatches(){
    patchCount=0;
    File dir=SD.open(PATCH_FOLDER_PATH);
    if(!dir)return;
    File f=dir.openNextFile();
    while(f){
        if(!f.isDirectory()){
            String n=String(f.name());
            int slash=n.lastIndexOf('/');if(slash>=0)n=n.substring(slash+1);
            if(n.endsWith(".json")&&patchCount<MAX_PATCHES)
                patchNames[patchCount++]=n.substring(0,n.length()-5);
        }
        f=dir.openNextFile();
    }
    dir.close();
    // Alphabetical sort (small N, simple insertion-style bubble sort is fine)
    for(int i=0;i<patchCount-1;i++)
        for(int j=0;j<patchCount-1-i;j++)
            if(patchNames[j]>patchNames[j+1]){String t=patchNames[j];patchNames[j]=patchNames[j+1];patchNames[j+1]=t;}
}

// Number of selectable rows in the browse list. SAVE mode has an extra
// "<New Patch>" placeholder row at the top.
int patchListCount(){return patchCount+(patchMode==PatchMode::SAVE?1:0);}
bool patchIsNewRow(int row){return patchMode==PatchMode::SAVE&&row==0;}
int  patchRowToNameIndex(int row){return patchMode==PatchMode::SAVE?row-1:row;}

void clampPatchSelection(){
    int c=patchListCount();
    if(selectedPatchIndex>=c)selectedPatchIndex=max(0,c-1);
    if(selectedPatchIndex<0)selectedPatchIndex=0;
}

void enterPatchBrowser(PatchMode mode){
    patchMode=mode;
    scanPatches();
    selectedPatchIndex=0;
    patchUiState=PatchUiState::BROWSE;
    patchRenaming=false;patchDuplicating=false;

    // Seed edge-trackers with whatever is CURRENTLY held so the very key
    // press that opened this screen (e.g. '/' or ',' on the SETTINGS menu)
    // isn't immediately re-read as a brand-new press inside the browser
    // (which previously caused an instant delete-confirm / false-select).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter,heldDelete=false,heldRename=false,heldDup=false;
    for(char c:s.word){
        if(c==';')heldUp=true;   if(c=='.')heldDown=true;
        if(c=='/')heldConfirm=true;
        if(c==',')heldDelete=true;
        if(c=='r')heldRename=true;
        if(c=='c')heldDup=true;
    }
    prevPatchUpPressed=heldUp; prevPatchDownPressed=heldDown;
    prevPatchConfirmPressed=heldConfirm; prevPatchTabPressed=s.tab;
    prevPatchRenamePressed=heldRename; prevPatchDupPressed=heldDup; prevPatchDeleteKeyPressed=heldDelete;
    prevPatchEnterPressed=s.enter; prevPatchDelPressed=s.del;
    prevPatchTypedWord=s.word;
    appMode=AppMode::PATCH;
}

void patchSaveEnter(){enterPatchBrowser(PatchMode::SAVE);}
void patchLoadEnter(){enterPatchBrowser(PatchMode::LOAD);}
const char *patchEnterLabel(){return "Select>";}

void updatePatchBrowser(){
    auto s=M5Cardputer.Keyboard.keysState();

    if(patchUiState==PatchUiState::NAME_ENTRY){
        for(char c:s.word){
            bool wasPressed=false;
            for(char p:prevPatchTypedWord)if(p==c){wasPressed=true;break;}
            if(!wasPressed&&isValidPatchChar(c)&&patchNameBuffer.length()<PATCH_NAME_MAX_LEN)
                patchNameBuffer+=c;
        }
        prevPatchTypedWord=s.word;

        if(s.del&&!prevPatchDelPressed&&patchNameBuffer.length()>0)
            patchNameBuffer.remove(patchNameBuffer.length()-1);
        prevPatchDelPressed=s.del;

        if(s.enter&&!prevPatchEnterPressed){
            String trimmed=patchNameBuffer;trimmed.trim();
            if(trimmed.length()>0){
                if(patchRenaming){
                    SD.rename(patchFilePath(patchNames[patchActionIndex]).c_str(),patchFilePath(trimmed).c_str());
                } else if(patchDuplicating){
                    File src=SD.open(patchFilePath(patchNames[patchActionIndex]).c_str(),FILE_READ);
                    File dst=SD.open(patchFilePath(trimmed).c_str(),FILE_WRITE);
                    if(src&&dst)while(src.available())dst.write(src.read());
                    if(src)src.close();
                    if(dst)dst.close();
                } else {
                    saveSettingsToFile(patchFilePath(trimmed).c_str());
                }
            }
            scanPatches();clampPatchSelection();
            patchUiState=PatchUiState::BROWSE;
            patchRenaming=false;patchDuplicating=false;
        }
        prevPatchEnterPressed=s.enter;

        if(s.tab&&!prevPatchTabPressed){
            patchUiState=PatchUiState::BROWSE;
            patchRenaming=false;patchDuplicating=false;
        }
        prevPatchTabPressed=s.tab;
        return;
    }

    if(patchUiState==PatchUiState::CONFIRM_DELETE||patchUiState==PatchUiState::CONFIRM_OVERWRITE){
        bool confirm=s.enter,cancel=s.tab;
        for(char c:s.word)if(c=='/')confirm=true;
        if(confirm&&!prevPatchConfirmPressed){
            if(patchUiState==PatchUiState::CONFIRM_DELETE){
                SD.remove(patchFilePath(patchNames[patchActionIndex]).c_str());
            } else {
                saveSettingsToFile(patchFilePath(patchNames[patchActionIndex]).c_str());
            }
            scanPatches();clampPatchSelection();
            patchUiState=PatchUiState::BROWSE;
        }
        prevPatchConfirmPressed=confirm;
        if(cancel&&!prevPatchTabPressed)patchUiState=PatchUiState::BROWSE;
        prevPatchTabPressed=cancel;
        return;
    }

    // BROWSE
    bool mU=false,mD=false,confirm=s.enter,doDelete=false,doRename=false,doDup=false,cancel=s.tab;
    for(char c:s.word){
        if(c==';')mU=true; if(c=='.')mD=true;
        if(c=='/')confirm=true;
        if(c==',')doDelete=true;
        if(c=='r')doRename=true;
        if(c=='c')doDup=true;
    }

    int count=patchListCount();
    if(mU&&!prevPatchUpPressed&&count>0)selectedPatchIndex=(selectedPatchIndex-1+count)%count;
    if(mD&&!prevPatchDownPressed&&count>0)selectedPatchIndex=(selectedPatchIndex+1)%count;
    prevPatchUpPressed=mU;prevPatchDownPressed=mD;

    if(confirm&&!prevPatchConfirmPressed&&count>0){
        if(patchIsNewRow(selectedPatchIndex)){
            patchNameBuffer="";
            patchRenaming=false;patchDuplicating=false;
            prevPatchTypedWord=s.word;
            prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
            patchUiState=PatchUiState::NAME_ENTRY;
        } else {
            int ni=patchRowToNameIndex(selectedPatchIndex);
            if(patchMode==PatchMode::LOAD){
                loadSettingsFromFile(patchFilePath(patchNames[ni]).c_str());
                appMode=AppMode::SETTINGS;
            } else {
                patchActionIndex=ni;
                patchUiState=PatchUiState::CONFIRM_OVERWRITE;
            }
        }
    }
    prevPatchConfirmPressed=confirm;

    if(doRename&&!prevPatchRenamePressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        patchNameBuffer=patchNames[patchActionIndex];
        patchRenaming=true;patchDuplicating=false;
        prevPatchTypedWord=s.word;
        prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
        patchUiState=PatchUiState::NAME_ENTRY;
    }
    prevPatchRenamePressed=doRename;

    if(doDup&&!prevPatchDupPressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        String suggested=patchNames[patchActionIndex]+"_copy";
        if(suggested.length()>PATCH_NAME_MAX_LEN)suggested=suggested.substring(0,PATCH_NAME_MAX_LEN);
        patchNameBuffer=suggested;
        patchRenaming=false;patchDuplicating=true;
        prevPatchTypedWord=s.word;
        prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
        patchUiState=PatchUiState::NAME_ENTRY;
    }
    prevPatchDupPressed=doDup;

    if(doDelete&&!prevPatchDeleteKeyPressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        patchUiState=PatchUiState::CONFIRM_DELETE;
    }
    prevPatchDeleteKeyPressed=doDelete;

    if(cancel&&!prevPatchTabPressed)appMode=AppMode::SETTINGS;
    prevPatchTabPressed=cancel;
}

// ==========================================================
// SETTING menu items
// ==========================================================
void imuXNext(){resetParamToDefault(imuAxisX.target);uint8_t v=(uint8_t)imuAxisX.target;imuAxisX.target=(ImuTarget)((v+1)%(uint8_t)ImuTarget::TARGET_COUNT);}
void imuXPrev(){resetParamToDefault(imuAxisX.target);uint8_t v=(uint8_t)imuAxisX.target;imuAxisX.target=(ImuTarget)(v==0?(uint8_t)ImuTarget::TARGET_COUNT-1:v-1);}
const char *imuXLabel(){return imuTargetName(imuAxisX.target);}
void imuYNext(){resetParamToDefault(imuAxisY.target);uint8_t v=(uint8_t)imuAxisY.target;imuAxisY.target=(ImuTarget)((v+1)%(uint8_t)ImuTarget::TARGET_COUNT);}
void imuYPrev(){resetParamToDefault(imuAxisY.target);uint8_t v=(uint8_t)imuAxisY.target;imuAxisY.target=(ImuTarget)(v==0?(uint8_t)ImuTarget::TARGET_COUNT-1:v-1);}
const char *imuYLabel(){return imuTargetName(imuAxisY.target);}

void bendWInc(){keyBendMaxCents=min(keyBendMaxCents+100.f,1200.f);}
void bendWDec(){keyBendMaxCents=max(keyBendMaxCents-100.f,0.f);}
char bwBuf[12];const char *bendWLabel(){snprintf(bwBuf,sizeof(bwBuf),"%.0fst",keyBendMaxCents/100);return bwBuf;}
void bendAInc(){keyBendAttackSmooth=min(keyBendAttackSmooth*1.3f,0.01f);}
void bendADec(){keyBendAttackSmooth=max(keyBendAttackSmooth/1.3f,0.00005f);}
char baBuf[12];const char *bendALabel(){snprintf(baBuf,sizeof(baBuf),"%.4f",keyBendAttackSmooth);return baBuf;}
void bendRInc(){keyBendReleaseSmooth=min(keyBendReleaseSmooth*1.3f,0.02f);}
void bendRDec(){keyBendReleaseSmooth=max(keyBendReleaseSmooth/1.3f,0.0005f);}
char brBuf[12];const char *bendRLabel(){snprintf(brBuf,sizeof(brBuf),"%.4f",keyBendReleaseSmooth);return brBuf;}

void portaToggle(){portaEnabled=!portaEnabled;if(!portaEnabled)portaFreq=0;}
void portaSpdInc(){portaSpeed=min(portaSpeed*1.3f,0.1f);}
void portaSpdDec(){portaSpeed=max(portaSpeed/1.3f,0.0001f);}
char ptOBuf[6];const char *portaOnOffLabel(){snprintf(ptOBuf,sizeof(ptOBuf),"%s",portaEnabled?"ON":"OFF");return ptOBuf;}
char ptSBuf[12];const char *portaSpdLabel(){snprintf(ptSBuf,sizeof(ptSBuf),"%.4f",portaSpeed);return ptSBuf;}

SettingItem settingItems[]={
    // Left column (0-3): Patch Save, Patch Load, IMU X, IMU Y
    {"PatchSv",  patchSaveEnter, patchSaveEnter, patchEnterLabel},
    {"PatchLd",  patchLoadEnter, patchLoadEnter, patchEnterLabel},
    {"IMU X",    imuXNext,    imuXPrev,    imuXLabel},
    {"IMU Y",    imuYNext,    imuYPrev,    imuYLabel},
    // Right column (4-8): Bend wid/atk/rel, Portamento, Porta spd
    {"Bend wid", bendWInc,    bendWDec,    bendWLabel},
    {"Bend atk", bendAInc,    bendADec,    bendALabel},
    {"Bend rel", bendRInc,    bendRDec,    bendRLabel},
    {"Portamento",portaToggle,portaToggle, portaOnOffLabel},
    {"Porta spd", portaSpdInc,portaSpdDec, portaSpdLabel},
};
const int NUM_SETTING_ITEMS=sizeof(settingItems)/sizeof(settingItems[0]);
constexpr int SETTINGS_SPLIT_COL=4; // first 4 items -> left column
int selectedSettingIndex=0;

// ==========================================================
// VCO menu items
// ==========================================================
void timbreInc(){params.timbreMorph=params.timbreMorphTarget=min(params.timbreMorph+0.1f,3.f);}
void timbreDec(){params.timbreMorph=params.timbreMorphTarget=max(params.timbreMorph-0.1f,0.f);}
char timBuf[16];const char *timbreLabel(){const char *s[]={"Sine","Tri","Saw","Sq"};snprintf(timBuf,sizeof(timBuf),"%s(%.1f)",s[constrain((int)params.timbreMorph,0,3)],params.timbreMorph);return timBuf;}
void pwmInc(){params.pwmWidth=min(params.pwmWidth+0.05f,0.9f);}
void pwmDec(){params.pwmWidth=max(params.pwmWidth-0.05f,0.1f);}
char pwmBuf[10];const char *pwmLabel(){snprintf(pwmBuf,sizeof(pwmBuf),"%.0f%%",params.pwmWidth*100);return pwmBuf;}
void detInc(){params.detuneCents=min(params.detuneCents+1.f,50.f);}
void detDec(){params.detuneCents=max(params.detuneCents-1.f,-50.f);}
char detBuf[10];const char *detuneLabel(){snprintf(detBuf,sizeof(detBuf),"%+.0fc",params.detuneCents);return detBuf;}
void finInc(){params.fineTuneCents=min(params.fineTuneCents+1.f,100.f);}
void finDec(){params.fineTuneCents=max(params.fineTuneCents-1.f,-100.f);}
char finBuf[10];const char *fineLabel(){snprintf(finBuf,sizeof(finBuf),"%+.0fc",params.fineTuneCents);return finBuf;}

// Sub oscillator
void subLInc(){params.subOscLevel=min(params.subOscLevel+0.05f,1.f);}
void subLDec(){params.subOscLevel=max(params.subOscLevel-0.05f,0.f);}
char subLBuf[10];const char *subLLabel(){snprintf(subLBuf,sizeof(subLBuf),"%.0f%%",params.subOscLevel*100);return subLBuf;}
void subOInc(){params.subOscOctave=min(params.subOscOctave+1,-1);}
void subODec(){params.subOscOctave=max(params.subOscOctave-1,-2);}
char subOBuf[8];const char *subOLabel(){snprintf(subOBuf,sizeof(subOBuf),"%d oct",params.subOscOctave);return subOBuf;}
// Noise blend
void noiseInc(){params.noiseLevel=min(params.noiseLevel+0.05f,1.f);}
void noiseDec(){params.noiseLevel=max(params.noiseLevel-0.05f,0.f);}
char noiseBuf[10];const char *noiseLabel(){snprintf(noiseBuf,sizeof(noiseBuf),"%.0f%%",params.noiseLevel*100);return noiseBuf;}

SettingItem vcoItems[]={
    // Left column (0-3): Timbre, PWM, Detune, FineTune
    {"Timbre",   timbreInc, timbreDec, timbreLabel},
    {"PWM",      pwmInc,    pwmDec,    pwmLabel},
    {"Detune",   detInc,    detDec,    detuneLabel},
    {"FineTune", finInc,    finDec,    fineLabel},
    // Right column (4-6): Sub, Sub Oct, Noise
    {"Sub Lvl",  subLInc,   subLDec,   subLLabel},
    {"Sub Oct",  subOInc,   subODec,   subOLabel},
    {"Noise",    noiseInc,  noiseDec,  noiseLabel},
};
const int NUM_VCO_ITEMS=sizeof(vcoItems)/sizeof(vcoItems[0]);
int selectedVcoIndex=0;

// ==========================================================
// VCF menu items
// ==========================================================
void ftNext(){uint8_t v=(uint8_t)filterParams.type;filterParams.type=(FilterType)((v+1)%5);updateFilterCoefficients();}
void ftPrev(){uint8_t v=(uint8_t)filterParams.type;filterParams.type=(FilterType)(v==0?4:v-1);updateFilterCoefficients();}
char ftBuf[8];const char *ftLabel(){snprintf(ftBuf,sizeof(ftBuf),"%s",filterTypeName(filterParams.type));return ftBuf;}
void fcInc(){filterParams.cutoffHz=min(filterParams.cutoffHz+100.f,FILTER_CUTOFF_MAX);updateFilterCoefficients();}
void fcDec(){filterParams.cutoffHz=max(filterParams.cutoffHz-100.f,FILTER_CUTOFF_MIN);updateFilterCoefficients();}
char fcBuf[12];const char *fcLabel(){snprintf(fcBuf,sizeof(fcBuf),"%.0fHz",filterParams.cutoffHz);return fcBuf;}
void fqInc(){filterParams.resonanceQ=min(filterParams.resonanceQ+0.1f,FILTER_Q_MAX);updateFilterCoefficients();}
void fqDec(){filterParams.resonanceQ=max(filterParams.resonanceQ-0.1f,FILTER_Q_MIN);updateFilterCoefficients();}
char fqBuf[10];const char *fqLabel(){snprintf(fqBuf,sizeof(fqBuf),"Q%.1f",filterParams.resonanceQ);return fqBuf;}
// Key tracking
void fktInc(){filterParams.keyTracking=min(filterParams.keyTracking+0.1f,1.f);}
void fktDec(){filterParams.keyTracking=max(filterParams.keyTracking-0.1f,0.f);}
char fktBuf[10];const char *fktLabel(){snprintf(fktBuf,sizeof(fktBuf),"%.0f%%",filterParams.keyTracking*100);return fktBuf;}
// Filter envelope
void fedInc(){filterEnv.depth=min(filterEnv.depth+100.f,3900.f);}
void fedDec(){filterEnv.depth=max(filterEnv.depth-100.f,-3900.f);}
char fedBuf[12];const char *fedLabel(){snprintf(fedBuf,sizeof(fedBuf),"%+.0fHz",filterEnv.depth);return fedBuf;}
void feaInc(){filterEnv.attackTime=min(filterEnv.attackTime+0.05f,ADSR_MAX_TIME);}
void feaDec(){filterEnv.attackTime=max(filterEnv.attackTime-0.05f,0.f);}
char feaBuf[10];const char *feaLabel(){snprintf(feaBuf,sizeof(feaBuf),"%.2fs",filterEnv.attackTime);return feaBuf;}
void feddInc(){filterEnv.decayTime=min(filterEnv.decayTime+0.05f,ADSR_MAX_TIME);}
void feddDec(){filterEnv.decayTime=max(filterEnv.decayTime-0.05f,0.f);}
char feddBuf[10];const char *feddLabel(){snprintf(feddBuf,sizeof(feddBuf),"%.2fs",filterEnv.decayTime);return feddBuf;}
void ferInc(){filterEnv.releaseTime=min(filterEnv.releaseTime+0.05f,ADSR_MAX_TIME);}
void ferDec(){filterEnv.releaseTime=max(filterEnv.releaseTime-0.05f,0.f);}
char ferBuf[10];const char *ferLabel(){snprintf(ferBuf,sizeof(ferBuf),"%.2fs",filterEnv.releaseTime);return ferBuf;}

SettingItem vcfItems[]={
    // Left column (0-3): Filter, Cutoff, Resonance, KeyTrack
    {"Filter",   ftNext,  ftPrev,  ftLabel},
    {"Cutoff",   fcInc,   fcDec,   fcLabel},
    {"Resonance",fqInc,   fqDec,   fqLabel},
    {"KeyTrack", fktInc,  fktDec,  fktLabel},
    // Right column (4-7): FEnv Depth, Atk, Dec, Rel
    {"FEnv Dep", fedInc,  fedDec,  fedLabel},
    {"FEnv Atk", feaInc,  feaDec,  feaLabel},
    {"FEnv Dec", feddInc, feddDec, feddLabel},
    {"FEnv Rel", ferInc,  ferDec,  ferLabel},
};
const int NUM_VCF_ITEMS=sizeof(vcfItems)/sizeof(vcfItems[0]);
int selectedVcfIndex=0;

// ==========================================================
// VCA menu items
// ==========================================================
char adsrBuf[12];
void aaInc(){adsr.attackTime=min(adsr.attackTime+0.05f,ADSR_MAX_TIME);}
void aaDec(){adsr.attackTime=max(adsr.attackTime-0.05f,0.f);}
const char *aaLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.attackTime);return adsrBuf;}
void adInc(){adsr.decayTime=min(adsr.decayTime+0.05f,ADSR_MAX_TIME);}
void adDec(){adsr.decayTime=max(adsr.decayTime-0.05f,0.f);}
const char *adLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.decayTime);return adsrBuf;}
void asInc(){adsr.sustainLevel=min(adsr.sustainLevel+0.05f,1.f);}
void asDec(){adsr.sustainLevel=max(adsr.sustainLevel-0.05f,0.f);}
const char *asLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%d%%",(int)(adsr.sustainLevel*100));return adsrBuf;}
void arInc(){adsr.releaseTime=min(adsr.releaseTime+0.05f,ADSR_MAX_TIME);}
void arDec(){adsr.releaseTime=max(adsr.releaseTime-0.05f,0.f);}
const char *arLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.releaseTime);return adsrBuf;}

SettingItem vcaItems[]={
    {"Attack",  aaInc,aaDec,aaLabel},
    {"Decay",   adInc,adDec,adLabel},
    {"Sustain", asInc,asDec,asLabel},
    {"Release", arInc,arDec,arLabel},
};
const int NUM_VCA_ITEMS=sizeof(vcaItems)/sizeof(vcaItems[0]);
int selectedVcaIndex=0;

// ==========================================================
// LFO menu items
// ==========================================================
void lfoWaveNext(){uint8_t v=(uint8_t)lfo.wave;lfo.wave=(LfoWave)((v+1)%4);}
void lfoWavePrev(){uint8_t v=(uint8_t)lfo.wave;lfo.wave=(LfoWave)(v==0?3:v-1);}
const char *lfoWaveLabel(){return lfoWaveName(lfo.wave);}
void lfoRateInc(){lfo.rateHz=min(lfo.rateHz*1.15f,LFO_RATE_MAX);}
void lfoRateDec(){lfo.rateHz=max(lfo.rateHz/1.15f,LFO_RATE_MIN);}
char lfoRateBuf[10];const char *lfoRateLabel(){snprintf(lfoRateBuf,sizeof(lfoRateBuf),"%.2fHz",lfo.rateHz);return lfoRateBuf;}
void lfoDepthInc(){lfo.depth=min(lfo.depth+0.05f,1.f);}
void lfoDepthDec(){lfo.depth=max(lfo.depth-0.05f,0.f);}
char lfoDepthBuf[8];const char *lfoDepthLabel(){snprintf(lfoDepthBuf,sizeof(lfoDepthBuf),"%.0f%%",lfo.depth*100);return lfoDepthBuf;}
void lfoTargetNext(){uint8_t v=(uint8_t)lfo.target;lfo.target=(LfoTarget)((v+1)%(uint8_t)LfoTarget::TARGET_COUNT);}
void lfoTargetPrev(){uint8_t v=(uint8_t)lfo.target;lfo.target=(LfoTarget)(v==0?(uint8_t)LfoTarget::TARGET_COUNT-1:v-1);}
const char *lfoTargetLabel(){return lfoTargetName(lfo.target);}

SettingItem lfoItems[]={
    {"Wave",   lfoWaveNext,  lfoWavePrev,  lfoWaveLabel},
    {"Rate",   lfoRateInc,   lfoRateDec,   lfoRateLabel},
    {"Depth",  lfoDepthInc,  lfoDepthDec,  lfoDepthLabel},
    {"Target", lfoTargetNext,lfoTargetPrev,lfoTargetLabel},
};
const int NUM_LFO_ITEMS=sizeof(lfoItems)/sizeof(lfoItems[0]);
int selectedLfoIndex=0;

// ==========================================================
// Menu navigation
// ==========================================================
void updateMenuNavigation(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool tab=s.tab,mU=false,mD=false,mI=false,mDe=false;
    for(char c:s.word){
        if(c==';')mU=true;if(c=='.')mD=true;
        if(c=='/')mI=true;if(c==',')mDe=true;
    }
    if(tab&&!prevTabPressed&&appMode!=AppMode::PATCH){
        AppMode prev=appMode;
        switch(appMode){
            case AppMode::PLAY:     appMode=AppMode::VCO;      currentFreq=0;break;
            case AppMode::VCO:      appMode=AppMode::VCF;      break;
            case AppMode::VCF:      appMode=AppMode::VCA;      break;
            case AppMode::VCA:      appMode=AppMode::LFO;      break;
            case AppMode::LFO:      appMode=AppMode::SETTINGS; break;
            case AppMode::SETTINGS: appMode=AppMode::PLAY;     break;
            default: break; // PATCH is handled by updatePatchBrowser()
        }
        if(prev==AppMode::SETTINGS&&appMode==AppMode::PLAY)saveSettings();
    }
    prevTabPressed=tab;

    if(appMode==AppMode::SETTINGS){
        if(mU&&!prevMenuUpPressed)  selectedSettingIndex=(selectedSettingIndex-1+NUM_SETTING_ITEMS)%NUM_SETTING_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedSettingIndex=(selectedSettingIndex+1)%NUM_SETTING_ITEMS;
        if(mI&&!prevMenuIncPressed) settingItems[selectedSettingIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)settingItems[selectedSettingIndex].onDecrement();
    }
    if(appMode==AppMode::VCO){
        if(mU&&!prevMenuUpPressed)  selectedVcoIndex=(selectedVcoIndex-1+NUM_VCO_ITEMS)%NUM_VCO_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcoIndex=(selectedVcoIndex+1)%NUM_VCO_ITEMS;
        if(mI&&!prevMenuIncPressed) vcoItems[selectedVcoIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcoItems[selectedVcoIndex].onDecrement();
    }
    if(appMode==AppMode::VCF){
        if(mU&&!prevMenuUpPressed)  selectedVcfIndex=(selectedVcfIndex-1+NUM_VCF_ITEMS)%NUM_VCF_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcfIndex=(selectedVcfIndex+1)%NUM_VCF_ITEMS;
        if(mI&&!prevMenuIncPressed) vcfItems[selectedVcfIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcfItems[selectedVcfIndex].onDecrement();
    }
    if(appMode==AppMode::VCA){
        if(mU&&!prevMenuUpPressed)  selectedVcaIndex=(selectedVcaIndex-1+NUM_VCA_ITEMS)%NUM_VCA_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcaIndex=(selectedVcaIndex+1)%NUM_VCA_ITEMS;
        if(mI&&!prevMenuIncPressed) vcaItems[selectedVcaIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcaItems[selectedVcaIndex].onDecrement();
    }
    if(appMode==AppMode::LFO){
        if(mU&&!prevMenuUpPressed)  selectedLfoIndex=(selectedLfoIndex-1+NUM_LFO_ITEMS)%NUM_LFO_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedLfoIndex=(selectedLfoIndex+1)%NUM_LFO_ITEMS;
        if(mI&&!prevMenuIncPressed) lfoItems[selectedLfoIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)lfoItems[selectedLfoIndex].onDecrement();
    }
    prevMenuUpPressed=mU;prevMenuDownPressed=mD;
    prevMenuIncPressed=mI;prevMenuDecPressed=mDe;
}

// ==========================================================
// Display helpers
// ==========================================================
void drawTabBar(AppMode cur){
    M5Cardputer.Display.fillRect(0,0,240,11,BLACK);
    struct{const char *l;AppMode m;int x;}tabs[]={
        {"MAIN",AppMode::PLAY,0},   {"VCO",AppMode::VCO,40},
        {"VCF",AppMode::VCF,80},    {"VCA",AppMode::VCA,120},
        {"LFO",AppMode::LFO,160},   {"SET",AppMode::SETTINGS,200}
    };
    constexpr int TW=40;
    for(auto &t:tabs){
        bool act=(cur==t.m);
        M5Cardputer.Display.drawRect(t.x,0,TW,11,GREEN);
        if(act){M5Cardputer.Display.fillRect(t.x+1,1,TW-2,9,WHITE);M5Cardputer.Display.setTextColor(BLACK,WHITE);}
        else    M5Cardputer.Display.setTextColor(GREEN,BLACK);
        int lx=t.x+(TW-(int)strlen(t.l)*6)/2;
        M5Cardputer.Display.setCursor(lx,2);M5Cardputer.Display.print(t.l);
    }
    M5Cardputer.Display.setTextColor(GREEN,BLACK);
}

void drawAdsrGraph(){
    constexpr int GX=0,GY=12,GW=240,GH=60;
    M5Cardputer.Display.fillRect(GX,GY,GW,GH,BLACK);
    constexpr float FIXED=ADSR_MAX_TIME,SF=0.15f;
    float sx=(float)GW/(FIXED+FIXED*SF);
    int top=GY+4,bot=GY+GH-4,susY=bot-(int)((bot-top)*adsr.sustainLevel);
    int x0=GX;
    int x1=x0+max(1,(int)(adsr.attackTime*sx));
    int x2=x1+max(1,(int)(adsr.decayTime*sx));
    int x3=x2+(int)(FIXED*SF*sx);
    int x4=x3+max(1,(int)(adsr.releaseTime*sx));
    x1=min(x1,GX+GW-3);x2=min(x2,GX+GW-2);x3=min(x3,GX+GW-1);x4=min(x4,GX+GW);
    M5Cardputer.Display.drawLine(x0,bot,x1,top,GREEN);
    M5Cardputer.Display.drawLine(x1,top,x2,susY,GREEN);
    M5Cardputer.Display.drawLine(x2,susY,x3,susY,GREEN);
    M5Cardputer.Display.drawLine(x3,susY,x4,bot,GREEN);
    uint16_t yel=M5Cardputer.Display.color565(255,255,0);
    for(auto &p:{std::pair<int,int>{x1,top},{x2,susY},{x3,susY}})
        M5Cardputer.Display.fillRect(p.first-1,p.second-1,3,3,yel);
    M5Cardputer.Display.setCursor(x0+2,GY+GH-10);M5Cardputer.Display.print("A");
    M5Cardputer.Display.setCursor(x1+2,GY+GH-10);M5Cardputer.Display.print("D");
    M5Cardputer.Display.setCursor((x2+x3)/2-3,GY+GH-10);M5Cardputer.Display.print("S");
    M5Cardputer.Display.setCursor(x3+2,GY+GH-10);M5Cardputer.Display.print("R");
    uint16_t dim=M5Cardputer.Display.color565(0,64,0);
    int m1=GX+(int)(1.f*sx),m25=GX+(int)(2.5f*sx);
    M5Cardputer.Display.drawFastVLine(m1,GY,GH,dim);
    M5Cardputer.Display.drawFastVLine(m25,GY,GH,dim);
    M5Cardputer.Display.setCursor(m1+1,GY+1);M5Cardputer.Display.print("1s");
    M5Cardputer.Display.setCursor(m25+1,GY+1);M5Cardputer.Display.print("2.5s");
    M5Cardputer.Display.drawFastHLine(GX,GY+GH,GW,GREEN);
}

void drawWaveform(){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2,CYCLES=3;
    M5Cardputer.Display.fillRect(GX,GY,GW,GH,BLACK);
    M5Cardputer.Display.drawFastHLine(GX,CY,GW,M5Cardputer.Display.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE*CYCLES)%WAVE_TABLE_SIZE;
        int16_t s=getMorphedSample(idx,params.timbreMorph,params.pwmWidth);
        int y=constrain(CY-(int)((float)s/32768.f*(GH/2-2)),GY,GY+GH-1);
        if(px>0)M5Cardputer.Display.drawLine(px-1,pY,px,y,GREEN);
        pY=y;
    }
    M5Cardputer.Display.drawFastHLine(GX,GY+GH,GW,GREEN);
}

// Draw a scrollable item list.
// splitCol: if >= 0, items from index 0..(splitCol-1) go left, rest go right.
// splitCol < 0: single column layout.
void drawItemList(SettingItem *items,int count,int sel,int startY=76,int splitCol=-1){
    constexpr int ROW=13;
    constexpr int LX=5,RX=123;
    bool twoCol=(splitCol>0&&splitCol<count);

    if(twoCol){
        // Vertical divider
        M5Cardputer.Display.drawFastVLine(119,startY-2,count/2*ROW+4,M5Cardputer.Display.color565(0,64,0));
    }

    for(int i=0;i<count;i++){
        int x,y;
        if(twoCol){
            bool left=(i<splitCol);
            x=left?LX:RX;
            y=startY+(left?i:(i-splitCol))*ROW;
        } else {
            x=LX; y=startY+i*ROW;
        }
        M5Cardputer.Display.setCursor(x,y);
        if(i==sel)M5Cardputer.Display.printf(">%-8s%s",items[i].name,items[i].valueLabel());
        else      M5Cardputer.Display.printf(" %-8s%s",items[i].name,items[i].valueLabel());
    }
    const char *nav=";/. select  ,// change  Tab:next";
    M5Cardputer.Display.setCursor((240-(int)strlen(nav)*6)/2,126);
    M5Cardputer.Display.print(nav);
}

void drawVcoScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full)drawTabBar(AppMode::VCO);
    drawWaveform();
    M5Cardputer.Display.fillRect(0,56,240,70,BLACK);
    // Left(0-3): Timbre/PWM/Detune/FineTune  Right(4-6): SubLvl/SubOct/Noise
    drawItemList(vcoItems,NUM_VCO_ITEMS,selectedVcoIndex,57,4);
    M5Cardputer.Display.endWrite();
}

void drawVcfScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full)drawTabBar(AppMode::VCF);
    constexpr int GX=0,GY=12,GW=240,GH=60;
    M5Cardputer.Display.fillRect(GX,GY,GW,GH+4,BLACK);

    // Draw frequency response curve using Biquad magnitude calculation.
    // X axis = log frequency scale (100Hz - 20000Hz mapped to 0 - GW).
    // Y axis = gain in dB (top = +18dB, centre = 0dB, bottom = -48dB).
    constexpr float F_MIN  = 100.0f;
    constexpr float F_MAX  = 20000.0f;
    constexpr float DB_TOP =  18.0f;  // top of graph
    constexpr float DB_BOT = -48.0f;  // bottom of graph
    constexpr float DB_RNG = DB_TOP - DB_BOT;
    int zeroY = GY + (int)((DB_TOP / DB_RNG) * GH); // y coordinate for 0dB

    // 0dB reference line (dim green)
    uint16_t dim = M5Cardputer.Display.color565(0,64,0);
    M5Cardputer.Display.drawFastHLine(GX, zeroY, GW, dim);

    // Compute Biquad coefficients at current settings
    float cut = constrain(filterParams.cutoffHz, 100.f, SAMPLE_RATE*0.45f);
    float Q   = constrain(filterParams.resonanceQ, FILTER_Q_MIN, FILTER_Q_MAX);
    float omega = 2.0f*PI*cut/SAMPLE_RATE;
    float sinW=sinf(omega),cosW=cosf(omega),alpha=sinW/(2.0f*Q);
    float b0,b1,b2,a0,a1,a2;
    switch(filterParams.type){
        case FilterType::LPF:  b0=(1-cosW)/2;b1=1-cosW;b2=(1-cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::HPF:  b0=(1+cosW)/2;b1=-(1+cosW);b2=(1+cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::BPF:  b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NOTCH:b0=1;b1=-2*cosW;b2=1;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NONE: b0=1;b1=b2=0;a0=1;a1=a2=0;break; // bypass
        default:               b0=1;b1=b2=0;a0=1;a1=a2=0;break;
    }
    // Normalise by a0
    float nb0=b0/a0,nb1=b1/a0,nb2=b2/a0;
    float na1=a1/a0,na2=a2/a0;

    int prevY = -1;
    for(int px=0;px<GW;px++){
        // Map pixel x to frequency (log scale)
        float t   = (float)px / (GW-1);
        float freq= F_MIN * powf(F_MAX/F_MIN, t);
        float w   = 2.0f*PI*freq/SAMPLE_RATE;

        // |H(e^jw)|^2 = |B(e^jw)|^2 / |A(e^jw)|^2
        // B = nb0 + nb1*e^-jw + nb2*e^-2jw
        float Br = nb0 + nb1*cosf(w) + nb2*cosf(2*w);
        float Bi =     - nb1*sinf(w) - nb2*sinf(2*w);
        float Ar = 1.0f + na1*cosf(w) + na2*cosf(2*w);
        float Ai =       - na1*sinf(w) - na2*sinf(2*w);
        float magSq = (Br*Br+Bi*Bi) / (Ar*Ar+Ai*Ai+1e-12f);
        float dB = 10.0f*log10f(magSq+1e-12f);

        // Map dB to y coordinate
        float norm = (DB_TOP - dB) / DB_RNG;
        int y = GY + (int)(norm * GH);
        y = constrain(y, GY, GY+GH-1);

        if(px>0 && prevY>=0)
            M5Cardputer.Display.drawLine(px-1, prevY, px, y, GREEN);
        prevY = y;
    }

    // Cutoff marker (yellow vertical line)
    // Convert linear cutoff ratio to log-scale x position
    int cx = GX + (int)((log(filterParams.cutoffHz/F_MIN)/log(F_MAX/F_MIN)) * GW);
    cx = constrain(cx, GX, GX+GW-1);
    M5Cardputer.Display.drawFastVLine(cx, GY, GH, M5Cardputer.Display.color565(255,255,0));

    // Cutoff frequency label
    char fLabel[12]; snprintf(fLabel,sizeof(fLabel),"%.0fHz",filterParams.cutoffHz);
    int lx = (cx+4 < GX+GW-30) ? cx+2 : cx-28;
    M5Cardputer.Display.setCursor(lx, GY+2); M5Cardputer.Display.print(fLabel);

    M5Cardputer.Display.drawFastHLine(GX,GY+GH,GW,GREEN);
    M5Cardputer.Display.fillRect(0,76,240,50,BLACK);
    // Left(0-3): Filter/Cutoff/Resonance/KeyTrack  Right(4-7): FEnv Dep/Atk/Dec/Rel
    drawItemList(vcfItems,NUM_VCF_ITEMS,selectedVcfIndex,76,4);
    M5Cardputer.Display.endWrite();
}

void drawVcaScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full)drawTabBar(AppMode::VCA);
    drawAdsrGraph();
    M5Cardputer.Display.fillRect(0,76,240,50,BLACK);
    // Single column, 4 items at 13px intervals: 76,89,102,115 — nav at 126 clears
    drawItemList(vcaItems,NUM_VCA_ITEMS,selectedVcaIndex,76);
    M5Cardputer.Display.endWrite();
}

// Live LFO waveform: one full cycle drawn across the screen width, scaled
// by the current depth, plus a moving marker showing the LFO's live phase.
void drawLfoWaveform(){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2;
    M5Cardputer.Display.fillRect(GX,GY,GW,GH,BLACK);
    M5Cardputer.Display.drawFastHLine(GX,CY,GW,M5Cardputer.Display.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE)%WAVE_TABLE_SIZE;
        float s=lfoTableSample(lfo.wave,idx)*lfo.depth;
        int y=constrain(CY-(int)(s*(GH/2-2)),GY,GY+GH-1);
        if(px>0)M5Cardputer.Display.drawLine(px-1,pY,px,y,GREEN);
        pY=y;
    }
    // Live phase marker
    int mx=GX+(int)((lfoPhase/(double)WAVE_TABLE_SIZE)*GW);
    mx=constrain(mx,GX,GX+GW-1);
    M5Cardputer.Display.drawFastVLine(mx,GY,GH,M5Cardputer.Display.color565(255,255,0));
    M5Cardputer.Display.drawFastHLine(GX,GY+GH,GW,GREEN);
}

void drawLfoScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full)drawTabBar(AppMode::LFO);
    drawLfoWaveform();
    M5Cardputer.Display.fillRect(0,56,240,70,BLACK);
    drawItemList(lfoItems,NUM_LFO_ITEMS,selectedLfoIndex,76);
    M5Cardputer.Display.endWrite();
}

void drawSettingsScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full)drawTabBar(AppMode::SETTINGS);
    // Clear entire area below tab bar to remove any residual drawing from other screens
    M5Cardputer.Display.fillRect(0,12,240,123,BLACK);

    // 2-column layout:
    // Left  (x=0..119):  Patch Save, Patch Load, IMU X, IMU Y
    // Right (x=120..239): Bend wid/atk/rel, Portamento, Porta spd
    constexpr int ROW=13;
    constexpr int LX=5, RX=123;

    // Left column (items 0..SETTINGS_SPLIT_COL-1)
    for(int i=0;i<SETTINGS_SPLIT_COL&&i<NUM_SETTING_ITEMS;i++){
        M5Cardputer.Display.setCursor(LX,20+i*ROW);
        if(i==selectedSettingIndex)
            M5Cardputer.Display.printf(">%-8s%s",settingItems[i].name,settingItems[i].valueLabel());
        else
            M5Cardputer.Display.printf(" %-8s%s",settingItems[i].name,settingItems[i].valueLabel());
    }

    // Vertical divider
    uint16_t dim=M5Cardputer.Display.color565(0,64,0);
    M5Cardputer.Display.drawFastVLine(119,12,101,dim);
    // Category separators: Patch Bank | IMU  (left column), Bend | Portamento (right column)
    M5Cardputer.Display.drawFastHLine(LX-2,42,112,dim);
    M5Cardputer.Display.drawFastHLine(RX-2,55,112,dim);

    // Right column (items SETTINGS_SPLIT_COL+)
    for(int i=SETTINGS_SPLIT_COL;i<NUM_SETTING_ITEMS;i++){
        M5Cardputer.Display.setCursor(RX,20+(i-SETTINGS_SPLIT_COL)*ROW);
        if(i==selectedSettingIndex)
            M5Cardputer.Display.printf(">%-8s%s",settingItems[i].name,settingItems[i].valueLabel());
        else
            M5Cardputer.Display.printf(" %-8s%s",settingItems[i].name,settingItems[i].valueLabel());
    }

    const char *n1=";/. select  ,// change";
    const char *n2="Tab: save & return to play";
    M5Cardputer.Display.setCursor((240-(int)strlen(n1)*6)/2,115);M5Cardputer.Display.print(n1);
    M5Cardputer.Display.setCursor((240-(int)strlen(n2)*6)/2,128);M5Cardputer.Display.print(n2);
    M5Cardputer.Display.endWrite();
}

// ==========================================================
// Patch Bank screen
// ==========================================================
void drawPatchScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full){
        M5Cardputer.Display.fillRect(0,0,240,11,BLACK);
        M5Cardputer.Display.drawRect(0,0,240,11,GREEN);
        M5Cardputer.Display.fillRect(1,1,238,9,WHITE);
        M5Cardputer.Display.setTextColor(BLACK,WHITE);
        const char *title=(patchMode==PatchMode::SAVE)?"PATCH BANK - SAVE":"PATCH BANK - LOAD";
        M5Cardputer.Display.setCursor((240-(int)strlen(title)*6)/2,2);
        M5Cardputer.Display.print(title);
        M5Cardputer.Display.setTextColor(GREEN,BLACK);
    }
    M5Cardputer.Display.fillRect(0,12,240,123,BLACK);

    if(patchUiState==PatchUiState::NAME_ENTRY){
        const char *label=patchRenaming?"Rename to:":(patchDuplicating?"Duplicate as:":"New patch name:");
        M5Cardputer.Display.setCursor(6,28);M5Cardputer.Display.print(label);
        M5Cardputer.Display.drawRect(6,40,228,16,GREEN);
        M5Cardputer.Display.setCursor(10,44);
        M5Cardputer.Display.printf("%s_",patchNameBuffer.c_str());
        const char *nav="Type name   Enter:OK   Tab:Cancel";
        M5Cardputer.Display.setCursor((240-(int)strlen(nav)*6)/2,110);
        M5Cardputer.Display.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_DELETE){
        M5Cardputer.Display.setCursor(6,40);
        M5Cardputer.Display.printf("Delete '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        M5Cardputer.Display.setCursor((240-(int)strlen(nav)*6)/2,60);
        M5Cardputer.Display.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_OVERWRITE){
        M5Cardputer.Display.setCursor(6,40);
        M5Cardputer.Display.printf("Overwrite '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        M5Cardputer.Display.setCursor((240-(int)strlen(nav)*6)/2,60);
        M5Cardputer.Display.print(nav);
    } else {
        int count=patchListCount();
        if(count==0){
            M5Cardputer.Display.setCursor(6,40);
            M5Cardputer.Display.print("No patches saved yet.");
        } else {
            constexpr int ROW=13,startY=16,maxRows=7;
            int top=constrain(selectedPatchIndex-maxRows/2,0,max(0,count-maxRows));
            for(int i=0;i<maxRows&&(top+i)<count;i++){
                int row=top+i;
                M5Cardputer.Display.setCursor(6,startY+i*ROW);
                String label=patchIsNewRow(row)?"<New Patch>":patchNames[patchRowToNameIndex(row)];
                if(row==selectedPatchIndex)M5Cardputer.Display.printf(">%s",label.c_str());
                else                       M5Cardputer.Display.printf(" %s",label.c_str());
            }
        }
        const char *nav1=";/.:Select  /:OK  r:Rename";
        const char *nav2="c:Duplicate  ,:Delete  Tab:Back";
        M5Cardputer.Display.setCursor((240-(int)strlen(nav1)*6)/2,111);
        M5Cardputer.Display.print(nav1);
        M5Cardputer.Display.setCursor((240-(int)strlen(nav2)*6)/2,122);
        M5Cardputer.Display.print(nav2);
    }
    M5Cardputer.Display.endWrite();
}

// ==========================================================
// MAIN screen
// ==========================================================
const char *getNoteName(float freq){
    if(freq<=0)return "---";
    static const char *n[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int nn=(int)roundf(12*log2f(freq/440)+69);
    int nm=nn%12;if(nm<0)nm+=12;
    static char buf[8];snprintf(buf,sizeof(buf),"%s%d",n[nm],nn/12-1);
    return buf;
}

void drawBendMeter(float bc,float mc){
    constexpr int MX=75,MY=57,MW=12,MH=50,MCY=MY+MH/2;
    M5Cardputer.Display.fillRect(MX,MY,MW,MH,BLACK);
    M5Cardputer.Display.drawRect(MX,MY,MW,MH,GREEN);
    M5Cardputer.Display.drawFastHLine(MX,MCY,MW,GREEN);
    if(fabsf(bc)>0.5f){
        float r=constrain(bc/mc,-1.f,1.f);
        int bh=(int)(fabsf(r)*(MH/2-1));
        M5Cardputer.Display.fillRect(MX+1,r>0?MCY-bh:MCY,MW-2,bh,GREEN);
    }
    M5Cardputer.Display.setCursor(MX-2,MY-7);M5Cardputer.Display.print("UP");
    M5Cardputer.Display.setCursor(MX-5,MY+MH+1);M5Cardputer.Display.print("DWN");
}

void drawImuPad(){
    constexpr int PX=90,PY=57,PAD_SIZE=44,cx=PX+PAD_SIZE/2,cy=PY+PAD_SIZE/2;
    M5Cardputer.Display.fillRect(PX-1,PY-9,PAD_SIZE+16,9,BLACK);
    M5Cardputer.Display.fillRect(PX,PY,PAD_SIZE,PAD_SIZE,BLACK);
    M5Cardputer.Display.drawRect(PX,PY,PAD_SIZE,PAD_SIZE,GREEN);
    uint16_t dim=M5Cardputer.Display.color565(0,64,0);
    M5Cardputer.Display.drawFastHLine(PX,cy,PAD_SIZE,dim);
    M5Cardputer.Display.drawFastVLine(cx,PY,PAD_SIZE,dim);
    float nX=constrain(lastAccelX/(TILT_MAX_DEGREES/57.3f),-1.f,1.f);
    float nY=constrain(lastAccelY/(TILT_MAX_DEGREES/57.3f),-1.f,1.f);
    int dX=constrain(cx+(int)(nX*(PAD_SIZE/2-3)),PX+2,PX+PAD_SIZE-3);
    int dY=constrain(cy+(int)(nY*(PAD_SIZE/2-3)),PY+2,PY+PAD_SIZE-3);
    M5Cardputer.Display.fillCircle(dX,dY,3,GREEN);
    M5Cardputer.Display.setCursor(cx-3,PY-8);M5Cardputer.Display.print("Y");
    M5Cardputer.Display.setCursor(PX+PAD_SIZE+2,cy-4);M5Cardputer.Display.print("X");
}

float getImuNorm(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        return params.timbreMorph/3.f;
        case ImuTarget::VIBRATO_DEPTH: return params.vibratoDepth;
        case ImuTarget::VIBRATO_RATE:  return (params.vibratoRateHz-1)/9.f;
        case ImuTarget::TREMOLO:       return params.tremoloDepth;
        case ImuTarget::VOLUME:        return params.volumeOffset+0.5f;
        case ImuTarget::PITCH_BEND:    return (params.pitchBendCents+keyBendMaxCents)/(keyBendMaxCents*2);
        case ImuTarget::BEND_UP:       return constrain(params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BEND_DOWN:     return constrain(-params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BITCRUSH:      return params.bitcrush;
        case ImuTarget::FILTER_CUTOFF: return params.filterCutoffOffset;
        default: return 0;
    }
}

String getImuValStr(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:{const char *s[]={"Sine","Tri","Saw","Sq"};return String(s[constrain((int)params.timbreMorph,0,3)])+"("+String(params.timbreMorph,1)+")";}
        case ImuTarget::VIBRATO_DEPTH: return String((int)(params.vibratoDepth*100))+"%";
        case ImuTarget::VIBRATO_RATE:  return String(params.vibratoRateHz,1)+"Hz";
        case ImuTarget::TREMOLO:       return String((int)(params.tremoloDepth*100))+"%";
        case ImuTarget::VOLUME:        return String((int)(params.volumeOffset*100))+"%";
        case ImuTarget::PITCH_BEND:    return String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_UP:       return "+"+String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_DOWN:     return String((int)params.pitchBendCents)+"c";
        case ImuTarget::BITCRUSH:      return String((int)(params.bitcrush*100))+"%";
        case ImuTarget::FILTER_CUTOFF:{float c=filterParams.cutoffHz*(1-params.filterCutoffOffset*0.9f);return String((int)constrain(c,FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX))+"Hz";}
        default: return "---";
    }
}

// HELP overlay: shown while H is held
// Drawn over the existing screen content without fillScreen (no flicker)
void drawHelpOverlay(){
    M5Cardputer.Display.fillRect(2,13,236,99,BLACK);
    M5Cardputer.Display.drawRect(2,13,236,99,GREEN);
    M5Cardputer.Display.setCursor(6,17); M5Cardputer.Display.print("=== HELP (hold H) ===");
    M5Cardputer.Display.setCursor(6,27); M5Cardputer.Display.print("1-0:Notes    =/- :Octave");
    M5Cardputer.Display.setCursor(6,37); M5Cardputer.Display.print("[/] :Transpose ,/.:Volume");
    M5Cardputer.Display.setCursor(6,47); M5Cardputer.Display.print("Z/X :Bend      C  :Porta");
    M5Cardputer.Display.setCursor(6,57); M5Cardputer.Display.print("A   :IMU-X hold S :IMU-Y hold");
    M5Cardputer.Display.setCursor(6,67); M5Cardputer.Display.print("D   :Note hold");
    M5Cardputer.Display.setCursor(6,77); M5Cardputer.Display.print("H   :This help (hold)");
    M5Cardputer.Display.setCursor(6,87); M5Cardputer.Display.print("Tab :Cycle menus");
    M5Cardputer.Display.setCursor(6,99); M5Cardputer.Display.print("release H to close");
}

void drawPlayScreen(bool full){
    M5Cardputer.Display.startWrite();
    if(full){
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.drawFastHLine(0,113,240,GREEN);
        const char *nav="Tab:MENU  H:HELP";
        M5Cardputer.Display.setCursor((240-(int)strlen(nav)*6)/2,121);
        M5Cardputer.Display.print(nav);
        drawTabBar(AppMode::PLAY);
    }

    // While HELP is visible, skip all other drawing and show only the overlay.
    // This prevents flickering caused by waveform/info redraws competing with
    // the overlay on every frame.
    if(helpVisible){
        drawHelpOverlay();
        M5Cardputer.Display.endWrite();
        return;
    }

    drawWaveform();

    // Left: note block
    M5Cardputer.Display.fillRect(0,56,73,57,BLACK);
    M5Cardputer.Display.drawRect(0,56,73,57,GREEN);
    float df=playingFreq>0?playingFreq:currentFreq;
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(4,60);
    M5Cardputer.Display.printf("%-4s",getNoteName(df));
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4,84);
    if(df>0)M5Cardputer.Display.printf("%-9s",(String(df,1)+"Hz").c_str());
    else    M5Cardputer.Display.print("---      ");
    M5Cardputer.Display.setCursor(4,95);
    M5Cardputer.Display.printf("O:%+d T:%+d ",params.octaveShift,transposeSemitones);
    M5Cardputer.Display.setCursor(4,104);
    M5Cardputer.Display.printf("P:%-3s H:%-3s",portaEnabled?"ON":"off",noteHeld?"ON":"off");

    // Bend meter
    drawBendMeter(params.pitchBendCents+keyBendCurrent,keyBendMaxCents);

    // IMU pad
    drawImuPad();

    // IMU parameter readout
    constexpr int TX=140,TW=100;
    M5Cardputer.Display.fillRect(TX,56,TW,57,BLACK);
    M5Cardputer.Display.setCursor(TX,57);
    M5Cardputer.Display.printf("X:%-10s",imuTargetName(imuAxisX.target));
    M5Cardputer.Display.setCursor(TX,66);
    {
        String xVal=getImuValStr(imuAxisX.target);
        if(imuXHeld)xVal+="(HOLD)";
        M5Cardputer.Display.printf(" %-11s",xVal.c_str());
    }
    if(imuAxisX.target!=ImuTarget::NONE){
        float n=constrain(getImuNorm(imuAxisX.target),0.f,1.f);
        M5Cardputer.Display.fillRect(TX,75,TW-2,4,M5Cardputer.Display.color565(0,64,0));
        if((int)(n*(TW-4))>0)M5Cardputer.Display.fillRect(TX,75,(int)(n*(TW-4)),4,GREEN);
    }
    M5Cardputer.Display.setCursor(TX,81);
    M5Cardputer.Display.printf("Y:%-10s",imuTargetName(imuAxisY.target));
    M5Cardputer.Display.setCursor(TX,90);
    {
        String yVal=getImuValStr(imuAxisY.target);
        if(imuYHeld)yVal+="(HOLD)";
        M5Cardputer.Display.printf(" %-11s",yVal.c_str());
    }
    if(imuAxisY.target!=ImuTarget::NONE){
        float n=constrain(getImuNorm(imuAxisY.target),0.f,1.f);
        M5Cardputer.Display.fillRect(TX,99,TW-2,4,M5Cardputer.Display.color565(0,64,0));
        if((int)(n*(TW-4))>0)M5Cardputer.Display.fillRect(TX,99,(int)(n*(TW-4)),4,GREEN);
    }
    M5Cardputer.Display.setCursor(TX,105);
    M5Cardputer.Display.printf("VOL:%d%% BND:%dst  ",
        (int)(params.keyVolume*100),(int)(keyBendMaxCents/100));

    // Redraw this line last: it spans the full screen width right above the
    // IMU pad / readout row, and earlier clears in that row (e.g. the IMU
    // pad's "Y" label clear rect) could otherwise leave a gap in it.
    M5Cardputer.Display.drawFastHLine(0,55,240,GREEN);

    M5Cardputer.Display.endWrite();
}

// ==========================================================
// setup / loop
// ==========================================================
void setup(){
    Serial.begin(115200);
    auto cfg=M5.config();
    M5Cardputer.begin(cfg,true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextColor(GREEN,BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("C.P.S. CardPuter Synth",10,10);

    buildWaveTables();
    updateFilterCoefficients();

    bool sdOk=initSDCard();
    if(sdOk){ensureCpsFolder();ensurePatchFolder();loadSettings();Serial.println("[SD] OK");}
    else Serial.println("[SD] not found");

    bool imuOk=M5.Imu.begin();
    Serial.println(imuOk?"[IMU] OK":"[IMU] not found");

    auto sc=M5Cardputer.Speaker.config();
    sc.sample_rate=SAMPLE_RATE;sc.dma_buf_count=4;
    sc.dma_buf_len=512;sc.task_pinned_core=APP_CPU_NUM;
    M5Cardputer.Speaker.config(sc);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(255);

    // Audio synthesis runs on Core 0 (PRO_CPU). Core 1 (APP_CPU) is left for
    // loop() (keyboard scan + display) and the Speaker's own DMA-feeding task
    // (task_pinned_core above). Both cores were previously shared between
    // loop() and this task, which could starve the watchdog under heavy
    // load (e.g. LFO active + rapid retriggering) and freeze the device.
    xTaskCreatePinnedToCore(audioTask,"audioTask",4096,nullptr,5,nullptr,PRO_CPU_NUM);
}

unsigned long lastDisplayMs=0;
AppMode lastDrawnMode=AppMode::SETTINGS;
bool lastHelpVisible=false;

void loop(){
    M5Cardputer.update();
    updateImu();
    updateMenuNavigation();
    if(appMode==AppMode::PATCH)updatePatchBrowser();

    bool keyChanged=M5Cardputer.Keyboard.isChange();
    if(keyChanged){
        updateOctaveAndVolume();
        // Note keys double as text-entry / value keys in SETTINGS and the
        // Patch Bank, so notes are not triggered while in those modes.
        if(appMode!=AppMode::SETTINGS&&appMode!=AppMode::PATCH){
            float nf=resolveFreqFromKeys();
            if(nf>0&&currentFreq==0){
                if(envPhase==EnvPhase::IDLE)envLevel=0;
                envPhase=EnvPhase::ATTACK;
                filterEnvPhase=EnvPhase::ATTACK;  // trigger filter envelope
                if(portaEnabled&&portaFreq==0)portaFreq=nf;
            }
            currentFreq=nf;
        } else {
            currentFreq=0;
        }
    }

    bool modeChanged=(appMode!=lastDrawnMode);
    lastDrawnMode=appMode;

    // Non-PLAY screens: only redraw on menu keys or mode change
    bool menuKey=false;
    if(appMode!=AppMode::PLAY&&keyChanged){
        auto st=M5Cardputer.Keyboard.keysState();
        for(char c:st.word)if(c==';'||c=='.'||c==','||c=='/')menuKey=true;
        if(st.tab)menuKey=true;
    }

    if(appMode==AppMode::VCO){
        unsigned long now=millis();
        if(menuKey||modeChanged||(now-lastDisplayMs)>=50){lastDisplayMs=now;drawVcoScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::VCF){if(menuKey||modeChanged)drawVcfScreen(modeChanged);delay(5);return;}
    if(appMode==AppMode::VCA){if(menuKey||modeChanged)drawVcaScreen(modeChanged);delay(5);return;}
    if(appMode==AppMode::LFO){
        unsigned long now=millis();
        if(menuKey||modeChanged||(now-lastDisplayMs)>=50){lastDisplayMs=now;drawLfoScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::SETTINGS){if(menuKey||modeChanged)drawSettingsScreen(modeChanged);delay(5);return;}
    if(appMode==AppMode::PATCH){
        unsigned long now=millis();
        if(keyChanged||modeChanged||(now-lastDisplayMs)>=50){lastDisplayMs=now;drawPatchScreen(modeChanged);}
        delay(5);return;
    }

    // MAIN screen
    bool helpChanged=(helpVisible!=lastHelpVisible);
    lastHelpVisible=helpVisible;
    unsigned long now=millis();
    // When HELP is released, force a full redraw to restore the normal screen cleanly
    bool forceFullRedraw=(helpChanged&&!helpVisible);
    if(keyChanged||modeChanged||helpChanged||(now-lastDisplayMs)>=50){
        lastDisplayMs=now;
        drawPlayScreen(modeChanged||forceFullRedraw);
    }
    delay(5);
}
