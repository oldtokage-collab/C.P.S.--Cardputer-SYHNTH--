/*
 * CPS (CardPuter Synth) - プロトタイプ v11
 * ------------------------------------------------------
 * CardputerADV用の自作シンセサイザーアプリ。
 *
 * 実装済み仕様:
 *   - キーボードの1段（数字キー "1234567890"）に音階(C4〜)を割り当て
 *   - サイン波をオンザフライで生成し、キーを押している間だけ発音
 *   - モノフォニック（同時押しは後勝ち = 最後に押したキーの音）
 *   - '='/'-' キーでオクターブ変更（-2〜+2の範囲）
 *   - ','/'.' キーで音量変更（0〜100%、5%刻み）
 *   - 'Z' キー: ベンドダウン、'X' キー: ベンドアップ
 *     ギターのチョーキングのように、押している間ゆっくり音程が
 *     しゃくり上がり/下がり、離すと素早く元に戻る(アタック/リリースが非対称)。
 *   - ADSRエンベロープ:
 *       Attack/Decay/Sustain/Releaseの4段階で音量を時間変化させる。
 *       離した後も Release時間ぶん余韻が残る(従来のオン/オフ的な発音から変更)。
 *       Release中に再度キーを押すと、その時点の音量からAttackを再開する
 *       (モノフォニックのままリトリガー的に動作)。
 *   - Biquadフィルター(二次IIR):
 *       LPF(ローパス)/HPF(ハイパス)/BPF(バンドパス)/Notch(バンドリジェクト)の
 *       4種類を切り替え可能。カットオフ周波数・レゾナンス(Q値)をEDIT画面で設定。
 *       信号フローは「波形生成 → ビットクラッシャー → フィルター → ADSR(音量)」の順。
 *   - IMU(BMI270)の傾きで各種パラメータを変化させる(マッピング設定で変更可能):
 *       X軸: デフォルト = 波形モーフィング（サイン→三角→ノコギリ→矩形）
 *       Y軸: デフォルト = ビブラート深さ
 *     割り当て可能なパラメータ:
 *       NONE / TIMBRE / VIBRATO_DEPTH / VIBRATO_RATE / TREMOLO /
 *       VOLUME / PITCH_BEND / BITCRUSH / FILTER_CUTOFF
 *     FILTER_CUTOFFを割り当てると、EDIT画面で設定した基準カットオフから
 *     傾けるほど下方向にオフセットがかかる(こもらせる方向)。
 *     IMU軸の割り当てを切り替えると、切替前のパラメータは自動的に
 *     デフォルト値へリセットされる。
 *   - 'Tab' キーで PLAY -> EDIT -> SETTINGS -> PLAY と画面を順送りで切り替え
 *       EDIT画面: ADSRエンベロープの形状をグラフ表示しつつ、A/D/S/Rと
 *         フィルター(Type/Cutoff/Resonance)の数値を設定できる
 *         (';'/'.' で項目選択、','/'/' で値変更、2カラム表示)
 *       SETTINGS画面: IMU X/Y軸の割り当て先、ベンド幅、
 *         ベンドのアタック/リリース速度を設定できる(操作方法は同様)
 *   - 設定の自動保存/読み込み:
 *       SETTINGS画面からTabキーでPLAY画面に戻るたびに、IMUマッピング・
 *       ベンド設定・ADSR設定・フィルター設定をまとめてSDカードの
 *       /CPS/settings.json に自動保存する。起動時にこのファイルがあれば
 *       自動で読み込む(無ければデフォルト値)。
 *   - 起動時にSDカードの /CPS フォルダを確認し、無ければ自動作成する
 *
 * 必要ライブラリ: M5Cardputer (内部で M5Unified / M5GFX を使用)
 */

#include "M5Cardputer.h"
#include <math.h>
#include <SPI.h>
#include <SD.h>

// ---------------------------------------------------------
// SDカード設定
// ---------------------------------------------------------
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
static const char *CPS_FOLDER_PATH = "/CPS";

// ---------------------------------------------------------
// オーディオ設定
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
// 音階テーブル
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
// IMUマッピング定義
// ---------------------------------------------------------
// IMUの各軸に割り当て可能なパラメータの種類。
// 今後パラメータを追加する際はここに enum 値を追加し、
// applyImuValue() と imuTargetName() に対応処理を追記する。
enum class ImuTarget : uint8_t {
    NONE,           // 無効（その軸は何もしない）
    TIMBRE,         // 波形モーフィング (0.0=サイン 〜 3.0=矩形)
    VIBRATO_DEPTH,  // ビブラート深さ   (0.0〜1.0)
    VIBRATO_RATE,   // ビブラート速さ   (1〜10Hz)
    TREMOLO,        // トレモロ深さ     (0.0〜1.0)
    VOLUME,         // 音量オフセット   (-0.5〜+0.5、キー操作の音量に加算)
    PITCH_BEND,     // ピッチベンド bipolar: 傾き方向で上下(ランダムピッチ遊び用)
    BEND_UP,        // ベンドアップ: 傾けるほど音程が上がる(絶対値、上方向のみ)
    BEND_DOWN,      // ベンドダウン: 傾けるほど音程が下がる(絶対値、下方向のみ)
    BITCRUSH,       // ビットクラッシュ (0.0=原音 〜 1.0=最大ビット削減)
    FILTER_CUTOFF,  // フィルターカットオフ (100〜8000Hz)
    TARGET_COUNT    // (要素数カウント用。実際のターゲットではない)
};

// 前方宣言: 定義は本ファイル下部（画面表示用関数群）にあるが、
// imuXLabel()/imuYLabel() (このすぐ下のSettingItem関連コードより前に登場)から
// 呼び出すため、ここで宣言だけ先に行っておく。
const char *imuTargetName(ImuTarget t);

// 各軸の設定を保持する構造体
struct ImuAxisConfig {
    ImuTarget target;   // どのパラメータに割り当てるか
    float sensitivity;  // 感度係数 (1.0 = 標準。後で設定メニューから変更可能にする予定)
    bool bipolar;       // false=絶対値で0〜1(どちらに傾けても同じ効果)
                        // true =符号あり-1〜+1(傾ける方向で効果の方向が変わる)
                        // ピッチベンド・音量オフセットはtrue、それ以外はfalseが自然
};

// デフォルトのマッピング設定
// (将来: /CPS/imu_map.json から読み込むようにする予定)
ImuAxisConfig imuAxisX = { ImuTarget::TIMBRE,       1.0f, false };
ImuAxisConfig imuAxisY = { ImuTarget::VIBRATO_DEPTH, 1.0f, false };

// 傾きの感度調整。この角度(度)で最大値(1.0)に達する。
constexpr float TILT_MAX_DEGREES = 35.0f;

// ---------------------------------------------------------
// シンセパラメータ
// ---------------------------------------------------------
// audioTaskが直接読む「現在値」と、loop()側が書く「目標値」を分離。
// audioTask内で指数移動平均により現在値を目標値に徐々に近づける(なめらか追従)。
// キー操作で変えるパラメータ(octaveShift, keyVolume)は即時反映。
// IMUが制御するパラメータは *Target 変数経由。
struct SynthParams {
    // --- キー操作で変更 ---
    float    keyVolume   = 0.5f;  // ,/. キーで変更 (0.0〜1.0)
    int      octaveShift = 0;     // =/- キーで変更 (-2〜+2)

    // --- IMUが制御: 現在値 ---
    float timbreMorph   = 0.0f;  // 波形モーフィング (0.0〜3.0)
    float vibratoDepth  = 0.0f;  // ビブラート深さ   (0.0〜1.0)
    float vibratoRateHz = 5.0f;  // ビブラート速さ   (Hz)
    float tremoloDepth  = 0.0f;  // トレモロ深さ     (0.0〜1.0)
    float volumeOffset  = 0.0f;  // 音量オフセット   (-0.5〜+0.5)
    float pitchBendCents= 0.0f;  // ピッチベンド     (セント)
    float bitcrush      = 0.0f;  // ビットクラッシュ (0.0〜1.0)
    float filterCutoffOffset = 0.0f; // フィルターカットオフへのIMUオフセット (-1.0〜+1.0、後でHz換算)

    // --- IMUが制御: 目標値 ---
    float timbreMorphTarget    = 0.0f;
    float vibratoDepthTarget   = 0.0f;
    float vibratoRateHzTarget  = 5.0f;
    float tremoloDepthTarget   = 0.0f;
    float volumeOffsetTarget   = 0.0f;
    float pitchBendCentsTarget = 0.0f;
    float bitcrushTarget       = 0.0f;
    float filterCutoffOffsetTarget = 0.0f;
} params;

// 現在鳴っている周波数 (0.0 = 無音)
float currentFreq = 0.0f;

// キー操作用エッジ検出フラグ
bool prevOctaveUpPressed   = false;
bool prevOctaveDownPressed = false;
bool prevVolumeUpPressed   = false;
bool prevVolumeDownPressed = false;

// 波形生成・LFO用の位相アキュムレータ
double phase        = 0.0;
double vibratoPhase = 0.0;
double tremoloPhase = 0.0;

constexpr float VIBRATO_MAX_CENTS = 35.0f; // ビブラート最大音程変化(セント)

// ベンドキー(Z/X)の最大幅。後でメニューから変更できるようにする予定。
// 200セント = 2半音、100セント = 1半音。
float keyBendMaxCents = 200.0f;

// ベンドキーの平滑化処理用変数
// keyBendGoal  : Z/Xキーが書き込む目標値（-keyBendMaxCents / 0 / +keyBendMaxCents）
// keyBendCurrent: audioTaskが実際に使う現在値（goalにゆっくり追従）
//
// アタックとリリースで追従速度を変えることで「チョーキング」らしい動きになる:
//   アタック(押した時)  : ゆっくり上昇 → しゃくり上げ感
//   リリース(離した時) : 素早く戻る   → チョーキングのリリース感
//
// 係数の意味: 毎サンプル (goal - current) * 係数 だけ current が近づく。
//   大きいほど速く追従。1バッファ(1024サンプル@44.1kHz ≒ 23ms)でどれだけ進むかで体感が変わる。
//   ATTACK=0.0003  → 最大まで約1.5秒かかる（ゆったりしたチョーキング）
//   RELEASE=0.003  → 最大から0まで約0.15秒で戻る（パチッとリリース）
//   好みに応じて値を調整してください。
float keyBendGoal    = 0.0f;
float keyBendCurrent = 0.0f;
constexpr float KEY_BEND_ATTACK_SMOOTH_DEFAULT  = 0.0003f;
constexpr float KEY_BEND_RELEASE_SMOOTH_DEFAULT = 0.003f;
// 上の2つはメニューで調整できるよう変数化する(constexprから昇格)
float keyBendAttackSmooth  = KEY_BEND_ATTACK_SMOOTH_DEFAULT;
float keyBendReleaseSmooth = KEY_BEND_RELEASE_SMOOTH_DEFAULT;

// ---------------------------------------------------------
// ADSRエンベロープ
// ---------------------------------------------------------
// Attack / Decay / Sustain / Release の4段階で音量を時間変化させる。
// 現在は線形変化で実装(指数カーブへの発展は将来の拡張ポイント)。
//
// フェーズの流れ:
//   IDLE -> (キーを押す) -> ATTACK -> DECAY -> SUSTAIN -> (キーを離す) -> RELEASE -> IDLE
//
// envLevel: 現在のエンベロープ出力(0.0〜1.0)。audioTask内で音量に掛け合わせる。
// envPhase: 現在どの段階にいるか。
// playingFreq: 実際にオシレーターが鳴らす周波数。
//   currentFreq(キーが押されているかどうかを表す)とは別に持つことで、
//   「キーは離されたがRelease中でまだ音が鳴っている」状態を表現できる。
enum class EnvPhase : uint8_t {
    IDLE,     // 無音、何も鳴っていない
    ATTACK,   // 0 -> 1 へ上昇中
    DECAY,    // 1 -> sustainLevel へ下降中
    SUSTAIN,  // sustainLevel を維持(キーを押し続けている間)
    RELEASE,  // 現在値 -> 0 へ下降中(キーを離した後)
};

struct AdsrParams {
    float attackTime  = 0.05f; // 秒。0に近いほど立ち上がりが速い(パーカッシブ)
    float decayTime   = 0.15f; // 秒
    float sustainLevel= 0.7f;  // 0.0〜1.0。Decay後に保持する音量レベル
    float releaseTime = 0.3f;  // 秒
} adsr;

constexpr float ADSR_MIN_TIME = 0.0f;   // 最短(ほぼ即時)
constexpr float ADSR_MAX_TIME = 5.0f;   // 最長5秒（パッド系の長い音にも対応）

// ---------------------------------------------------------
// Biquadフィルター (二次IIRフィルター)
// ---------------------------------------------------------
// LPF(ローパス)/HPF(ハイパス)/BPF(バンドパス)/Notch(バンドリジェクト)を
// 同じ差分方程式構造のまま、係数(b0,b1,b2,a1,a2)の計算式だけ変えて実現する。
// 標準的な「Audio EQ Cookbook」の式を使用。
//
// 差分方程式:
//   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
// (係数はすべて a0=1 になるよう正規化済みの値を保持する)
enum class FilterType : uint8_t {
    LPF,    // ローパス: カットオフより高い成分を削る(こもった音)
    HPF,    // ハイパス: カットオフより低い成分を削る(シャリシャリした音)
    BPF,    // バンドパス: カットオフ付近だけ通す(電話越し・鼻声っぽい音)
    NOTCH,  // ノッチ(バンドリジェクト): カットオフ付近だけ削る(独特な抜け感)
};

struct FilterParams {
    FilterType type     = FilterType::LPF;
    float cutoffHz       = 2000.0f; // カットオフ周波数(Hz)
    float resonanceQ     = 0.707f;  // レゾナンス(Q値)。0.707=バターワース特性(素直な効き)
} filterParams;

constexpr float FILTER_CUTOFF_MIN = 100.0f;
constexpr float FILTER_CUTOFF_MAX = 8000.0f;
constexpr float FILTER_Q_MIN      = 0.5f;
constexpr float FILTER_Q_MAX      = 10.0f;

// フィルター係数(audioTaskが直接参照する。updateFilterCoefficients()で再計算する)
float filterB0 = 1.0f, filterB1 = 0.0f, filterB2 = 0.0f;
float filterA1 = 0.0f, filterA2 = 0.0f;

// フィルターの内部状態(直前2サンプル分の入力・出力)。モノフォニックなので1系統のみ。
float filterX1 = 0.0f, filterX2 = 0.0f; // 入力の1つ前・2つ前
float filterY1 = 0.0f, filterY2 = 0.0f; // 出力の1つ前・2つ前

// ---------------------------------------------------------
// 現在のfilterParams(type/cutoffHz/resonanceQ)からBiquad係数を再計算する。
// カットオフやQ、タイプを変更するたびに呼ぶ必要がある。
// 参照: Audio EQ Cookbook (R.Bristow-Johnson)の標準式
// ---------------------------------------------------------
void updateFilterCoefficients() {
    float cutoff = filterParams.cutoffHz;
    if (cutoff < FILTER_CUTOFF_MIN) cutoff = FILTER_CUTOFF_MIN;
    if (cutoff > FILTER_CUTOFF_MAX) cutoff = FILTER_CUTOFF_MAX;
    // ナイキスト周波数(SAMPLE_RATE/2)を超えないようさらに安全側にクランプ
    if (cutoff > SAMPLE_RATE * 0.45f) cutoff = SAMPLE_RATE * 0.45f;

    float Q = filterParams.resonanceQ;
    if (Q < FILTER_Q_MIN) Q = FILTER_Q_MIN;
    if (Q > FILTER_Q_MAX) Q = FILTER_Q_MAX;

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
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 =  1.0f - alpha;
            break;
        case FilterType::HPF:
            b0 =  (1.0f + cosW) / 2.0f;
            b1 = -(1.0f + cosW);
            b2 =  (1.0f + cosW) / 2.0f;
            a0 =   1.0f + alpha;
            a1 =  -2.0f * cosW;
            a2 =   1.0f - alpha;
            break;
        case FilterType::BPF:
            // 定数ピークゲイン型(constant 0 dB peak gain)のBPF
            b0 =  alpha;
            b1 =  0.0f;
            b2 = -alpha;
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 =  1.0f - alpha;
            break;
        case FilterType::NOTCH:
            b0 =  1.0f;
            b1 = -2.0f * cosW;
            b2 =  1.0f;
            a0 =  1.0f + alpha;
            a1 = -2.0f * cosW;
            a2 =  1.0f - alpha;
            break;
        default:
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a0 = 1.0f; a1 = 0.0f; a2 = 0.0f;
            break;
    }

    // a0で正規化して保存(差分方程式の計算を毎サンプル軽くするため)
    filterB0 = b0 / a0;
    filterB1 = b1 / a0;
    filterB2 = b2 / a0;
    filterA1 = a1 / a0;
    filterA2 = a2 / a0;
}

// ---------------------------------------------------------
// 1サンプル分のBiquadフィルター処理
// ---------------------------------------------------------
int16_t applyFilter(int16_t inSample) {
    float x0 = (float)inSample;
    float y0 = filterB0 * x0 + filterB1 * filterX1 + filterB2 * filterX2
             - filterA1 * filterY1 - filterA2 * filterY2;

    // 状態を1サンプル分シフト
    filterX2 = filterX1;
    filterX1 = x0;
    filterY2 = filterY1;
    filterY1 = y0;

    // クリッピング(レゾナンスを上げすぎた際のオーバーフロー対策)
    if (y0 > 32767.0f)  y0 = 32767.0f;
    if (y0 < -32768.0f) y0 = -32768.0f;
    return (int16_t)y0;
}

EnvPhase envPhase  = EnvPhase::IDLE;
float    envLevel  = 0.0f;  // 現在のエンベロープ値(0.0〜1.0)。audioTaskが直接読み書きする。
float    playingFreq = 0.0f; // 実際にオシレーターが鳴らす周波数(Release中も保持される)


// Tabキーで PLAY <-> SETTINGS をトグルする。
// 今後 EDIT(音色編集)メニューを追加する際は、ここに値を増やし、
// Tabキーの遷移順(PLAY -> EDIT -> SETTINGS -> PLAY)を更新する想定。
// ---------------------------------------------------------
// アプリのモード（画面/メニュー切り替え）
// ---------------------------------------------------------
// Tabキーで PLAY -> EDIT -> SETTINGS -> PLAY と順送りでトグルする。
enum class AppMode : uint8_t {
    PLAY,
    EDIT,
    SETTINGS,
};
AppMode appMode = AppMode::PLAY;
bool prevTabPressed = false;

// ---------------------------------------------------------
// 設定メニュー項目の定義
// ---------------------------------------------------------
// SETTINGS画面で「;」「.」キーにより上下選択し、「,」「/」キーで値を増減する。
// 各項目は「どの変数を」「どんな刻み幅で」「どう表示するか」を持つ。
// IMU軸のターゲットのように enum を切り替える項目と、
// ベンド幅のように float の数値を増減する項目の両方に対応するため、
// 関数ポインタ(onIncrement/onDecrement)とラベル取得関数で抽象化する。
// 前方宣言: 定義は本ファイル下部(SD保存処理)にあるが、
// updateMenuNavigation()内のTabキー処理(SETTINGS->PLAY遷移時の自動保存)から
// 呼び出すため、ここで宣言だけ先に行っておく。
bool saveSettings();

struct SettingItem {
    const char *name;                  // 項目名(画面表示用)
    void (*onIncrement)();             // ">" 方向に値を変える処理
    void (*onDecrement)();             // "<" 方向に値を変える処理
    const char *(*valueLabel)();       // 現在値の表示文字列を返す処理
};

// ---------------------------------------------------------
// 指定したImuTargetに対応するパラメータを「無傾き時のデフォルト値」に
// リセットする。IMU割り当てを切り替えた際、それまで傾けて変化させていた
// 数値が残り続けてしまう問題への対処。
// Target値(目標値)と現在値の両方をリセットする
// (Target値だけ戻すと、audioTask側の平滑化により古い値からゆっくり
//  動いてしまい、リセットされたように見えないため)。
// ---------------------------------------------------------
void resetParamToDefault(ImuTarget t) {
    switch (t) {
        case ImuTarget::TIMBRE:
            params.timbreMorph = params.timbreMorphTarget = 0.0f;
            break;
        case ImuTarget::VIBRATO_DEPTH:
            params.vibratoDepth = params.vibratoDepthTarget = 0.0f;
            break;
        case ImuTarget::VIBRATO_RATE:
            params.vibratoRateHz = params.vibratoRateHzTarget = 5.0f; // 標準速度
            break;
        case ImuTarget::TREMOLO:
            params.tremoloDepth = params.tremoloDepthTarget = 0.0f;
            break;
        case ImuTarget::VOLUME:
            params.volumeOffset = params.volumeOffsetTarget = 0.0f;
            break;
        case ImuTarget::PITCH_BEND:
            params.pitchBendCents = params.pitchBendCentsTarget = 0.0f;
            break;
        case ImuTarget::BEND_UP:
            params.pitchBendCents = params.pitchBendCentsTarget = 0.0f;
            break;
        case ImuTarget::BEND_DOWN:
            params.pitchBendCents = params.pitchBendCentsTarget = 0.0f;
            break;
        case ImuTarget::BITCRUSH:
            params.bitcrush = params.bitcrushTarget = 0.0f;
            break;
        case ImuTarget::FILTER_CUTOFF:
            params.filterCutoffOffset = params.filterCutoffOffsetTarget = 0.0f;
            break;
        case ImuTarget::NONE:
        default:
            break; // 何もしない
    }
}

// --- IMU X軸ターゲットの増減 ---
// 切り替え前に古いターゲットのパラメータをリセットしてから切り替える。
// (新しいターゲット側は、IMUの次回更新時に現在の傾きに応じた値へ
//  即座に上書きされるため、こちらは特にリセット不要)
void imuXNext() {
    resetParamToDefault(imuAxisX.target); // 切り替え前のパラメータをリセット
    uint8_t v = (uint8_t)imuAxisX.target;
    v = (v + 1) % (uint8_t)ImuTarget::TARGET_COUNT;
    imuAxisX.target = (ImuTarget)v;
}
void imuXPrev() {
    resetParamToDefault(imuAxisX.target);
    uint8_t v = (uint8_t)imuAxisX.target;
    v = (v == 0) ? (uint8_t)ImuTarget::TARGET_COUNT - 1 : v - 1;
    imuAxisX.target = (ImuTarget)v;
}
const char *imuXLabel() { return imuTargetName(imuAxisX.target); }

// --- IMU Y軸ターゲットの増減 ---
void imuYNext() {
    resetParamToDefault(imuAxisY.target);
    uint8_t v = (uint8_t)imuAxisY.target;
    v = (v + 1) % (uint8_t)ImuTarget::TARGET_COUNT;
    imuAxisY.target = (ImuTarget)v;
}
void imuYPrev() {
    resetParamToDefault(imuAxisY.target);
    uint8_t v = (uint8_t)imuAxisY.target;
    v = (v == 0) ? (uint8_t)ImuTarget::TARGET_COUNT - 1 : v - 1;
    imuAxisY.target = (ImuTarget)v;
}
const char *imuYLabel() { return imuTargetName(imuAxisY.target); }

// --- ベンド幅(半音単位で増減。内部はセント=半音x100で保持) ---
void bendWidthInc() {
    keyBendMaxCents += 100.0f; // 1半音ずつ
    if (keyBendMaxCents > 1200.0f) keyBendMaxCents = 1200.0f; // 最大1オクターブ
}
void bendWidthDec() {
    keyBendMaxCents -= 100.0f;
    if (keyBendMaxCents < 0.0f) keyBendMaxCents = 0.0f;
}
char bendWidthLabelBuf[16];
const char *bendWidthLabel() {
    snprintf(bendWidthLabelBuf, sizeof(bendWidthLabelBuf), "%.1f st", keyBendMaxCents / 100.0f);
    return bendWidthLabelBuf;
}

// --- ベンドのしゃくり上げ速度(アタック) ---
// 値が小さいほどゆっくり(しゃくり上げが強調される)、大きいほど速い。
// 操作上は「速さ」として直感的に表示したいので、増減は逆方向(増やす=速くする)にする。
void bendAttackInc() {
    keyBendAttackSmooth *= 1.3f;
    if (keyBendAttackSmooth > 0.01f) keyBendAttackSmooth = 0.01f;
}
void bendAttackDec() {
    keyBendAttackSmooth /= 1.3f;
    if (keyBendAttackSmooth < 0.00005f) keyBendAttackSmooth = 0.00005f;
}
char bendAttackLabelBuf[16];
const char *bendAttackLabel() {
    snprintf(bendAttackLabelBuf, sizeof(bendAttackLabelBuf), "%.4f", keyBendAttackSmooth);
    return bendAttackLabelBuf;
}

// --- ベンドのリリース速度 ---
void bendReleaseInc() {
    keyBendReleaseSmooth *= 1.3f;
    if (keyBendReleaseSmooth > 0.02f) keyBendReleaseSmooth = 0.02f;
}
void bendReleaseDec() {
    keyBendReleaseSmooth /= 1.3f;
    if (keyBendReleaseSmooth < 0.0005f) keyBendReleaseSmooth = 0.0005f;
}
char bendReleaseLabelBuf[16];
const char *bendReleaseLabel() {
    snprintf(bendReleaseLabelBuf, sizeof(bendReleaseLabelBuf), "%.4f", keyBendReleaseSmooth);
    return bendReleaseLabelBuf;
}

// 設定項目リスト本体。項目を追加したい場合はここに1行足すだけでよい。
SettingItem settingItems[] = {
    { "IMU X axis",    imuXNext,      imuXPrev,      imuXLabel },
    { "IMU Y axis",    imuYNext,      imuYPrev,      imuYLabel },
    { "Bend width",    bendWidthInc,  bendWidthDec,  bendWidthLabel },
    { "Bend attack",   bendAttackInc, bendAttackDec, bendAttackLabel },
    { "Bend release",  bendReleaseInc,bendReleaseDec,bendReleaseLabel },
};
const int NUM_SETTING_ITEMS = sizeof(settingItems) / sizeof(settingItems[0]);
int selectedSettingIndex = 0;

// ---------------------------------------------------------
// EDITメニュー項目: ADSRエンベロープ
// ---------------------------------------------------------
// A/D/Rは時間(秒)を0.0〜5.0の範囲で0.05秒刻みに、Sustainは0〜100%を5%刻みに増減する。
constexpr float ADSR_TIME_STEP = 0.05f;

void adsrAttackInc()  { adsr.attackTime  += ADSR_TIME_STEP; if (adsr.attackTime  > ADSR_MAX_TIME) adsr.attackTime  = ADSR_MAX_TIME; }
void adsrAttackDec()  { adsr.attackTime  -= ADSR_TIME_STEP; if (adsr.attackTime  < ADSR_MIN_TIME) adsr.attackTime  = ADSR_MIN_TIME; }
void adsrDecayInc()   { adsr.decayTime   += ADSR_TIME_STEP; if (adsr.decayTime   > ADSR_MAX_TIME) adsr.decayTime   = ADSR_MAX_TIME; }
void adsrDecayDec()   { adsr.decayTime   -= ADSR_TIME_STEP; if (adsr.decayTime   < ADSR_MIN_TIME) adsr.decayTime   = ADSR_MIN_TIME; }
void adsrSustainInc() { adsr.sustainLevel += 0.05f; if (adsr.sustainLevel > 1.0f) adsr.sustainLevel = 1.0f; }
void adsrSustainDec() { adsr.sustainLevel -= 0.05f; if (adsr.sustainLevel < 0.0f) adsr.sustainLevel = 0.0f; }
void adsrReleaseInc() { adsr.releaseTime += ADSR_TIME_STEP; if (adsr.releaseTime > ADSR_MAX_TIME) adsr.releaseTime = ADSR_MAX_TIME; }
void adsrReleaseDec() { adsr.releaseTime -= ADSR_TIME_STEP; if (adsr.releaseTime < ADSR_MIN_TIME) adsr.releaseTime = ADSR_MIN_TIME; }

char adsrLabelBuf[16];
const char *adsrAttackLabel()  { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.attackTime);  return adsrLabelBuf; }
const char *adsrDecayLabel()   { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.decayTime);   return adsrLabelBuf; }
const char *adsrSustainLabel() { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%d%%",  (int)(adsr.sustainLevel * 100)); return adsrLabelBuf; }
const char *adsrReleaseLabel() { snprintf(adsrLabelBuf, sizeof(adsrLabelBuf), "%.2fs", adsr.releaseTime);  return adsrLabelBuf; }

// --- フィルタータイプ(LPF/HPF/BPF/Notch)の切り替え ---
const char *filterTypeName(FilterType t) {
    switch (t) {
        case FilterType::LPF:   return "LPF";
        case FilterType::HPF:   return "HPF";
        case FilterType::BPF:   return "BPF";
        case FilterType::NOTCH: return "Notch";
        default:                return "?";
    }
}
void filterTypeNext() {
    uint8_t v = (uint8_t)filterParams.type;
    v = (v + 1) % 4; // FilterTypeは4種類(LPF/HPF/BPF/NOTCH)
    filterParams.type = (FilterType)v;
    updateFilterCoefficients();
}
void filterTypePrev() {
    uint8_t v = (uint8_t)filterParams.type;
    v = (v == 0) ? 3 : v - 1;
    filterParams.type = (FilterType)v;
    updateFilterCoefficients();
}
char filterTypeLabelBuf[8];
const char *filterTypeLabel() {
    snprintf(filterTypeLabelBuf, sizeof(filterTypeLabelBuf), "%s", filterTypeName(filterParams.type));
    return filterTypeLabelBuf;
}

// --- カットオフ周波数(100Hz刻みで増減) ---
constexpr float FILTER_CUTOFF_STEP = 100.0f;
void filterCutoffInc() {
    filterParams.cutoffHz += FILTER_CUTOFF_STEP;
    if (filterParams.cutoffHz > FILTER_CUTOFF_MAX) filterParams.cutoffHz = FILTER_CUTOFF_MAX;
    updateFilterCoefficients();
}
void filterCutoffDec() {
    filterParams.cutoffHz -= FILTER_CUTOFF_STEP;
    if (filterParams.cutoffHz < FILTER_CUTOFF_MIN) filterParams.cutoffHz = FILTER_CUTOFF_MIN;
    updateFilterCoefficients();
}
char filterCutoffLabelBuf[16];
const char *filterCutoffLabel() {
    snprintf(filterCutoffLabelBuf, sizeof(filterCutoffLabelBuf), "%.0fHz", filterParams.cutoffHz);
    return filterCutoffLabelBuf;
}

// --- レゾナンス(Q値、0.1刻み) ---
void filterResonanceInc() {
    filterParams.resonanceQ += 0.1f;
    if (filterParams.resonanceQ > FILTER_Q_MAX) filterParams.resonanceQ = FILTER_Q_MAX;
    updateFilterCoefficients();
}
void filterResonanceDec() {
    filterParams.resonanceQ -= 0.1f;
    if (filterParams.resonanceQ < FILTER_Q_MIN) filterParams.resonanceQ = FILTER_Q_MIN;
    updateFilterCoefficients();
}
char filterResonanceLabelBuf[16];
const char *filterResonanceLabel() {
    snprintf(filterResonanceLabelBuf, sizeof(filterResonanceLabelBuf), "Q%.1f", filterParams.resonanceQ);
    return filterResonanceLabelBuf;
}

// EDITメニュー項目リスト。ADSR4項目 + フィルター3項目。
SettingItem editItems[] = {
    { "Attack",   adsrAttackInc,    adsrAttackDec,    adsrAttackLabel },
    { "Decay",    adsrDecayInc,     adsrDecayDec,     adsrDecayLabel },
    { "Sustain",  adsrSustainInc,   adsrSustainDec,   adsrSustainLabel },
    { "Release",  adsrReleaseInc,   adsrReleaseDec,   adsrReleaseLabel },
    { "Filter",   filterTypeNext,   filterTypePrev,   filterTypeLabel },
    { "Cutoff",   filterCutoffInc,  filterCutoffDec,  filterCutoffLabel },
    { "Resonance",filterResonanceInc, filterResonanceDec, filterResonanceLabel },
};
const int NUM_EDIT_ITEMS = sizeof(editItems) / sizeof(editItems[0]);
int selectedEditIndex = 0;

// メニュー操作キー用エッジ検出フラグ
bool prevMenuUpPressed    = false; // ';'
bool prevMenuDownPressed  = false; // '.'
bool prevMenuIncPressed   = false; // '/'
bool prevMenuDecPressed   = false; // ','

// ---------------------------------------------------------
// 波形テーブルの初期化
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

// ---------------------------------------------------------
// 波形モーフィングサンプル取得
// ---------------------------------------------------------
int16_t getMorphedSample(int idx, float morph) {
    if (morph < 0.0f) morph = 0.0f;
    if (morph > 3.0f) morph = 3.0f;
    int16_t *tables[4] = {sineTable, triangleTable, sawtoothTable, squareTable};
    int loIdx = (int)morph;
    if (loIdx > 2) loIdx = 2;
    float frac = morph - (float)loIdx;
    return (int16_t)(tables[loIdx][idx]*(1.0f-frac) + tables[loIdx+1][idx]*frac);
}

// ---------------------------------------------------------
// ビットクラッシャー
// ---------------------------------------------------------
// crush=0.0: 原音(16bit)  crush=1.0: 最大削減(約3bit相当)
// 量子化ビット数を下げることでレトロゲーム機風のザラついた音色になる。
int16_t applyBitcrush(int16_t sample, float crush) {
    if (crush <= 0.0f) return sample;
    // crush=1.0のとき 3bit(8段階)、0.0のとき 16bit(65536段階)
    float bits = 16.0f - crush * 13.0f; // 16〜3 bit
    float levels = powf(2.0f, bits);
    float normalized = sample / 32768.0f;
    float quantized  = roundf(normalized * levels) / levels;
    return (int16_t)(quantized * 32768.0f);
}

// ---------------------------------------------------------
// ADSRエンベロープの1サンプル分の進行処理
// ---------------------------------------------------------
// envPhase / envLevel / playingFreq を更新する。
// keyHeld: 現在音階キーが押されているか(true)離されているか(false)。
//
// 線形変化の式: 1秒間に levelStep だけ進むように、サンプルあたりの
// 増分を「1.0 / (time[秒] * SAMPLE_RATE)」で計算する。
// time=0の場合はゼロ除算を避けるため、実質1サンプルで遷移させる。
void advanceEnvelope(bool keyHeld) {
    switch (envPhase) {
        case EnvPhase::IDLE:
            // 何もしない。noteOnトリガはupdateOctaveAndVolume側でenvPhaseを
            // ATTACKに変更することで開始する。
            break;

        case EnvPhase::ATTACK: {
            float step = (adsr.attackTime <= 0.0f)
                ? 1.0f
                : (1.0f / (adsr.attackTime * SAMPLE_RATE));
            envLevel += step;
            if (envLevel >= 1.0f) {
                envLevel = 1.0f;
                envPhase = EnvPhase::DECAY;
            }
            if (!keyHeld) envPhase = EnvPhase::RELEASE; // Attack中に離されたらRelease開始
            break;
        }

        case EnvPhase::DECAY: {
            float step = (adsr.decayTime <= 0.0f)
                ? 1.0f
                : ((1.0f - adsr.sustainLevel) / (adsr.decayTime * SAMPLE_RATE));
            envLevel -= step;
            if (envLevel <= adsr.sustainLevel) {
                envLevel = adsr.sustainLevel;
                envPhase = EnvPhase::SUSTAIN;
            }
            if (!keyHeld) envPhase = EnvPhase::RELEASE;
            break;
        }

        case EnvPhase::SUSTAIN:
            envLevel = adsr.sustainLevel; // キーを押し続けている間は一定
            if (!keyHeld) envPhase = EnvPhase::RELEASE;
            break;

        case EnvPhase::RELEASE: {
            // releaseTime秒かけて1.0から0.0まで下がる前提の固定減少幅を使う
            // (現在のenvLevelの大小によらず一定速度で下がるシンプルな実装。
            //  Decay直後など既にenvLevelがsustainLevel程度に下がっている場合、
            //  実際の消音にかかる時間はreleaseTimeよりやや短くなる)。
            float simpleStep = 1.0f / fmaxf(adsr.releaseTime * SAMPLE_RATE, 1.0f);
            envLevel -= simpleStep;
            if (envLevel <= 0.0f) {
                envLevel = 0.0f;
                envPhase = EnvPhase::IDLE;
                playingFreq = 0.0f; // 完全に無音になったらオシレーターも停止
            }
            if (keyHeld) {
                // Release中に再度キーが押された場合、現在のenvLevelから
                // Attackを再開する(ゼロから上げ直すと不自然なため)。
                envPhase = EnvPhase::ATTACK;
            }
            break;
        }
    }
}

// ---------------------------------------------------------
// audioTask: 波形生成 + 全エフェクト処理
// ---------------------------------------------------------
void audioTask(void *pvParameters) {
    const int bufferSamples = 1024;
    static int16_t bufs[3][bufferSamples];
    int bufIndex = 0;
    constexpr int CH = 0;

    // 平滑化係数(追従速度)。大きいほど速く反応するがノイズが出やすくなる。
    constexpr float SM = 0.0008f;

    while (true) {
        int16_t *buf = bufs[bufIndex];
        bufIndex = (bufIndex + 1) % 3;

        // ADSRがIDLEかつキーも押されていない場合のみ完全な無音区間として扱う。
        // (Release中はキーを離していてもまだ発音中なので、ここではbuf生成を続ける)
        bool keyHeld = (currentFreq > 0.0f);
        if (keyHeld) playingFreq = currentFreq; // 押している間は常に最新の周波数に追従

        if (envPhase != EnvPhase::IDLE || keyHeld) {
            for (int i = 0; i < bufferSamples; i++) {
                // --- 各パラメータをなめらかに目標値へ近づける ---
                params.timbreMorph    += (params.timbreMorphTarget    - params.timbreMorph)    * SM;
                params.vibratoDepth   += (params.vibratoDepthTarget   - params.vibratoDepth)   * SM;
                params.vibratoRateHz  += (params.vibratoRateHzTarget  - params.vibratoRateHz)  * SM;
                params.tremoloDepth   += (params.tremoloDepthTarget   - params.tremoloDepth)   * SM;
                params.volumeOffset   += (params.volumeOffsetTarget   - params.volumeOffset)   * SM;
                params.pitchBendCents += (params.pitchBendCentsTarget - params.pitchBendCents) * SM;
                params.bitcrush       += (params.bitcrushTarget       - params.bitcrush)       * SM;
                params.filterCutoffOffset += (params.filterCutoffOffsetTarget - params.filterCutoffOffset) * SM;

                // --- ADSRエンベロープを1サンプル進める ---
                advanceEnvelope(keyHeld);

                // --- ビブラートLFO ---
                int vibIdx = ((int)vibratoPhase) % WAVE_TABLE_SIZE;
                if (vibIdx < 0) vibIdx += WAVE_TABLE_SIZE;
                float vibratoLfo = sineTable[vibIdx] / 32000.0f;
                vibratoPhase += (double)WAVE_TABLE_SIZE * params.vibratoRateHz / SAMPLE_RATE;
                if (vibratoPhase >= WAVE_TABLE_SIZE) vibratoPhase -= WAVE_TABLE_SIZE;

                // --- トレモロLFO(ビブラートと同じ5Hzで揺らす) ---
                int tremIdx = ((int)tremoloPhase) % WAVE_TABLE_SIZE;
                if (tremIdx < 0) tremIdx += WAVE_TABLE_SIZE;
                float tremoloLfo = (sineTable[tremIdx] / 32000.0f + 1.0f) * 0.5f; // 0〜1
                tremoloPhase += (double)WAVE_TABLE_SIZE * 5.0f / SAMPLE_RATE;
                if (tremoloPhase >= WAVE_TABLE_SIZE) tremoloPhase -= WAVE_TABLE_SIZE;

                // --- ベンドキーの平滑化（アタック/リリースで速度を変える） ---
                float bendDiff = keyBendGoal - keyBendCurrent;
                float bendSmooth = (fabsf(keyBendGoal) < fabsf(keyBendCurrent) || keyBendGoal == 0.0f)
                                   ? keyBendReleaseSmooth
                                   : keyBendAttackSmooth;
                keyBendCurrent += bendDiff * bendSmooth;

                // --- 合計ピッチオフセット(ビブラート + IMUピッチベンド + キーベンド) ---
                float totalCents = vibratoLfo * params.vibratoDepth * VIBRATO_MAX_CENTS
                                 + params.pitchBendCents  // IMU由来
                                 + keyBendCurrent;        // Z/Xキー由来（平滑化済み）
                float pitchRatio  = powf(2.0f, totalCents / 1200.0f);
                // playingFreqを使う: Release中もノートオフ時点の周波数で鳴り続ける
                double phaseInc   = (double)WAVE_TABLE_SIZE * (playingFreq * pitchRatio) / SAMPLE_RATE;

                // --- 波形取得 ---
                int idx = ((int)phase) % WAVE_TABLE_SIZE;
                if (idx < 0) idx += WAVE_TABLE_SIZE;
                int16_t sample = getMorphedSample(idx, params.timbreMorph);

                // --- ビットクラッシャー ---
                sample = applyBitcrush(sample, params.bitcrush);

                // --- フィルター (Biquad) ---
                // IMUでFILTER_CUTOFFが割り当てられている場合、傾きに応じて
                // EDITメニューで設定した基準カットオフ(filterParams.cutoffHz)から
                // 下方向にオフセットをかける(傾けるほどこもる方向)。
                // IMU未割り当て時はfilterCutoffOffsetが常に0なので基準値のまま動作する。
                if (params.filterCutoffOffset > 0.0001f) {
                    float dynamicCutoff = filterParams.cutoffHz * (1.0f - params.filterCutoffOffset * 0.9f);
                    if (dynamicCutoff < FILTER_CUTOFF_MIN) dynamicCutoff = FILTER_CUTOFF_MIN;
                    // 係数を都度再計算する(三角関数を含むためやや重いが、
                    // ESP32-S3の処理能力であれば1024サンプル/バッファでも問題ない想定)
                    float savedCutoff = filterParams.cutoffHz;
                    filterParams.cutoffHz = dynamicCutoff;
                    updateFilterCoefficients();
                    filterParams.cutoffHz = savedCutoff; // EDITメニュー上の表示値は変えない
                }
                sample = applyFilter(sample);

                // --- 音量 (keyVolume + IMU音量オフセット + トレモロ + ADSRエンベロープ) ---
                float totalVolume = params.keyVolume + params.volumeOffset;
                if (totalVolume < 0.0f) totalVolume = 0.0f;
                if (totalVolume > 1.0f) totalVolume = 1.0f;
                // トレモロ: depth=0のとき音量変化なし、depth=1のとき0〜2倍で揺れる
                float tremoloGain = 1.0f - params.tremoloDepth + params.tremoloDepth * tremoloLfo * 2.0f;
                if (tremoloGain < 0.0f) tremoloGain = 0.0f;
                if (tremoloGain > 2.0f) tremoloGain = 2.0f;

                buf[i] = (int16_t)(sample * totalVolume * tremoloGain * envLevel);

                phase += phaseInc;
                if (phase >= WAVE_TABLE_SIZE) phase -= WAVE_TABLE_SIZE;
            }
            M5Cardputer.Speaker.playRaw(buf, bufferSamples, SAMPLE_RATE, false, 1, CH, false);
        } else {
            vTaskDelay(5 / portTICK_PERIOD_MS);
            phase = 0.0;
        }
    }
}

// ---------------------------------------------------------
// 音階キー → 周波数
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
// オクターブ・音量・ベンドキー処理（PLAYモード中のみ有効）
// ---------------------------------------------------------
void updateOctaveAndVolume() {
    // SETTINGSメニュー表示中は演奏キーを無効化する
    // (Tabで切り替えた瞬間に意図せず音階キーが反応しないようにするため)
    if (appMode != AppMode::PLAY) return;

    auto status = M5Cardputer.Keyboard.keysState();
    bool octUp = false, octDn = false, volUp = false, volDn = false;
    bool bendDn = false, bendUp = false;

    for (char c : status.word) {
        if (c == '=') octUp  = true;
        if (c == '-') octDn  = true;
        if (c == '.') volUp  = true;
        if (c == ',') volDn  = true;
        if (c == 'z') bendDn = true; // Zキー: ベンドダウン
        if (c == 'x') bendUp = true; // Xキー: ベンドアップ
    }

    // オクターブ: エッジトリガ
    if (octUp && !prevOctaveUpPressed   && params.octaveShift < 2)  params.octaveShift++;
    if (octDn && !prevOctaveDownPressed && params.octaveShift > -2) params.octaveShift--;
    prevOctaveUpPressed   = octUp;
    prevOctaveDownPressed = octDn;

    // 音量: エッジトリガ
    if (volUp && !prevVolumeUpPressed) {
        params.keyVolume += 0.05f;
        if (params.keyVolume > 1.0f) params.keyVolume = 1.0f;
    }
    if (volDn && !prevVolumeDownPressed) {
        params.keyVolume -= 0.05f;
        if (params.keyVolume < 0.0f) params.keyVolume = 0.0f;
    }
    prevVolumeUpPressed   = volUp;
    prevVolumeDownPressed = volDn;

    // ベンドキー: 押している間だけ目標値をセット、離すと0へ戻る
    // Z/X同時押しは相殺(0)とする。
    // keyBendGoalはaudioTask内で平滑化されてkeyBendCurrentに反映される。
    // アタック(押した時)はゆっくり上昇、リリース(離した時)は素早く戻る。
    if (bendDn && !bendUp) {
        keyBendGoal = -keyBendMaxCents;
    } else if (bendUp && !bendDn) {
        keyBendGoal = +keyBendMaxCents;
    } else {
        keyBendGoal = 0.0f;
    }
}

// ---------------------------------------------------------
// Tabキーによるモード切替 + EDIT/SETTINGSメニューのナビゲーション処理
// ---------------------------------------------------------
// Tab: PLAY -> EDIT -> SETTINGS -> PLAY と順送りでトグル
// EDIT/SETTINGSモード中（共通操作）:
//   ';' : 選択項目を上へ
//   '.' : 選択項目を下へ
//   '/' : 選択中の項目の値を増やす
//   ',' : 選択中の項目の値を減らす
void updateMenuNavigation() {
    // Tabキーは status.word には入らず、専用の bool フラグ status.tab で判定する
    auto status = M5Cardputer.Keyboard.keysState();
    bool tabPressed   = false;
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
    tabPressed = status.tab;

    // Tabキー: モード切替（PLAY -> EDIT -> SETTINGS -> PLAY の順送り）
    if (tabPressed && !prevTabPressed) {
        AppMode previousMode = appMode;

        switch (appMode) {
            case AppMode::PLAY:
                appMode = AppMode::EDIT;
                currentFreq = 0.0f;
                break;
            case AppMode::EDIT:
                appMode = AppMode::SETTINGS;
                break;
            case AppMode::SETTINGS:
                appMode = AppMode::PLAY;
                break;
        }

        if (previousMode == AppMode::SETTINGS && appMode == AppMode::PLAY) {
            saveSettings();
        }
    }
    prevTabPressed = tabPressed;

    // EDITモード中: ADSR項目の選択・値変更を処理する
    if (appMode == AppMode::EDIT) {
        if (menuUp && !prevMenuUpPressed) {
            selectedEditIndex--;
            if (selectedEditIndex < 0) selectedEditIndex = NUM_EDIT_ITEMS - 1;
        }
        if (menuDown && !prevMenuDownPressed) {
            selectedEditIndex++;
            if (selectedEditIndex >= NUM_EDIT_ITEMS) selectedEditIndex = 0;
        }
        if (menuIncrease && !prevMenuIncPressed) {
            editItems[selectedEditIndex].onIncrement();
        }
        if (menuDecrease && !prevMenuDecPressed) {
            editItems[selectedEditIndex].onDecrement();
        }
    }

    // SETTINGSモード中: IMU/ベンド項目の選択・値変更を処理する
    if (appMode == AppMode::SETTINGS) {
        if (menuUp && !prevMenuUpPressed) {
            selectedSettingIndex--;
            if (selectedSettingIndex < 0) selectedSettingIndex = NUM_SETTING_ITEMS - 1;
        }
        if (menuDown && !prevMenuDownPressed) {
            selectedSettingIndex++;
            if (selectedSettingIndex >= NUM_SETTING_ITEMS) selectedSettingIndex = 0;
        }
        if (menuIncrease && !prevMenuIncPressed) {
            settingItems[selectedSettingIndex].onIncrement();
        }
        if (menuDecrease && !prevMenuDecPressed) {
            settingItems[selectedSettingIndex].onDecrement();
        }
    }

    prevMenuUpPressed   = menuUp;
    prevMenuDownPressed = menuDown;
    prevMenuIncPressed  = menuIncrease;
    prevMenuDecPressed  = menuDecrease;
}

// ---------------------------------------------------------
// IMUマッピング: 正規化された軸値をターゲットパラメータに書き込む
// ---------------------------------------------------------
// value: bipolar=false なら 0.0〜1.0、bipolar=true なら -1.0〜+1.0
void applyImuValue(ImuTarget target, float value) {
    switch (target) {
        case ImuTarget::TIMBRE:
            params.timbreMorphTarget = value * 3.0f; // 0〜3.0
            break;
        case ImuTarget::VIBRATO_DEPTH:
            params.vibratoDepthTarget = value; // 0〜1.0
            break;
        case ImuTarget::VIBRATO_RATE:
            // value(0〜1) → 1〜10Hz にマッピング
            params.vibratoRateHzTarget = 1.0f + value * 9.0f;
            break;
        case ImuTarget::TREMOLO:
            params.tremoloDepthTarget = value; // 0〜1.0
            break;
        case ImuTarget::VOLUME:
            // bipolar想定: -1〜+1 → -0.5〜+0.5 の音量オフセット
            params.volumeOffsetTarget = value * 0.5f;
            break;
        case ImuTarget::PITCH_BEND:
            // bipolar想定: -1〜+1 → ±keyBendMaxCents
            // 傾ける方向で音程が上下する(ランダムピッチ遊び用)
            params.pitchBendCentsTarget = value * keyBendMaxCents;
            break;
        case ImuTarget::BEND_UP:
            // 絶対値(0〜1): 傾けるほど音程が上がる(方向問わず)
            params.pitchBendCentsTarget = fabsf(value) * keyBendMaxCents;
            break;
        case ImuTarget::BEND_DOWN:
            // 絶対値(0〜1): 傾けるほど音程が下がる(方向問わず)
            params.pitchBendCentsTarget = -fabsf(value) * keyBendMaxCents;
            break;
        case ImuTarget::BITCRUSH:
            params.bitcrushTarget = value; // 0〜1.0
            break;
        case ImuTarget::FILTER_CUTOFF:
            // value(0〜1、傾きの大きさ)をそのままオフセットとして保持。
            // audioTask側で「EDITメニューで設定したcutoffHzを基準に、
            // 傾けるほどカットオフを下げる(こもらせる)」方向にマッピングする。
            params.filterCutoffOffsetTarget = value; // 0〜1.0
            break;
        case ImuTarget::NONE:
        default:
            break;
    }
}

// ---------------------------------------------------------
// IMU読み取り → 各軸の正規化値を計算 → applyImuValue()
// ---------------------------------------------------------
// IMU生データ保持変数（drawImuPadで参照するためグローバルに保持）
float lastAccelX = 0.0f;
float lastAccelY = 0.0f;

void updateImu() {
    if (!M5.Imu.update()) return;
    auto data = M5.Imu.getImuData();
    float ax = data.accel.x;
    float ay = data.accel.y;
    lastAccelX = ax; // 描画用に保存
    lastAccelY = ay;

    auto clamp1 = [](float v){ return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); };
    float angleX = asinf(clamp1(ax)) * 180.0f / PI;
    float angleY = asinf(clamp1(ay)) * 180.0f / PI;

    // ターゲットに応じてbipolar(符号あり)かabsolute(絶対値)かを自動判定する。
    // PITCH_BENDは必ずbipolar(傾ける方向で上下)、
    // BEND_UP/BEND_DOWNは必ずabsolute(方向問わず効果の方向が固定)。
    // それ以外はImuAxisConfigのbipolar設定に従う。
    auto resolveBipolar = [](ImuTarget t, bool configBipolar) -> bool {
        if (t == ImuTarget::PITCH_BEND) return true;
        if (t == ImuTarget::BEND_UP || t == ImuTarget::BEND_DOWN) return false;
        return configBipolar;
    };

    // X軸
    if (imuAxisX.target != ImuTarget::NONE) {
        float norm = (angleX / TILT_MAX_DEGREES) * imuAxisX.sensitivity;
        norm = norm < -1.0f ? -1.0f : (norm > 1.0f ? 1.0f : norm);
        bool bipolar = resolveBipolar(imuAxisX.target, imuAxisX.bipolar);
        float val = bipolar ? norm : fabsf(norm);
        applyImuValue(imuAxisX.target, val);
    }
    // Y軸
    if (imuAxisY.target != ImuTarget::NONE) {
        float norm = (angleY / TILT_MAX_DEGREES) * imuAxisY.sensitivity;
        norm = norm < -1.0f ? -1.0f : (norm > 1.0f ? 1.0f : norm);
        bool bipolar = resolveBipolar(imuAxisY.target, imuAxisY.bipolar);
        float val = bipolar ? norm : fabsf(norm);
        applyImuValue(imuAxisY.target, val);
    }
}

// ---------------------------------------------------------
// IMUターゲット名(画面表示用)
// ---------------------------------------------------------
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
            // フィルタータイプ名も含めて表示 (例: "Filter(HPF)")
            static char filterBuf[16];
            snprintf(filterBuf, sizeof(filterBuf), "Filter(%s)", filterTypeName(filterParams.type));
            return filterBuf;
        }
        default:                       return "?";
    }
}

// ---------------------------------------------------------
// SD: 初期化 + /CPS フォルダ確認
// ---------------------------------------------------------
bool initSDCard() {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    return SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
}

bool ensureCpsFolder() {
    if (SD.exists(CPS_FOLDER_PATH)) return true;
    return SD.mkdir(CPS_FOLDER_PATH);
}

// ---------------------------------------------------------
// 設定の保存先パス
// ---------------------------------------------------------
static const char *SETTINGS_FILE_PATH = "/CPS/settings.json";

// ---------------------------------------------------------
// 設定をJSON形式でSDに保存する
// ---------------------------------------------------------
// 項目数が少なく固定フォーマットのため、ArduinoJson等は使わず
// 自前の単純なフォーマットで書き出す(依存ライブラリを増やさないため)。
// 値は1行1項目の "key": value 形式(末尾カンマなし最終行以外カンマ区切り)。
// 将来エディットメニュー(ADSR等)の項目が増えた場合もこの関数に追記していく想定。
bool saveSettings() {
    File f = SD.open(SETTINGS_FILE_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[Settings] failed to open file for write");
        return false;
    }

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
    f.printf("  \"filter_q\": %.2f\n",        filterParams.resonanceQ); // 最終行はカンマなし
    f.println("}");

    f.close();
    Serial.println("[Settings] saved to " + String(SETTINGS_FILE_PATH));
    return true;
}

// ---------------------------------------------------------
// 簡易JSON風パーサ: "key": value という行を1行ずつ読み取り、
// keyに応じた変数へ反映する。
// 本格的なJSONパーサではなく、このアプリが書き出すフォーマット専用の
// 単純な行ベースパーサ(汎用JSONとしての正しさは保証しない)。
// ---------------------------------------------------------
void parseSettingLine(const String &line) {
    // 例: "  \"imu_x_target\": 1," のような行から
    //   key = imu_x_target, valueStr = "1" を取り出す
    int firstQuote  = line.indexOf('"');
    if (firstQuote < 0) return;
    int secondQuote = line.indexOf('"', firstQuote + 1);
    if (secondQuote < 0) return;
    String key = line.substring(firstQuote + 1, secondQuote);

    int colonPos = line.indexOf(':', secondQuote);
    if (colonPos < 0) return;

    String valueStr = line.substring(colonPos + 1);
    valueStr.trim();
    if (valueStr.endsWith(",")) valueStr.remove(valueStr.length() - 1);
    valueStr.trim();
    if (valueStr.length() == 0) return;

    float value = valueStr.toFloat();

    if (key == "imu_x_target") {
        uint8_t v = (uint8_t)value;
        if (v < (uint8_t)ImuTarget::TARGET_COUNT) imuAxisX.target = (ImuTarget)v;
    } else if (key == "imu_y_target") {
        uint8_t v = (uint8_t)value;
        if (v < (uint8_t)ImuTarget::TARGET_COUNT) imuAxisY.target = (ImuTarget)v;
    } else if (key == "bend_max_cents") {
        keyBendMaxCents = value;
    } else if (key == "bend_attack") {
        keyBendAttackSmooth = value;
    } else if (key == "bend_release") {
        keyBendReleaseSmooth = value;
    } else if (key == "adsr_attack") {
        adsr.attackTime = value;
    } else if (key == "adsr_decay") {
        adsr.decayTime = value;
    } else if (key == "adsr_sustain") {
        adsr.sustainLevel = value;
    } else if (key == "adsr_release") {
        adsr.releaseTime = value;
    } else if (key == "filter_type") {
        uint8_t v = (uint8_t)value;
        if (v <= (uint8_t)FilterType::NOTCH) filterParams.type = (FilterType)v;
    } else if (key == "filter_cutoff") {
        filterParams.cutoffHz = value;
    } else if (key == "filter_q") {
        filterParams.resonanceQ = value;
    }
    // 未知のkeyは無視する(将来バージョンで項目が増減しても古い設定ファイルを壊さないため)
}

// ---------------------------------------------------------
// 設定をSDから読み込む。ファイルが無い場合は何もしない
// (その場合、ソースコード上のデフォルト値がそのまま使われる)。
// ---------------------------------------------------------
bool loadSettings() {
    if (!SD.exists(SETTINGS_FILE_PATH)) {
        Serial.println("[Settings] no settings file - using defaults");
        return false;
    }

    File f = SD.open(SETTINGS_FILE_PATH, FILE_READ);
    if (!f) {
        Serial.println("[Settings] failed to open file for read");
        return false;
    }

    while (f.available()) {
        String line = f.readStringUntil('\n');
        parseSettingLine(line);
    }
    f.close();

    // 読み込んだフィルターパラメータを実際の係数に反映する
    // (cutoffHz/resonanceQ/typeを読み込んだだけでは係数が更新されないため)
    updateFilterCoefficients();

    Serial.println("[Settings] loaded from " + String(SETTINGS_FILE_PATH));
    Serial.printf("[Settings] X=%s Y=%s bendMax=%.1fc attack=%.6f release=%.6f\n",
        imuTargetName(imuAxisX.target), imuTargetName(imuAxisY.target),
        keyBendMaxCents, keyBendAttackSmooth, keyBendReleaseSmooth);
    Serial.printf("[Settings] ADSR A=%.2fs D=%.2fs S=%d%% R=%.2fs\n",
        adsr.attackTime, adsr.decayTime, (int)(adsr.sustainLevel * 100), adsr.releaseTime);
    Serial.printf("[Settings] Filter type=%s cutoff=%.0fHz Q=%.2f\n",
        filterTypeName(filterParams.type), filterParams.cutoffHz, filterParams.resonanceQ);
    return true;
}

// ---------------------------------------------------------
// setup
// ---------------------------------------------------------
void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextColor(GREEN, BLACK); // 背景色BLACKを明示することで
                                                     // スペース文字でも前の文字が消え、
                                                     // 文字の重なりとちらつきを両立して防ぐ
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("CPS - CardPuter Synth", 10, 10);
    M5Cardputer.Display.drawString("Keys 1-0: notes  =/- oct  ,/. vol", 10, 25);

    buildWaveTables();
    updateFilterCoefficients(); // フィルター係数の初期値を計算しておく

    // SD初期化
    bool sdOk = initSDCard();
    if (sdOk) {
        ensureCpsFolder();
        M5Cardputer.Display.drawString("SD: OK", 10, 40);
        Serial.println("[SD] OK - /CPS folder ready");

        // 保存済みの設定があれば読み込む(無ければデフォルト値のまま)
        loadSettings();
    } else {
        M5Cardputer.Display.drawString("SD: none (defaults)", 10, 40);
        Serial.println("[SD] not found - using defaults");
    }

    // IMU初期化
    bool imuOk = M5.Imu.begin();
    Serial.println(imuOk ? "[IMU] OK" : "[IMU] not found");

    // IMUマッピング表示（初期設定の確認用）
    Serial.printf("[IMU] X -> %s  Y -> %s\n",
        imuTargetName(imuAxisX.target), imuTargetName(imuAxisY.target));

    // スピーカー設定
    auto spk_cfg = M5Cardputer.Speaker.config();
    spk_cfg.sample_rate      = SAMPLE_RATE;
    spk_cfg.dma_buf_count    = 4;
    spk_cfg.dma_buf_len      = 512;
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    M5Cardputer.Speaker.config(spk_cfg);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(255);

    xTaskCreatePinnedToCore(audioTask, "audioTask", 4096, nullptr, 5, nullptr, 1);
}

// ---------------------------------------------------------
// EDIT画面の描画: ADSRエンベロープのグラフ + 項目リスト
// ---------------------------------------------------------
// 画面上半分にA/D/S/Rの形を折れ線で描画し、下半分に数値リストを表示する。
// グラフは横軸=時間、縦軸=音量(0〜1)。
// A/D/Rの時間は最大5秒だが、グラフ上はA+D+R(+Sの表示用の固定幅)を
// 画面幅いっぱいに収まるよう比率で縮小して描画する(実時間スケールではない)。
void drawAdsrGraph() {
    constexpr int GRAPH_X = 10;
    constexpr int GRAPH_Y = 14;
    constexpr int GRAPH_W = 220;
    constexpr int GRAPH_H = 38;
    constexpr int GRAPH_BOTTOM = GRAPH_Y + GRAPH_H;

    // 枠を描画
    M5Cardputer.Display.drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, DARKGREY);

    // Sustain区間の表示幅は固定(時間の概念がないため画面上の見た目の幅として確保)
    constexpr float SUSTAIN_DISPLAY_WIDTH = 0.25f; // グラフ全体に対する比率

    float totalTime = adsr.attackTime + adsr.decayTime + adsr.releaseTime;
    if (totalTime < 0.01f) totalTime = 0.01f; // ゼロ除算防止

    // A/D/Rそれぞれの時間をグラフ幅に占める割合に変換
    // (Sustain分の表示幅をあらかじめ差し引いた残りをA/D/Rで配分する)
    float adrWidth = GRAPH_W * (1.0f - SUSTAIN_DISPLAY_WIDTH);
    float aWidth = adrWidth * (adsr.attackTime / totalTime);
    float dWidth = adrWidth * (adsr.decayTime  / totalTime);
    float rWidth = adrWidth * (adsr.releaseTime/ totalTime);
    float sWidth = GRAPH_W * SUSTAIN_DISPLAY_WIDTH;

    // 各点のXY座標を計算(Y座標は画面座標系なので「上が0、下が1」になるよう反転)
    int x0 = GRAPH_X;                                  int y0 = GRAPH_BOTTOM;            // 開始(無音)
    int x1 = x0 + (int)aWidth;                          int y1 = GRAPH_Y;                 // Attack頂点(最大音量)
    int x2 = x1 + (int)dWidth;                          int y2 = GRAPH_BOTTOM - (int)(GRAPH_H * adsr.sustainLevel); // Decay終点(Sustainレベル)
    int x3 = x2 + (int)sWidth;                          int y3 = y2;                      // Sustain終点(同じ高さを維持)
    int x4 = x3 + (int)rWidth;                          int y4 = GRAPH_BOTTOM;             // Release終点(無音)

    // 折れ線を描画(GREEN系で統一)
    M5Cardputer.Display.drawLine(x0, y0, x1, y1, GREEN);
    M5Cardputer.Display.drawLine(x1, y1, x2, y2, GREEN);
    M5Cardputer.Display.drawLine(x2, y2, x3, y3, GREEN);
    M5Cardputer.Display.drawLine(x3, y3, x4, y4, GREEN);

    // 各フェーズの頂点に小さい点を打って見やすくする
    M5Cardputer.Display.fillCircle(x1, y1, 2, YELLOW);
    M5Cardputer.Display.fillCircle(x2, y2, 2, YELLOW);
    M5Cardputer.Display.fillCircle(x3, y3, 2, YELLOW);

    // 区間ラベル(A/D/S/R)をグラフ下に表示
    M5Cardputer.Display.setCursor(x0 + (x1 - x0) / 2 - 3, GRAPH_BOTTOM + 2);
    M5Cardputer.Display.print("A");
    M5Cardputer.Display.setCursor(x1 + (x2 - x1) / 2 - 3, GRAPH_BOTTOM + 2);
    M5Cardputer.Display.print("D");
    M5Cardputer.Display.setCursor(x2 + (x3 - x2) / 2 - 3, GRAPH_BOTTOM + 2);
    M5Cardputer.Display.print("S");
    M5Cardputer.Display.setCursor(x3 + (x4 - x3) / 2 - 3, GRAPH_BOTTOM + 2);
    M5Cardputer.Display.print("R");
}

// ---------------------------------------------------------
// 共通タブバー描画
// ---------------------------------------------------------
// 画面最上部(y=0〜10)にMAIN/SETTING/EDITのタブを表示し、
// 現在のモードを反転表示（白背景+黒文字）で強調する。
void drawTabBar(AppMode current) {
    // タブバー背景
    M5Cardputer.Display.fillRect(0, 0, 240, 11, BLACK);

    struct { const char *label; AppMode mode; int x; int w; } tabs[] = {
        { "MAIN",    AppMode::PLAY,     0,   80 },
        { "EDIT",    AppMode::EDIT,     80,  80 },
        { "SETTING", AppMode::SETTINGS, 160, 80 },
    };

    for (auto &t : tabs) {
        bool active = (current == t.mode);
        // 区切り線
        M5Cardputer.Display.drawRect(t.x, 0, t.w, 11, GREEN);
        if (active) {
            // アクティブタブ: 白背景+黒文字
            M5Cardputer.Display.fillRect(t.x + 1, 1, t.w - 2, 9, WHITE);
            M5Cardputer.Display.setTextColor(BLACK, WHITE);
        } else {
            M5Cardputer.Display.setTextColor(GREEN, BLACK);
        }
        // ラベルをタブ内に中央揃えで表示
        int labelLen = strlen(t.label) * 6; // フォントサイズ1で約6px/文字
        int lx = t.x + (t.w - labelLen) / 2;
        M5Cardputer.Display.setCursor(lx, 2);
        M5Cardputer.Display.print(t.label);
    }
    M5Cardputer.Display.setTextColor(GREEN, BLACK); // 色をデフォルトに戻す
}

// ---------------------------------------------------------
// PLAY画面の描画
// ---------------------------------------------------------
// モックアップのレイアウトに基づく:
//   Y: 0- 10  タブバー
//   Y:11- 55  リアルタイム波形グラフ (44px高)
//   Y:56-107  情報エリア
//     左(X:0-72):   音程ブロック（音名+周波数+オクターブ）
//     中(X:73-90):  ベンドメーター
//     右(X:91-239): IMUパッド + パラメータ情報
//   Y:108-113 区切り線
//   Y:114-134 操作ナビ
// ---------------------------------------------------------

// 音名を返す(周波数から計算)
// playingFreqが0の場合は"---"を返す
const char *getNoteName(float freq) {
    if (freq <= 0.0f) return "---";
    static const char *noteNames[] = {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };
    // A4=440Hzを基準に半音単位でノート番号を計算
    int noteNum = (int)roundf(12.0f * log2f(freq / 440.0f)) + 69;
    int octave  = noteNum / 12 - 1;
    int name    = noteNum % 12;
    if (name < 0) name += 12;
    static char buf[8];
    snprintf(buf, sizeof(buf), "%s%d", noteNames[name], octave);
    return buf;
}

// 波形グラフ描画(1周期分をグラフ領域に表示)
// getMorphedSample()を使い、現在のtimbreMorphを反映した波形を描く。
// リアルタイムに変化する位相を追いかけると速く動いて読みにくいため、
// 常に位相0から1周期を描く静止波形表示とする。
void drawWaveform() {
    constexpr int GX = 0;   // グラフ左端X
    constexpr int GY = 12;  // グラフ上端Y
    constexpr int GW = 240; // グラフ幅
    constexpr int GH = 43;  // グラフ高さ
    constexpr int CY = GY + GH / 2; // 中心Y

    // 前回描画の消去(行単位fillRectでちらつきを抑える)
    M5Cardputer.Display.fillRect(GX, GY, GW, GH, BLACK);

    // 中心線(暗めの緑で細く表示)
    M5Cardputer.Display.drawFastHLine(GX, CY, GW, M5Cardputer.Display.color565(0, 64, 0));

    // 波形描画: 画面幅240pxに3周期分を表示して視認性を上げる
    constexpr int CYCLES = 3;
    int prevY = CY;
    for (int px = 0; px < GW; px++) {
        // 0〜(WAVE_TABLE_SIZE*CYCLES)の範囲で波形テーブルを参照
        int idx = (int)((float)px / GW * WAVE_TABLE_SIZE * CYCLES) % WAVE_TABLE_SIZE;
        int16_t sample = getMorphedSample(idx, params.timbreMorph);
        // -32768〜32767 → GY〜GY+GH に正規化
        int y = CY - (int)((float)sample / 32768.0f * (GH / 2 - 2));
        y = constrain(y, GY, GY + GH - 1);
        if (px > 0) {
            M5Cardputer.Display.drawLine(px - 1, prevY, px, y, GREEN);
        }
        prevY = y;
    }

    // グラフ枠(タブバーとの境界線)
    M5Cardputer.Display.drawFastHLine(GX, GY + GH, GW, GREEN);
}

// ベンドメーター描画
// 縦棒メーター: 上半分がベンドアップ、下半分がベンドダウン
void drawBendMeter(float bendCents, float maxCents) {
    constexpr int MX  = 75;  // メーター左端X
    constexpr int MY  = 57;  // メーター上端Y
    constexpr int MW  = 12;  // メーター幅
    constexpr int MH  = 50;  // メーター全高
    constexpr int MCY = MY + MH / 2; // 中心Y(ゼロ点)

    M5Cardputer.Display.fillRect(MX, MY, MW, MH, BLACK);
    M5Cardputer.Display.drawRect(MX, MY, MW, MH, GREEN);
    M5Cardputer.Display.drawFastHLine(MX, MCY, MW, GREEN); // ゼロ線

    if (fabsf(bendCents) > 0.5f) {
        float ratio = bendCents / maxCents;
        ratio = constrain(ratio, -1.0f, 1.0f);
        int barH = (int)(fabsf(ratio) * (MH / 2 - 1));
        int barY = (ratio > 0) ? (MCY - barH) : MCY;
        M5Cardputer.Display.fillRect(MX + 1, barY, MW - 2, barH, GREEN);
    }

    // UP/DOWN ラベル
    M5Cardputer.Display.setCursor(MX - 2, MY - 7);
    M5Cardputer.Display.print("UP");
    M5Cardputer.Display.setCursor(MX - 5, MY + MH + 1);
    M5Cardputer.Display.print("DWN");
}

// IMUパッド描画（十字線+ドット）
// パッド上辺にY、右辺にXとだけ表示するシンプルな構成。
// パラメータ情報はdrawPlayScreen()内の右側テキストエリアで表示する。
void drawImuPad() {
    constexpr int PX       = 90;  // パッド左端X
    constexpr int PY       = 57;  // パッド上端Y
    constexpr int PAD_SIZE = 44;  // パッド一辺のサイズ(右側テキストエリアを広く確保)

    M5Cardputer.Display.fillRect(PX - 1, PY - 9, PAD_SIZE + 2 + 14, 9, BLACK); // ラベル行クリア
    M5Cardputer.Display.fillRect(PX, PY, PAD_SIZE, PAD_SIZE, BLACK);
    M5Cardputer.Display.drawRect(PX, PY, PAD_SIZE, PAD_SIZE, GREEN);

    int cx = PX + PAD_SIZE / 2;
    int cy = PY + PAD_SIZE / 2;

    // 十字線（暗い緑）
    M5Cardputer.Display.drawFastHLine(PX, cy, PAD_SIZE, M5Cardputer.Display.color565(0, 64, 0));
    M5Cardputer.Display.drawFastVLine(cx, PY, PAD_SIZE, M5Cardputer.Display.color565(0, 64, 0));

    // IMU傾き位置をドットで表示
    float normX = constrain(lastAccelX / (TILT_MAX_DEGREES / 57.3f), -1.0f, 1.0f);
    float normY = constrain(lastAccelY / (TILT_MAX_DEGREES / 57.3f), -1.0f, 1.0f);
    int dotX = cx + (int)(normX * (PAD_SIZE / 2 - 3));
    int dotY = cy + (int)(normY * (PAD_SIZE / 2 - 3));
    dotX = constrain(dotX, PX + 2, PX + PAD_SIZE - 3);
    dotY = constrain(dotY, PY + 2, PY + PAD_SIZE - 3);
    M5Cardputer.Display.fillCircle(dotX, dotY, 3, GREEN);

    // 軸ラベル: パッド上に「Y」、パッド右に「X」のみ表示
    M5Cardputer.Display.setCursor(cx - 3, PY - 8);
    M5Cardputer.Display.print("Y");
    M5Cardputer.Display.setCursor(PX + PAD_SIZE + 2, cy - 4);
    M5Cardputer.Display.print("X");
}

// IMUパラメータの現在値を0〜1の正規化値で返す（表示用）
float getImuParamNormalized(ImuTarget target) {
    switch (target) {
        case ImuTarget::TIMBRE:        return params.timbreMorph / 3.0f;
        case ImuTarget::VIBRATO_DEPTH: return params.vibratoDepth;
        case ImuTarget::VIBRATO_RATE:  return (params.vibratoRateHz - 1.0f) / 9.0f;
        case ImuTarget::TREMOLO:       return params.tremoloDepth;
        case ImuTarget::VOLUME:        return (params.volumeOffset + 0.5f);
        case ImuTarget::PITCH_BEND:    return (params.pitchBendCents + keyBendMaxCents) / (keyBendMaxCents * 2.0f);
        case ImuTarget::BEND_UP:       return constrain(params.pitchBendCents / keyBendMaxCents, 0.0f, 1.0f);
        case ImuTarget::BEND_DOWN:     return constrain(-params.pitchBendCents / keyBendMaxCents, 0.0f, 1.0f);
        case ImuTarget::BITCRUSH:      return params.bitcrush;
        case ImuTarget::FILTER_CUTOFF: return params.filterCutoffOffset;
        default:                       return 0.0f;
    }
}

// IMUパラメータの現在値を文字列で返す（表示用）
String getImuParamValueStr(ImuTarget target) {
    switch (target) {
        case ImuTarget::TIMBRE: {
            const char *s[] = {"Sine","Tri","Saw","Sq"};
            return String(s[constrain((int)params.timbreMorph, 0, 3)])
                   + "(" + String(params.timbreMorph, 1) + ")";
        }
        case ImuTarget::VIBRATO_DEPTH: return String((int)(params.vibratoDepth*100)) + "%";
        case ImuTarget::VIBRATO_RATE:  return String(params.vibratoRateHz, 1) + "Hz";
        case ImuTarget::TREMOLO:       return String((int)(params.tremoloDepth*100)) + "%";
        case ImuTarget::VOLUME:        return String((int)(params.volumeOffset*100),10) + "%";
        case ImuTarget::PITCH_BEND:    return String((int)params.pitchBendCents) + "c";
        case ImuTarget::BEND_UP:       return "+" + String((int)params.pitchBendCents) + "c";
        case ImuTarget::BEND_DOWN:     return String((int)params.pitchBendCents) + "c";
        case ImuTarget::BITCRUSH:      return String((int)(params.bitcrush*100)) + "%";
        case ImuTarget::FILTER_CUTOFF: {
            float c = filterParams.cutoffHz*(1.0f-params.filterCutoffOffset*0.9f);
            return String((int)constrain(c, FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX)) + "Hz";
        }
        case ImuTarget::NONE: return "---";
        default:              return "---";
    }
}

// PLAY画面全体の描画
void drawPlayScreen(bool fullRedraw) {
    if (fullRedraw) {
        M5Cardputer.Display.fillScreen(BLACK);
        // 区切り線（固定）
        M5Cardputer.Display.drawFastHLine(0, 113, 240, GREEN);
        // 操作ナビ（固定、2行）
        M5Cardputer.Display.setCursor(3, 115);
        M5Cardputer.Display.print("1-0:NOTE Z/X:BEND =/-.OCT ,/.VOL");
        M5Cardputer.Display.setCursor(3, 125);
        M5Cardputer.Display.print("Tab:MENU");
    }

    // タブバー
    drawTabBar(AppMode::PLAY);

    // 波形グラフ
    drawWaveform();

    // ================================================================
    // 情報エリア (Y:56〜112)
    // 左ブロック  (X:0〜72):  音名・周波数
    // 中ブロック  (X:73〜90): ベンドメーター
    // 右ブロック  (X:91〜239): IMUパッド(X:90〜135) + パラメータ情報(X:140〜239)
    // ================================================================

    // --- 左: 音程ブロック ---
    M5Cardputer.Display.fillRect(0, 56, 73, 57, BLACK);
    M5Cardputer.Display.drawRect(0, 56, 73, 57, GREEN);

    // 音名（フォントサイズ2で大きく）
    const char *noteName = getNoteName(playingFreq > 0.0f ? playingFreq : currentFreq);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(4, 60);
    M5Cardputer.Display.printf("%-4s", noteName);
    M5Cardputer.Display.setTextSize(1);

    // 周波数
    M5Cardputer.Display.setCursor(4, 84);
    if (playingFreq > 0.0f || currentFreq > 0.0f) {
        float dispFreq = playingFreq > 0.0f ? playingFreq : currentFreq;
        M5Cardputer.Display.printf("%-9s", (String(dispFreq, 1) + "Hz").c_str());
    } else {
        M5Cardputer.Display.print("---      ");
    }

    // OCT（Volは右下エリアに統合したのでここでは表示しない）
    M5Cardputer.Display.setCursor(4, 95);
    M5Cardputer.Display.printf("OCT:%+d  ", params.octaveShift);

    // --- 中: ベンドメーター ---
    float totalBend = params.pitchBendCents + keyBendCurrent;
    drawBendMeter(totalBend, keyBendMaxCents);

    // --- 右: IMUパッド ---
    drawImuPad();

    // --- 右: パラメータ情報テキストエリア (X=140〜239, Y=56〜112) ---
    constexpr int TX = 140;
    constexpr int TW = 100; // テキストエリア幅

    // エリア全体をクリア
    M5Cardputer.Display.fillRect(TX, 56, TW, 57, BLACK);

    // X軸: 割り当て名
    M5Cardputer.Display.setCursor(TX, 57);
    M5Cardputer.Display.printf("X:%-10s", imuTargetName(imuAxisX.target));

    // X軸: 現在値（文字列）
    M5Cardputer.Display.setCursor(TX, 66);
    String xValStr = getImuParamValueStr(imuAxisX.target);
    M5Cardputer.Display.printf(" %-11s", xValStr.c_str());

    // X軸: 現在値（ミニバー）
    if (imuAxisX.target != ImuTarget::NONE) {
        float xNorm = constrain(getImuParamNormalized(imuAxisX.target), 0.0f, 1.0f);
        int barW = (int)(xNorm * (TW - 4));
        M5Cardputer.Display.fillRect(TX, 75, TW - 2, 4, M5Cardputer.Display.color565(0, 64, 0));
        if (barW > 0) M5Cardputer.Display.fillRect(TX, 75, barW, 4, GREEN);
    }

    // Y軸: 割り当て名
    M5Cardputer.Display.setCursor(TX, 81);
    M5Cardputer.Display.printf("Y:%-10s", imuTargetName(imuAxisY.target));

    // Y軸: 現在値（文字列）
    M5Cardputer.Display.setCursor(TX, 90);
    String yValStr = getImuParamValueStr(imuAxisY.target);
    M5Cardputer.Display.printf(" %-11s", yValStr.c_str());

    // Y軸: 現在値（ミニバー）
    if (imuAxisY.target != ImuTarget::NONE) {
        float yNorm = constrain(getImuParamNormalized(imuAxisY.target), 0.0f, 1.0f);
        int barW = (int)(yNorm * (TW - 4));
        M5Cardputer.Display.fillRect(TX, 99, TW - 2, 4, M5Cardputer.Display.color565(0, 64, 0));
        if (barW > 0) M5Cardputer.Display.fillRect(TX, 99, barW, 4, GREEN);
    }

    // Vol / Bend幅
    M5Cardputer.Display.setCursor(TX, 105);
    M5Cardputer.Display.printf("VOL:%d%% BND:%dst  ",
        (int)(params.keyVolume * 100),
        (int)(keyBendMaxCents / 100.0f));
}

// EDITとSETTINGSにもタブバーを付加するよう修正
void drawEditScreen() {
    M5Cardputer.Display.fillScreen(BLACK);
    drawTabBar(AppMode::EDIT);

    drawAdsrGraph();

    // グラフの下に項目リストを2カラムで表示
    constexpr int LIST_Y = 76;
    constexpr int ROW_H  = 12;
    constexpr int COL2_X = 125;

    for (int i = 0; i < NUM_EDIT_ITEMS; i++) {
        bool isLeftColumn = (i < 4);
        int row = isLeftColumn ? i : (i - 4);
        int x = isLeftColumn ? 5 : COL2_X;
        int y = LIST_Y + row * ROW_H;
        M5Cardputer.Display.setCursor(x, y);
        if (i == selectedEditIndex) {
            M5Cardputer.Display.printf(">%-9s%s", editItems[i].name, editItems[i].valueLabel());
        } else {
            M5Cardputer.Display.printf(" %-9s%s", editItems[i].name, editItems[i].valueLabel());
        }
    }
    M5Cardputer.Display.setCursor(5, 126);
    M5Cardputer.Display.print(";/. select ,// chg  Tab:next");
}

void drawSettingsScreen() {
    M5Cardputer.Display.fillScreen(BLACK);
    drawTabBar(AppMode::SETTINGS);

    for (int i = 0; i < NUM_SETTING_ITEMS; i++) {
        int y = 18 + i * 15;
        M5Cardputer.Display.setCursor(5, y);
        if (i == selectedSettingIndex) {
            M5Cardputer.Display.printf("> %-14s %s", settingItems[i].name, settingItems[i].valueLabel());
        } else {
            M5Cardputer.Display.printf("  %-14s %s", settingItems[i].name, settingItems[i].valueLabel());
        }
    }
    M5Cardputer.Display.setCursor(5, 115);
    M5Cardputer.Display.println(";/. select  ,// change");
    M5Cardputer.Display.setCursor(5, 128);
    M5Cardputer.Display.println("Tab: save & back to play");
}

// ---------------------------------------------------------
// loop
// ---------------------------------------------------------
unsigned long lastDisplayUpdateMs = 0;
// 起動時に必ずmodeChanged=trueになるよう、PLAY以外の値で初期化する
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

    // --- PLAY モードの描画 ---
    unsigned long now = millis();
    // PLAY画面はIMUや波形がリアルタイムに変化するため50msごとに更新
    if (keyChanged || modeChanged || (now - lastDisplayUpdateMs) >= 50) {
        lastDisplayUpdateMs = now;
        drawPlayScreen(modeChanged);
    }

    delay(5);
}
