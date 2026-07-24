/*
 * C.P.S. (CardPuter Synth) - v0.971
 * -------------------------------------------------------
 * A DIY synthesizer app for the M5Stack Cardputer family.
 * Runs on both CardputerADV (full feature set, incl. IMU) and the
 * original Cardputer (auto-detected at boot — see "Original Cardputer"
 * section below for what's different).
 *
 * Features:
 *   - Both EZ and Pro Mode use the same two physical key rows, each
 *     spanning as many octaves as needed to fit 13 notes of the active
 *     scale with no gaps or overlaps (computed automatically per scale):
 *     "1234567890-=" + Backspace (row 1) and "qwertyuiop[]\" (row 2,
 *     some octaves below row 1). Switch modes in SETTING > Play Mode.
 *   - EZ Mode (default): always the Major scale — simple and
 *     predictable, but with a wide 2-row range so octave-spanning
 *     melodies don't require manually shifting octaves
 *   - Pro Mode: choose any scale, including "Chromatic" (the default,
 *     giving full black-key access — see Scale below)
 *   - Monophonic: last key pressed wins
 *   - Notes can be played on every screen except the Patch Bank (VCO/
 *     VCF/VCA/LFO/SETTINGS/CATEGORY all only use ;/./,// for their own
 *     navigation, so playing while editing lets you hear tone/filter/
 *     LFO changes live)
 *   - ';' / '.' keys: octave shift (-2 to +2) [CardputerADV; see below
 *     for original Cardputer]
 *   - ',' / '/' keys: transpose (-12 to +12 semitones), independent
 *     of octave shift [CardputerADV; see below for original Cardputer]
 *   - 'k' / 'l' keys: volume control (0-100%, 5% steps)
 *   - 'Z' key: bend down  /  'X' key: bend up
 *     Guitar-chording style: slow pitch rise on press,
 *     fast return on release (asymmetric attack/release).
 *   - 'C' key: portamento ON/OFF toggle (PLAY mode)
 *   - 'A' key: IMU/PAD X axis hold toggle
 *   - 'S' key: IMU/PAD Y axis hold toggle
 *   - 'D' key: note hold toggle
 *   - 'H' key: hold to show HELP overlay on MAIN screen
 *   - MAIN screen shows "(HOLD)" next to an IMU/PAD axis readout when
 *     that axis is held
 *   - ADSR envelope with retrigger support
 *   - Biquad filter: LPF/HPF/BPF/Notch/None, with its own filter envelope
 *   - Portamento: glide speed & on/off (SETTING > Portamento sub-screen)
 *   - SETTING screen is a category launcher: Patch / IMU(PAD) / Bend /
 *     Portamento / Play Mode each open their own dedicated sub-screen
 *     (Tab goes back one level, same pattern as the Patch Bank screen)
 *   - Sub oscillator, noise blend, bit-crusher
 *   - General-purpose LFO (Sine/Triangle/Sawtooth/Square) that can
 *     modulate Pitch / Volume / Timbre / Filter cutoff / PWM.
 *     Fully independent from the existing Vibrato and Tremolo LFOs.
 *   - IMU (BMI270) tilt-to-parameter mapping across 17 targets (customizable,
 *     hold state and frozen value persist across save/load), selected via
 *     a scrollable picker grouped by category (Pitch/Volume/Timbre/Filter/LFO/Effect)
 *   - Per-axis IMU fine control: Sensitivity, Invert, response Curve
 *     (Linear/Exponential), Deadzone, and a Calibrate ON/OFF toggle
 *     (ON opens a confirm dialog and re-zeros to current tilt, OFF resets)
 *     [CardputerADV only — see below for original Cardputer]
 *   - IMU Volume target is now a relative multiplier of the current
 *     volume (0-100%), so it can only attenuate, never exceed the set level
 *   - Patch Bank: save/load full synth state (incl. IMU mapping/hold
 *     state and portamento) as named patches under /CPS/Patch.
 *     Rename, duplicate and delete are available from the Patch Bank
 *     screen (SETTING menu). The app never navigates outside this
 *     dedicated folder.
 *   - Pattern Bank (Phase 4): save/load Sequencer patterns (16 steps +
 *     Tempo/Swing) to/from a grid of 8 lettered banks (A-H) x 8 numbered
 *     slots (1-8), stored under /CPS/Pattern as one file per slot
 *     (e.g. /CPS/Pattern/A1.json). Accessed via SETTING > Pattern >
 *     Save/Load, a grid browser (own AppMode::PATTERN, own Tab handling
 *     like Patch Bank) — navigate with ;/./,//,  confirm with Enter,
 *     Backspace clears the selected slot (with a Y/N confirm, same for
 *     overwriting an occupied slot on Save). Reuses the exact same
 *     seq_tempo/seq_swing/seqN_* field names the main settings file
 *     already uses for the Sequencer, via a shared writeSeqPatternFields()
 *     helper (save) and the existing parseSettingLine() (load) — a
 *     pattern file is really just a tiny settings file scoped to only
 *     those fields. SETTING > Pattern > Random generates a fresh 16-step
 *     pattern in-key with the current scale (picked from the same
 *     row1Freqs/row2Freqs tables note entry uses), including Tie/Accent/
 *     Slide, not just pitch/velocity — behind the same confirm dialog as
 *     Randomize Patch. Tempo/Swing are left untouched (pattern-level, not
 *     part of "randomize the steps"). Randomize Patch's own Noise amount
 *     is deliberately rare (15% chance) and subtle (5-20%) when it does
 *     apply — full 0-100% randomization made pitch clarity noticeably
 *     worse even at seemingly-low rolls. SETTING > Pattern is hidden
 *     while PLAY is the active home mode (only meaningful from SEQ),
 *     the mirror image of Arp being hidden from SEQ — needed splitting
 *     original Cardputer's settings list into PLAY/SEQ variants too,
 *     which it didn't have before (Pattern showed regardless of home
 *     mode there).
 *   - Song mode (Phase 4 continued, v0.95-v0.953): arranges saved
 *     Pattern Bank patterns into a sequence and plays them back. Its own
 *     AppMode::SONG, entered via a LONG press (500ms) of G0 — short
 *     press keeps the existing PLAY<->SEQ toggle unchanged, now resolved
 *     on release instead of press-down so the two can be told apart.
 *     Has its own fixed UI color (cyan) rather than inheriting PLAY's
 *     green or SEQ's orange, since it's a distinct third mode — `uiColor`
 *     is computed once per loop() iteration, AFTER all of that frame's
 *     mode-changing input (G0, Tab-cycle, and each mode's own Tab
 *     handling) has already been processed, not at the very top of
 *     loop() — computing it too early meant a mode change and its color
 *     update landed on different frames, causing a stale/mixed-color
 *     flash right at the moment of switching (fixed in v0.954). Each song
 *     entry references a Pattern Bank slot (bank+slot) plus its own
 *     Transpose (semitones, chromatic — consistent with how Octave/
 *     Transpose already works elsewhere, not a scale-aware diatonic
 *     shift) and Repeat count (times through before advancing) — a new
 *     entry inherits the Bank/Slot of whichever entry the cursor was on
 *     (handy for building similar-pattern sequences), but Transpose/
 *     Repeat reset to defaults rather than also carrying over. Two
 *     global toggles: whether each entry uses its own saved pattern's
 *     Tempo/Swing ('I' key, on by default — patterns already save these,
 *     so this comes for free) or a dedicated Song-level Tempo/Swing
 *     instead throughout, and whether the song loops back to the start
 *     after the last entry or stops ('O' key, loop by default). Editor
 *     has two focuses (like SEQ's STEP/PATTERN split): 'f' toggles
 *     between Entry focus (,// moves the entry cursor — matching the
 *     horizontal timeline's own layout, swapped from SEQ's convention in
 *     v0.961 per user feedback — 'g' cycles which field — Bank/Slot/
 *     Transpose/Repeat — ;/. adjusts it) and Song
 *     focus ('g' cycles Tempo/Swing instead, ;/. adjusts it) — the
 *     Song-level Tempo/Swing and Volume are always shown regardless of
 *     focus, matching how SEQ always shows every value rather than
 *     hiding the non-selected ones. Volume (k/l) and other non-
 *     conflicting performance keys work here too, same "usable anywhere
 *     except Patch" principle as elsewhere — Shift+S/Shift+L are
 *     reserved for Save/Load specifically in SONG, so the plain
 *     IMU-Y-hold/Volume-up meanings of 's'/'l' are skipped there, same
 *     pattern as SEQ's Shift+C/Shift+X reservations. Enter inserts a new
 *     entry after the cursor, Backspace deletes the selected entry,
 *     Space plays/stops the song, Shift+S/Shift+L open a Save/Load slot
 *     picker (8 numbered slots, no lettered banks — fewer songs expected
 *     than patterns) under /CPS/Song, Tab returns to whichever of
 *     PLAY/SEQ was home.
 *     Visual design (v0.96): rather than a plain text list, entries show
 *     as a horizontal timeline of fixed-width blocks colored by Bank
 *     letter (A-H each get a distinct fixed color, songBankColors[]) —
 *     the playing entry's block turns white, the edit cursor's block
 *     gets a white outline instead (so both can show distinctly even on
 *     the same block); a thin bar under each block is proportional to
 *     that entry's Repeat count. Below the timeline, a small step-grid
 *     preview (mirroring SEQ's own grid look, but simplified — filled=
 *     note, half-height=Tie, no velocity/accent detail) shows the actual
 *     shape of whichever pattern is currently playing, or the cursor's
 *     entry when stopped — read via a new loadPatternPreview() into a
 *     dedicated songPreviewSteps[] buffer (cached by bank+slot) so
 *     browsing entries in the editor never disturbs the live seqSteps[]
 *     that may actually be playing.
 *     Playback engine reuses SEQ's own step-timing engine (
 *     updateSeqTiming(), seqPlaying) entirely: loading an entry
 *     (songLoadEntry()) pulls that pattern in via the existing Pattern
 *     Bank load function, applies the entry's Transpose as a multiplier
 *     on newly-triggered pitches, and completing a full 16-step pass
 *     (songAdvanceOnPassComplete(), called from updateSeqTiming()) counts
 *     against the entry's Repeat count before advancing (or looping/
 *     stopping at the end of the song). Deliberately simple/no pre-fetch
 *     — each entry transition does a synchronous SD card read; verified
 *     on hardware with no audible hiccup at pattern boundaries so far.
 *     SEQ also has step Copy/Cut/Paste: 'V' marks/confirms/clears a
 *     step-range selection (shown as a yellow strip above the selected
 *     steps), extended live by ,// while marking; Shift+C copies the
 *     selection, Shift+X cuts it (clears the source to rests), Enter
 *     pastes the clipboard starting at the cursor (truncated if it would
 *     run past step 16). Reusing 'V'/Shift+C/Shift+X this way meant
 *     freeing them from their usual meanings specifically within SEQ:
 *     plain 'V' no longer toggles Arp Latch there (Shift+V still toggles
 *     Arp on/off everywhere, unchanged), and Shift+C/Shift+X no longer
 *     also fire Portamento-toggle/Bend-up there — chosen because Arp
 *     Latch has no audible effect during SEQ anyway (Arp is suppressed
 *     while a pattern plays), same reasoning as hiding SETTING > Arp
 *     from SEQ.
 *   - Reset to default: Patch category resets VCO/VCF/VCA/LFO/IMU to a
 *     simple starting patch (behind a confirm dialog); Bend and
 *     Portamento categories each have their own separate reset
 *   - Randomize: Patch category can also randomize every tone parameter
 *     (incl. filter type, LFO wave/target, IMU targets) behind a confirm
 *     dialog — a quick way to discover new sounds
 *   - Play Mode (SETTING > Play Mode): EZ Mode (default) vs Pro Mode,
 *     see above
 *   - Scale (SETTING > Play Mode > Scale, Pro Mode only — EZ Mode is
 *     always Major): choose from 49 scales across 9 categories
 *     (Chromatic, Classical, Symmetrical, Pentatonic, Japan, China,
 *     India, Middle East, Europe) via a 2-level picker (v0.954 added 16
 *     more — Harmonic/Neapolitan Major, Lydian Augmented/Dominant,
 *     2 Messiaen modes, 2 more pentatonics, Ahir Bhairav/Marva/Purvi/
 *     Charukeshi, Nikriz/Persian, Romanian Minor/Hungarian Major — all
 *     appended after the existing entries rather than inserted, so any
 *     already-saved currentScaleIndex stays pointing at the same scale).
 *     New scales are always appended, never inserted, for this reason.
 *     The per-category index buffer used by the picker was bumped from
 *     16 to 32 slots to leave headroom for further additions — Classical
 *     is the largest category so far at 13. Selecting a
 *     scale takes effect immediately, so holding a note key while
 *     scrolling previews it live. Current scale (with category) is
 *     shown on the MAIN screen.
 *   - Arpeggiator (SETTING > Arp, CardputerADV only): hold up to 6 notes
 *     at once as a chord; Up / Down / Up-Down / As Played / Random
 *     patterns, adjustable Tempo (40-240 BPM), Rate (note length per
 *     step, 1/1 to 1/32 incl. two triplets), and Swing (-100% to +100%:
 *     positive delays the off-beat step for a classic swung feel,
 *     negative pushes it earlier for a pushed/anticipated feel). Tempo
 *     and Swing can also be assigned as IMU targets — both are always
 *     bipolar (+/- the base value) regardless of the axis's own
 *     Invert/bipolar setting, same as Pitch Bend. Each step
 *     force-retriggers the envelope for a percussive, stepped feel.
 *     'V' key toggles Latch mode: each note-key press toggles that
 *     note's membership in the chord, instead of needing continuous
 *     physical holds (easier on a small keyboard). Shift+'V' toggles
 *     the Arpeggiator on/off, usable on any screen except the Patch
 *     Bank (the redundant SETTING > Arp on/off toggle was removed once
 *     this covered every screen). PLAY screen lists
 *     every held note (press order) with the currently-sounding one
 *     highlighted, in place of the normal single note-name display.
 *     Since notes now work on every screen (see above), this preview
 *     works everywhere too — including while adjusting Arp's own
 *     settings, without needing to back out to PLAY first.
 *   - Step Sequencer: a 16-step, TB-303-style pattern. Each step has a
 *     Note (or Rest), Velocity, and three performance flags: Tie
 *     (extends the previous note instead of retriggering — chaining
 *     consecutive Tie steps is how a note's length varies, instead of a
 *     Gate percentage), Slide (glides from the previous pitch to this
 *     one instead of retriggering, via its own lightweight portamento-
 *     style glide independent of the global Portamento toggle), and
 *     Accent (boosts this step's Velocity and gives the filter cutoff a
 *     temporary boost, for the classic TB-303 "punch"). Has its own
 *     independent Tempo and Swing (separate from the Arpeggiator's —
 *     PLAY and SEQ are treated as distinct performance modes with their
 *     own timing), though assigning the Arpeggiator's Tempo/Swing IMU
 *     targets controls the Sequencer's instead whenever SEQ is the
 *     active home mode — one target per axis works contextually for
 *     both. Works on both boards (unlike the Arpeggiator) since
 *     entering one note per step doesn't need multi-key rollover.
 *     Accessed via its own SEQ mode (see G0 button below) rather than
 *     the Tab cycle, since it's conceptually a second "home" screen
 *     alongside PLAY, not another editor tab. The Arp SETTING entry is
 *     hidden while SEQ is the active home mode, since the Arpeggiator
 *     needs live chord-holding that SEQ playback suppresses.
 *     Orange accent color applies to the ENTIRE UI (not just the SEQ
 *     screen itself) whenever SEQ is the active home mode — even while
 *     viewing VCO/VCF/etc — so it's always visible at a glance which
 *     mode you're in; a first step toward user-customizable UI colors,
 *     planned for later.
 *     The SEQ screen otherwise mirrors PLAY's layout exactly (no
 *     waveform, but the same IMU/PAD block, gauge bars, Bend meter, and
 *     Scale name display on the right/bottom, working identically):
 *     step grid where the waveform would be — each step shows Velocity
 *     as a bottom-aligned bar (taller = louder); a run of Tie-connected
 *     steps merges into one shape (thick outer border, no internal
 *     vertical line at the join) so it visibly reads as one sustained
 *     note, while each step's own bar segment still shows (using the
 *     run's starting velocity) so the cursor/playhead can still pick out
 *     individual steps within it; Accent turns the bar red instead of
 *     the usual orange (a shape-based indicator — a triangle top — was
 *     tried first but proved hard to distinguish at 13px step width;
 *     color reads reliably at any size). Slide gets a small diagonal
 *     notch at the bottom-left corner. Velocity carries over from
 *     whatever was last explicitly set to the next note you enter,
 *     instead of resetting to the 100 default each time. Then step/
 *     tempo/swing/octave/transpose/portamento/hold details on the left,
 *     all always visible regardless of what's currently selected to
 *     edit. Full key reference is in the HELP overlay ('H', same as
 *     PLAY) rather than fixed on-screen text.
 *     The Bend meter (used by
 *     both PLAY and SEQ) fits its UP/DWN labels within the same
 *     vertical footprint as the other side blocks now, instead of
 *     spilling into the waveform/step-grid area above and below the box.
 *     Sequencer Play/Stop (Space) works from any screen except Patch,
 *     not just from SEQ itself, so playback can be started/stopped
 *     while tweaking VCO/VCF/etc.
 *     Controls: ','/'/' move the step cursor, note keys assign that
 *     pitch to the selected step (playing a brief preview so you can
 *     hear it, and auto-advancing the cursor to the next step — use
 *     ,// instead of a note key to skip a step and leave it as a rest),
 *     Backspace clears the selected step entirely (note + Tie/Slide/
 *     Accent) back to a plain rest, Shift+Backspace clears the whole
 *     16-step pattern at once, Space starts/stops playback. ';'/'.'
 *     adjusts one of two separate
 *     "focuses" (kept conceptually distinct, not one flat list): STEP
 *     focus cycles through the selected step's Velocity (adjusted
 *     numerically), Tie, Slide, and Accent (each toggled on/off by
 *     either ';' or '.'); PATTERN focus adjusts the whole sequence's
 *     Tempo or Swing. 'f' toggles which focus is active, 'g' cycles
 *     which of the current focus's values ';'/'.' affects. All the
 *     step's values, plus Tempo/Swing, Octave/Transpose, and Portamento/
 *     Hold status (both usable here too, same as PLAY), are always
 *     shown on screen regardless of focus — only a single "Ed:" label
 *     indicates which one is currently adjustable. Shift +
 *     ';'/'.'/','/'/ ' adjusts Octave/Transpose on CardputerADV,
 *     mirroring PLAY's unshifted keys for muscle-memory consistency
 *     (original Cardputer's J/N/B/M don't need Shift, since they don't
 *     collide with SEQ's own keys).
 *     While playing, the Sequencer keeps looping even on other screens
 *     (VCO/VCF/etc, but not SEQ's own editing), so tone/filter changes
 *     can be heard against the pattern — normal note-triggering and the
 *     Arpeggiator are suppressed while it's playing, to avoid fighting
 *     over which note is currently sounding. Volume ('k'/'l', shown on
 *     screen too) now works on every screen except the Patch Bank, same
 *     as note-triggering. Assigning the same "ArpTempo"/"ArpSwing" IMU
 *     targets used by the Arpeggiator instead controls the Sequencer's
 *     own Tempo/Swing whenever SEQ is the active home mode — one target
 *     per axis works contextually for both, no separate assignment
 *     needed per mode. Switching between PLAY and SEQ via G0 silences
 *     whatever was sounding (sequencer playback, an Arp chord, or a
 *     held note via Note Hold), since they're treated as distinct modes.
 *   - G0 button (physically separate from the keyboard, so it can't be
 *     hit by accident while playing/typing): toggles between PLAY and
 *     SEQ from anywhere. Whichever was toggled to last becomes the
 *     "home" position that Tab cycling returns to after SETTINGS.
 *   - Tab key cycles: PLAY/SEQ (whichever is current) -> VCO -> VCF ->
 *     VCA -> LFO -> SETTINGS -> back to PLAY/SEQ. Shift+Tab cycles the
 *     same chain backward (v0.97) — same edge-tracker, just a reversed
 *     switch statement when Shift is held. PATCH/PATTERN/SONG each
 *     handle their own Tab (always "back", no forward/reverse
 *     distinction there) and aren't part of this cycle.
 *   - Auto-save / auto-load via SD card (/CPS/settings.json)
 *   - /CPS folder created automatically on first boot
 *
 * Original Cardputer (no IMU, GPIO-matrix keyboard):
 *   Board type is auto-detected at boot (M5.getBoard()). On original
 *   Cardputer:
 *   - Octave shift moves to 'J' (up) / 'N' (down); Transpose moves to
 *     'M' (up) / 'B' (down) — freeing ';' '.' ',' '/' for PAD control
 *   - IMU is replaced by a key-driven "PAD": ';' / '.' move a virtual
 *     Y axis up/down, ',' / '/' move a virtual X axis left/right.
 *     Moves toward the extreme while held, springs back to center on
 *     release — unless that axis's Hold ('A'/'S', unchanged) is on, in
 *     which case it stays wherever it was instead of springing back.
 *     Everywhere the UI said "IMU" now says "PAD" instead.
 *   - Deadzone and Calibrate are hidden from the PAD sub-menu — neither
 *     concept applies to a clean key-driven signal (no sensor noise to
 *     filter, no physical zero-point to correct)
 *   - Arpeggiator is not available: it needs multi-key chord holding,
 *     which the original's 3-key rollover limit can't reliably support
 *   - IMPORTANT: the original Cardputer's GPIO-matrix keyboard only
 *     reliably supports 3 simultaneous key presses. Pressing a 4th key
 *     at the same time can cause "ghosting" (incorrect/missing key
 *     detection) — this is a hardware limitation of the original
 *     Cardputer itself and cannot be fully corrected in software, since
 *     the ambiguity already exists by the time a key press reaches this
 *     app. Keep this in mind with combinations like note + PAD + Bend.
 *
 * Display rendering: every screen draws into a single off-screen canvas
 * (M5Canvas, 240x135, in PSRAM) and pushes the finished frame to the
 * display in one single transfer, instead of many small direct draws —
 * this eliminates a diagonal tearing/flicker artifact that was visible
 * whenever several UI elements updated in the same frame. Shared drawing
 * helpers (drawTabBar, drawWaveform, drawBendMeter, drawImuPad,
 * drawHelpOverlay) take a LovyanGFX& target parameter, a holdover from
 * when PLAY/SEQ were converted first and other screens still drew
 * directly to the display; now that everything uses the canvas, they
 * could be simplified to call it directly, but the parameter is
 * harmless to keep.
 *
 * PLAY screen specifically also uses a dirty-rect split into four
 * independently pushed canvases: canvasTop (y=0-54: tab bar + waveform,
 * skipped when the waveform hasn't meaningfully changed), canvasName
 * (x=0-73, y=55-112: note info, always pushed since that's what changes
 * on every note keypress), canvasImu (x=73-240, y=55-112: bend meter,
 * IMU pad+readout, volume — skipped when none of those values actually
 * changed, which is the common "IMU=None, just playing notes" case),
 * and canvasNav (y=113-134: scale name + nav text, rarely changes).
 * This was needed because even DMA-based SPI transfers still consume
 * shared memory-bus bandwidth long enough to occasionally overlap
 * audioTask's real-time budget on Core 0, audible as an intermittent
 * crackle correlated with key presses; reducing push FREQUENCY alone (a
 * redraw-rate throttle) didn't fully resolve it, since each full push
 * still took its fixed transfer time whenever it did happen, and even
 * the first (2-way) split didn't help much since canvasBottom back then
 * still bundled note info together with IMU/bend/volume, so it still
 * pushed on every note regardless of whether IMU/bend/volume changed.
 * drawBendMeter/drawImuPad take optional yOff/xOff parameters (default
 * 0, unused by SEQ/HELP which still use the single full canvas) so the
 * same functions work against canvasImu's offset coordinate space too
 * (PLAY, yOff=-55, xOff=-73).
 *
 * Required library: M5Cardputer (uses M5Unified / M5GFX internally)
 */

#include "M5Cardputer.h"
#include <math.h>
#include <SPI.h>
#include <SD.h>

// Off-screen canvas (sprite buffer): every screen draws into this fully
// in memory, then pushes the finished frame to the display in one single
// SPI transfer via pushSprite(). This eliminates the tearing/flicker
// that came from drawing many small regions directly to the display one
// at a time (visible as a diagonal "wipe" while the panel's own scan-out
// caught mid-update content). Sized to the full physical display
// (240x135) and created in setup().
M5Canvas canvas(&M5Cardputer.Display);
// PLAY-only dirty-rect split (v0.937, further split in v0.9372 — see
// canvasName/canvasImu/canvasNav below): PLAY is redrawn far more often
// than any other screen (every note key press), so its 63KB full-canvas
// push was the main contributor to an audible crackle correlated with
// key presses — SPI-DMA bus time for a ~65KB transfer occasionally
// overlapped audioTask's real-time budget on Core 0. canvasTop (tab bar
// + waveform, y=0-54, no coordinate offset needed — already 0-based) is
// pushed independently from the rest, and skipped entirely when nothing
// is modulating Timbre/PWM. Other screens (SEQ/VCO/etc) and the HELP
// overlay still use the single full-size `canvas` above, unchanged.
M5Canvas canvasTop(&M5Cardputer.Display);
// v0.9372: the original single "everything below the waveform" canvas
// still transferred on EVERY note keypress even with IMU=None, since it
// bundled note info (changes every note) together with IMU/bend/volume
// (often static). Split further into canvasName (note info only, ~8KB,
// still pushed every note) and canvasImu (bend+IMU pad+readout+volume,
// ~27KB, only pushed when THAT content actually changes) so playing
// notes with IMU=None only transfers canvasName. canvasNav (scale name +
// nav text, rarely changes) split out too so it doesn't need to ride
// along on every canvasName/canvasImu push.
M5Canvas canvasName(&M5Cardputer.Display);
M5Canvas canvasImu(&M5Cardputer.Display);
M5Canvas canvasNav(&M5Cardputer.Display);
constexpr int BOTTOM_Y_OFFSET=55; // shared by canvasName/canvasImu (both start at absolute y=55)
constexpr int IMU_X_OFFSET=73;    // canvasImu starts at absolute x=73
constexpr int NAV_Y_OFFSET=113;   // canvasNav starts at absolute y=113

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
// Note-key layout — shared physical layout for EZ and Pro Mode
// ---------------------------------------------------------
// Both modes use the same two physical rows of 13 keys each:
//   Row 1 (number row): "1234567890-=" + Backspace (13th key — not a
//   printable character, so it's detected via KeysState.del instead of
//   .word; see resolveFreqFromKeys())
//   Row 2 (qwerty row, some octaves below Row 1): "qwertyuiop[]\"
// EZ Mode always uses the Major scale (see MAJOR_SCALE below); Pro Mode
// can pick any scale from SCALES[], including "Chromatic" (the default,
// reproducing the original Pro Mode note layout exactly).
constexpr char ROW1_KEYS[12] = {'1','2','3','4','5','6','7','8','9','0','-','='};
constexpr char ROW2_KEYS[13] = {'q','w','e','r','t','y','u','i','o','p','[',']','\\'};
float row1Freqs[13]; // index 12 = the Backspace/del key
float row2Freqs[13];

// ---------------------------------------------------------
// Scales
// ---------------------------------------------------------
// Each scale is a set of semitone offsets from the root (fixed at C, since
// Transpose already covers changing key). The 13 keys of a row are mapped
// onto the scale degrees in order, wrapping into the next octave once the
// scale's own note count is exceeded — e.g. a 5-note pentatonic scale
// spans keys 1-5 (octave 1), 6-10 (octave 2), 11-13 (start of octave 3).
// Row 2 uses the same scale, shifted down by just enough octaves that its
// own 13th key lands at or below Row 1's root (see computeRow2OctaveShift).
struct ScaleDef { const char *name; uint8_t category; int8_t intervals[12]; uint8_t length; };
const ScaleDef MAJOR_SCALE = {"Major",0,{0,2,4,5,7,9,11},7}; // EZ Mode is always this

const char *SCALE_CATEGORY_NAMES[] = {
    "Chromatic","Classical","Symmetrical","Pentatonic","Japan","China","India","Middle East","Europe"
};
constexpr int NUM_SCALE_CATEGORIES = sizeof(SCALE_CATEGORY_NAMES)/sizeof(SCALE_CATEGORY_NAMES[0]);

const ScaleDef SCALES[]={
    // Chromatic — Pro Mode's default (reproduces the original layout)
    {"Chromatic",     0,{0,1,2,3,4,5,6,7,8,9,10,11},12},
    // Classical / church modes
    {"Major",         1,{0,2,4,5,7,9,11},   7},
    {"Natural Minor", 1,{0,2,3,5,7,8,10},   7},
    {"Dorian",        1,{0,2,3,5,7,9,10},   7},
    {"Phrygian",      1,{0,1,3,5,7,8,10},   7},
    {"Lydian",        1,{0,2,4,6,7,9,11},   7},
    {"Mixolydian",    1,{0,2,4,5,7,9,10},   7},
    {"Locrian",       1,{0,1,3,5,6,8,10},   7},
    {"Harmonic Minor",1,{0,2,3,5,7,8,11},   7},
    {"Melodic Minor", 1,{0,2,3,5,7,9,11},   7},
    // Symmetrical / artificial scales
    {"Whole Tone",        2,{0,2,4,6,8,10},      6},
    {"Diminished (W-H)",  2,{0,2,3,5,6,8,9,11},  8},
    {"Diminished (H-W)",  2,{0,1,3,4,6,7,9,10},  8},
    {"Augmented",         2,{0,3,4,7,8,11},      6},
    // Pentatonic
    {"Major Pentatonic",  3,{0,2,4,7,9},   5},
    {"Minor Pentatonic",  3,{0,3,5,7,10},  5},
    {"Blues",             3,{0,3,5,6,7,10},6},
    {"Egyptian (Sus)",    3,{0,2,5,7,10},  5},
    // Japan
    {"Hirajoshi",        4,{0,2,3,7,8},  5},
    {"In Sen",           4,{0,1,5,7,10}, 5},
    {"Iwato",            4,{0,1,5,6,10}, 5},
    {"Kumoi",            4,{0,2,3,7,9},  5},
    {"Yo Scale",         4,{0,2,5,7,9},  5},
    {"Ryukyu (Okinawa)", 4,{0,4,5,7,11}, 5},
    // China — the traditional pentatonic (Gong/Shang/Jue/Zhi/Yu) is
    // intervallically the same 5-note set as Western pentatonic, just
    // started from a different degree (like a mode). Gong mode = Major
    // Pentatonic and Yu mode = Minor Pentatonic above, so only the three
    // remaining, genuinely distinct modes are listed here.
    {"Shang Mode", 5,{0,2,5,7,10}, 5},
    {"Jue Mode",   5,{0,3,5,8,10}, 5},
    {"Zhi Mode",   5,{0,2,5,7,9},  5},
    // India
    {"Bhairav", 6,{0,1,4,5,7,8,11}, 7},
    {"Todi",    6,{0,1,3,6,7,8,11}, 7},
    // Middle East
    {"Hijaz",           7,{0,1,4,5,7,8,10}, 7},
    {"Double Harmonic", 7,{0,1,4,5,7,8,11}, 7},
    // Europe
    {"Hungarian Minor",  8,{0,2,3,6,7,8,11}, 7},
    {"Neapolitan Minor", 8,{0,1,3,5,7,8,11}, 7},
    // Additional scales (appended, not inserted, so existing saved
    // currentScaleIndex values from before this addition stay valid)
    {"Harmonic Major",    1,{0,2,4,5,7,8,11},      7},
    {"Neapolitan Major",  1,{0,1,3,5,7,9,11},      7},
    {"Lydian Augmented",  1,{0,2,4,6,8,9,11},      7},
    {"Lydian Dominant",   1,{0,2,4,6,7,9,10},      7},
    {"Messiaen Mode 3",   2,{0,2,3,4,6,7,8,10,11}, 9},
    {"Messiaen Mode 6",   2,{0,2,4,5,6,8,10,11},   8},
    {"Major b6 Pent.",    3,{0,2,4,7,8},           5},
    {"Pentatonic b5",     3,{0,3,5,6,10},          5},
    {"Ahir Bhairav",      6,{0,1,4,5,7,9,10},      7},
    {"Marva",             6,{0,1,4,6,7,9,11},      7},
    {"Purvi",             6,{0,1,4,6,7,8,11},      7},
    {"Charukeshi",        6,{0,2,4,5,7,8,10},      7},
    {"Nikriz",            7,{0,2,3,6,7,9,10},      7},
    {"Persian",           7,{0,1,4,5,6,8,11},      7},
    {"Romanian Minor",    8,{0,2,3,6,7,9,10},      7},
    {"Hungarian Major",   8,{0,3,4,6,7,9,10},      7},
};
constexpr int NUM_SCALES = sizeof(SCALES)/sizeof(SCALES[0]);
int currentScaleIndex = 0; // index into SCALES[]; default = Chromatic. Only relevant in Pro Mode — EZ always uses MAJOR_SCALE.

enum class PlayMode : uint8_t { EZ, PRO };
PlayMode playMode = PlayMode::EZ;

// How many octaves to shift Row 2 down. Uses floor division so Row 2
// starts as high as possible while still landing a full octave (or more)
// below Row 1 — this favors a wider combined range and allows a bit of
// overlap at the boundary, rather than guaranteeing zero overlap at the
// cost of a gap of unplayable notes in between (which is worse for
// actually playing songs that span more than an octave).
int computeRow2OctaveShift(const ScaleDef &sc){
    int topOffset=sc.intervals[12%sc.length]+(12/sc.length)*12;
    return topOffset/12; // floor division
}

// Recomputes both rows' key frequencies (and the Backspace-key top note)
// from whichever scale is currently active (MAJOR_SCALE for EZ Mode,
// SCALES[currentScaleIndex] for Pro Mode). Cheap (26 powf() calls), so
// safe to call on every scroll step while live-previewing scales, and
// whenever Play Mode is toggled.
void recomputeKeyNotes(){
    const ScaleDef &sc=(playMode==PlayMode::EZ)?MAJOR_SCALE:SCALES[currentScaleIndex];
    for(int i=0;i<13;i++){
        int octave=i/sc.length;
        int degree=i%sc.length;
        float semitones=(float)sc.intervals[degree]+octave*12.f;
        row1Freqs[i]=261.63f*powf(2.f,semitones/12.f);
    }
    int shift=computeRow2OctaveShift(sc);
    for(int i=0;i<13;i++){
        int octave=i/sc.length;
        int degree=i%sc.length;
        float semitones=(float)sc.intervals[degree]+octave*12.f-shift*12.f;
        row2Freqs[i]=261.63f*powf(2.f,semitones/12.f);
    }
}

// Board auto-detection: CardputerADV has an IMU and a TCA8418 keyboard
// controller (10+ key rollover); the original Cardputer has neither (no
// IMU, and only 3-key rollover on its GPIO matrix keyboard). Determined
// once in setup() via M5.getBoard(); everything IMU-related is gated on
// this at runtime, and original-Cardputer builds substitute key-driven
// "PAD" control for the missing IMU (see updatePadVirtualAxes()).
bool isCardputerAdv = true;
uint16_t seqAccentColor = 0xFD20; // placeholder; recomputed properly in setup() via color565(255,140,0)
uint16_t seqAccentNoteColor = 0xF800; // placeholder red; recomputed in setup() — accented steps' velocity bar
uint16_t songAccentColor = 0x07FF; // placeholder cyan; recomputed in setup() — SONG mode's own fixed UI color, distinct from PLAY's green and SEQ's orange
uint16_t uiColor = GREEN; // the "current" UI accent color — GREEN normally, seqAccentColor whenever SEQ is the active home mode (see loop())

// ---------------------------------------------------------
// IMU mapping
// ---------------------------------------------------------
enum class ImuTarget : uint8_t {
    NONE, TIMBRE, VIBRATO_DEPTH, VIBRATO_RATE, TREMOLO,
    VOLUME, PITCH_BEND, BEND_UP, BEND_DOWN, BITCRUSH, FILTER_CUTOFF,
    PWM, DETUNE, NOISE, SUB_LEVEL, RESONANCE, LFO_RATE, LFO_DEPTH,
    ARP_TEMPO, ARP_SWING,
    TARGET_COUNT
};

// Forward declarations
const char *imuTargetName(ImuTarget t);
void resetParamToDefault(ImuTarget t);
void drawWaveform(LovyanGFX &gfx,float morph,float pwm);
void drawAdsrGraph();
void arpToggle();
void drawHelpOverlay(LovyanGFX &gfx);

struct ImuAxisConfig {
    ImuTarget target;
    float sensitivity;
    bool bipolar;
    bool invert;        // v0.8: flips tilt direction
    bool exponential;   // v0.8: response curve, false=linear true=exponential
    float deadzone;     // v0.8: 0.0-0.3 (0-30%), center dead zone
    float calOffsetDeg; // v0.8: calibration zero-point (degrees)
};

ImuAxisConfig imuAxisX = { ImuTarget::TIMBRE,        1.0f, false, false, false, 0.0f, 0.0f };
ImuAxisConfig imuAxisY = { ImuTarget::VIBRATO_DEPTH,  1.0f, false, false, false, 0.0f, 0.0f };

// Raw tilt angle (degrees, before calibration offset) from the most recent
// updateImu() call — used by the Calibrate action to capture a new zero point.
float lastAngleXDeg=0.f, lastAngleYDeg=0.f;

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
    float volumeScale        = 1.0f; // v0.8: relative multiplier (0-1) of keyVolume, was an additive offset
    float pitchBendCents     = 0.0f;
    float bitcrush           = 0.0f;
    float filterCutoffOffset = 0.0f;
    // New in v0.8: offsets for IMU-controlled PWM/Detune/Noise/SubLevel/Resonance.
    // Same pattern as the offsets above: added on top of the VCO/VCF menu's
    // stored value at the point of use, never overwriting the stored value itself.
    float pwmOffset       = 0.0f;
    float detuneOffset    = 0.0f;
    float noiseOffset     = 0.0f;
    float subLevelOffset  = 0.0f;
    float resonanceOffset = 0.0f;
    // IMU targets
    float timbreMorphTarget        = 0.0f;
    float vibratoDepthTarget       = 0.0f;
    float vibratoRateHzTarget      = 5.0f;
    float tremoloDepthTarget       = 0.0f;
    float volumeScaleTarget        = 1.0f;
    float pitchBendCentsTarget     = 0.0f;
    float bitcrushTarget           = 0.0f;
    float filterCutoffOffsetTarget = 0.0f;
    float pwmOffsetTarget       = 0.0f;
    float detuneOffsetTarget    = 0.0f;
    float noiseOffsetTarget     = 0.0f;
    float subLevelOffsetTarget  = 0.0f;
    float resonanceOffsetTarget = 0.0f;
} params;

float currentFreq = 0.0f;
float seqVelocityMult = 1.0f; // per-step velocity multiplier while the Sequencer is playing a note (1.0 = no effect)
// Slide: a separate, lightweight glide independent of the global
// Portamento toggle, so per-step Slide works regardless of whether the
// user has Portamento on or off elsewhere.
bool  seqSliding=false;
float seqSlideFreq=0.f;
constexpr float SEQ_SLIDE_SPEED=0.35f; // per-buffer smoothing coeff — tuned so the glide clearly completes within roughly one step
// Accent: temporary filter cutoff + resonance boost, smoothed like other offsets.
float seqAccentCutoffBoost=0.f, seqAccentCutoffBoostTarget=0.f;
float seqAccentResoBoost=0.f, seqAccentResoBoostTarget=0.f;
constexpr float SEQ_ACCENT_CUTOFF_BOOST=3500.0f; // Hz, added while an accented step is sounding
constexpr float SEQ_ACCENT_RESO_BOOST=4.0f;      // Q, added while an accented step is sounding — the classic TB-303 "quack"
constexpr float SEQ_ACCENT_VELOCITY_MULT=1.3f;   // velocity multiplier for accented steps

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
bool imuCalibrated = false; // true once IMU Calibrate has been confirmed at least once
bool prevImuXHoldPressed = false, prevImuYHoldPressed = false;
bool prevNoteHoldPressed = false;
float heldFreq = 0.0f;
bool prevPortaPressed = false;
bool prevArpLatchPressed = false;
bool prevArpToggleKeyPressed = false;
bool prevSeqPlayKeyPressedGlobal = false;

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
float phase        = 0.0f;
float subPhase     = 0.0f; // independent phase accumulator for the sub oscillator
float vibratoPhase = 0.0f;
float tremoloPhase = 0.0f;

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

float lfoPhase = 0.0f;

// IMU-controlled offsets for the general LFO's own Rate/Depth (v0.8).
// Same additive-offset pattern as the SynthParams offsets above: never
// overwrites the LFO menu's stored rate/depth, just nudges it live.
float lfoRateOffset  = 0.0f, lfoRateOffsetTarget  = 0.0f;
float lfoDepthOffset = 0.0f, lfoDepthOffsetTarget = 0.0f;
float arpTempoOffset  = 0.0f, arpTempoOffsetTarget  = 0.0f; // +/- BPM, applied to arpTempoBpm
float arpSwingOffset= 0.0f, arpSwingOffsetTarget= 0.0f; // +/- %, applied to arpSwing
float seqTempoOffset  = 0.0f, seqTempoOffsetTarget  = 0.0f; // same IMU target (ARP_TEMPO), rerouted here when lastMainMode==SEQ
float seqSwingOffset= 0.0f, seqSwingOffsetTarget= 0.0f;     // same IMU target (ARP_SWING), rerouted here when lastMainMode==SEQ

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
enum class AppMode : uint8_t { PLAY, VCO, VCF, VCA, LFO, SETTINGS, PATCH, CATEGORY, SEQ, PATTERN, SONG };
AppMode appMode = AppMode::PLAY;
AppMode lastMainMode = AppMode::PLAY; // remembers whether PLAY or SEQ is "home" (G0 toggles between them; Tab cycling returns to whichever was last active)
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
            // Only use the held frequency when no key is actively pressed —
            // otherwise a genuinely new keypress should always take priority
            // (previously heldFreq permanently overrode any new note).
            playingFreq=(noteHeld&&heldFreq>0&&currentFreq==0)?heldFreq:currentFreq;
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
// Instantaneous morph/PWM actually used for audio this sample (includes IMU
// offsets and General LFO modulation) — read by MAIN screen's waveform
// preview so what's displayed matches what's actually playing.
float lastModMorph=0.f, lastModPwm=0.5f;

// Diagnostic buffer-timing counters — audioTask (Core 0) only updates these
// (fast, no I/O); loop() (Core 1) does the actual Serial.printf() once a
// second, so USB/Serial I/O timing can never affect audio-critical Core 0.
unsigned long diagWindowStartMs=0, diagCount=0, diagOverCount=0, diagSumUs=0, diagMaxUs=0;
volatile bool diagPrintPending=false;
unsigned long diagPrintCount=0, diagPrintSumUs=0, diagPrintMaxUs=0, diagPrintOverCount=0;

void audioTask(void *pvParameters){
    const int BUF=1024;
    static int16_t bufs[5][1024];
    int bi=0;
    constexpr int CH=0;
    constexpr float SM=0.0008f;

    while(true){
        unsigned long bufStartUs=micros();
        int16_t *buf=bufs[bi];bi=(bi+1)%5;
        bool keyHeld=(currentFreq>0)||noteHeld;
        advanceEnvelope(keyHeld);

        if(envPhase==EnvPhase::IDLE){
            vTaskDelay(5/portTICK_PERIOD_MS);
            phase=0;subPhase=0;continue;
        }

        // Portamento: smoothly move portaFreq toward target
        float targetFreq=(noteHeld&&heldFreq>0&&currentFreq==0)?heldFreq:currentFreq;
        if(portaEnabled&&portaFreq>0&&targetFreq>0){
            portaFreq+=(targetFreq-portaFreq)*portaSpeed;
            if(fabsf(portaFreq-targetFreq)<0.1f)portaFreq=targetFreq;
        } else {
            portaFreq=targetFreq;
        }

        // Sequencer Slide: a separate glide, independent of the Portamento
        // toggle above, so per-step Slide works regardless of it.
        if(seqSliding&&currentFreq>0){
            seqSlideFreq+=(currentFreq-seqSlideFreq)*SEQ_SLIDE_SPEED;
            if(fabsf(seqSlideFreq-currentFreq)<0.1f){seqSlideFreq=currentFreq;seqSliding=false;}
        }

        // v0.8's 7 new IMU-controlled offsets only need to be this smooth,
        // not audio-rate: IMU tilt changes slowly compared to a ~23ms buffer,
        // so smoothing these once per buffer (instead of once per sample,
        // 1024x fewer times) removes a meaningful chunk of per-sample CPU
        // cost with no audible difference — this was found to be a likely
        // contributor to periodic buffer-underrun stutter under heavy load.
        constexpr float SM_BUF=0.5f;
        params.pwmOffset       +=(params.pwmOffsetTarget      -params.pwmOffset)      *SM_BUF;
        params.detuneOffset    +=(params.detuneOffsetTarget   -params.detuneOffset)   *SM_BUF;
        params.noiseOffset     +=(params.noiseOffsetTarget    -params.noiseOffset)    *SM_BUF;
        params.subLevelOffset  +=(params.subLevelOffsetTarget -params.subLevelOffset) *SM_BUF;
        params.resonanceOffset +=(params.resonanceOffsetTarget-params.resonanceOffset)*SM_BUF;
        lfoRateOffset          +=(lfoRateOffsetTarget         -lfoRateOffset)         *SM_BUF;
        lfoDepthOffset         +=(lfoDepthOffsetTarget        -lfoDepthOffset)        *SM_BUF;
        arpTempoOffset         +=(arpTempoOffsetTarget         -arpTempoOffset)        *SM_BUF;
        arpSwingOffset       +=(arpSwingOffsetTarget       -arpSwingOffset)      *SM_BUF;
        seqTempoOffset         +=(seqTempoOffsetTarget         -seqTempoOffset)        *SM_BUF;
        seqSwingOffset       +=(seqSwingOffsetTarget       -seqSwingOffset)      *SM_BUF;
        seqAccentCutoffBoost +=(seqAccentCutoffBoostTarget -seqAccentCutoffBoost)*SM_BUF;
        seqAccentResoBoost   +=(seqAccentResoBoostTarget   -seqAccentResoBoost)  *SM_BUF;

        // Hoisted out of the per-sample loop: these only depend on values that
        // are now buffer-constant (the offsets above, or menu-set values that
        // never change mid-buffer), so recomputing them 1024x/buffer was pure
        // waste. The sub-oscillator's powf() call in particular was expensive
        // (transcendental function) and was being called on every single
        // sample whenever the sub-oscillator was active — this was a major
        // contributor to audioTask missing its real-time budget every buffer.
        float effSubLevel  =constrain(params.subOscLevel+params.subLevelOffset,0.f,1.f);
        float effNoiseLevel=constrain(params.noiseLevel+params.noiseOffset,0.f,1.f);
        float pwmBase      =constrain(params.pwmWidth+params.pwmOffset,0.1f,0.9f);
        float subOctaveRatio=powf(2.f,(float)params.subOscOctave);
        float effLfoRateHz =constrain(lfo.rateHz+lfoRateOffset,LFO_RATE_MIN,LFO_RATE_MAX);
        float effLfoDepth  =constrain(lfo.depth+lfoDepthOffset,0.f,1.f);

        // Dynamic filter cutoff: base + key tracking + filter env + IMU offset + LFO
        {
            float playF=(seqSliding&&seqSlideFreq>0)?seqSlideFreq:((portaEnabled&&portaFreq>0)?portaFreq:playingFreq);
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
                lfoHz = lfoTableSample(lfo.wave,li)*effLfoDepth*LFO_FILTER_MAX_HZ;
            }
            float dynCutoff = constrain(
                (filterParams.cutoffHz + trackHz + envHz + lfoHz + seqAccentCutoffBoost) * imuScale,
                FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX);
            float dynQ = constrain(filterParams.resonanceQ + params.resonanceOffset + seqAccentResoBoost, FILTER_Q_MIN, FILTER_Q_MAX);
            float savedCutoff=filterParams.cutoffHz, savedQ=filterParams.resonanceQ;
            filterParams.cutoffHz=dynCutoff;
            filterParams.resonanceQ=dynQ;
            updateFilterCoefficients();
            filterParams.cutoffHz=savedCutoff;
            filterParams.resonanceQ=savedQ;
        }

        for(int i=0;i<BUF;i++){
            params.timbreMorph        +=(params.timbreMorphTarget       -params.timbreMorph)       *SM;
            params.vibratoDepth       +=(params.vibratoDepthTarget      -params.vibratoDepth)      *SM;
            params.vibratoRateHz      +=(params.vibratoRateHzTarget     -params.vibratoRateHz)     *SM;
            params.tremoloDepth       +=(params.tremoloDepthTarget      -params.tremoloDepth)      *SM;
            params.volumeScale        +=(params.volumeScaleTarget       -params.volumeScale)       *SM;
            params.pitchBendCents     +=(params.pitchBendCentsTarget    -params.pitchBendCents)    *SM;
            params.bitcrush           +=(params.bitcrushTarget          -params.bitcrush)          *SM;
            params.filterCutoffOffset +=(params.filterCutoffOffsetTarget-params.filterCutoffOffset)*SM;

            // Vibrato LFO
            int vi=((int)vibratoPhase)%WAVE_TABLE_SIZE;if(vi<0)vi+=WAVE_TABLE_SIZE;
            float vlfo=sineTable[vi]/32000.f;
            vibratoPhase+=(float)WAVE_TABLE_SIZE*params.vibratoRateHz/SAMPLE_RATE;
            if(vibratoPhase>=WAVE_TABLE_SIZE)vibratoPhase-=WAVE_TABLE_SIZE;

            // Tremolo LFO
            int ti=((int)tremoloPhase)%WAVE_TABLE_SIZE;if(ti<0)ti+=WAVE_TABLE_SIZE;
            float tlfo=(sineTable[ti]/32000.f+1.f)*0.5f;
            tremoloPhase+=(float)WAVE_TABLE_SIZE*5.f/SAMPLE_RATE;
            if(tremoloPhase>=WAVE_TABLE_SIZE)tremoloPhase-=WAVE_TABLE_SIZE;

            // General-purpose LFO (independent from vibrato/tremolo above).
            // Always runs so the LFO tab's live phase marker stays accurate,
            // but only affects audio when routed to a target below.
            // IMU can nudge the LFO's own Rate/Depth without touching the
            // stored LFO menu values (same offset pattern as everything else).
            // effLfoRateHz/effLfoDepth are computed once per buffer above.
            int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
            float lfoRaw=lfoTableSample(lfo.wave,li);
            lfoPhase+=(float)WAVE_TABLE_SIZE*effLfoRateHz/SAMPLE_RATE;
            if(lfoPhase>=WAVE_TABLE_SIZE)lfoPhase-=WAVE_TABLE_SIZE;
            float lfoVal=lfoRaw*effLfoDepth;

            // Key bend smoothing
            float bd=keyBendGoal-keyBendCurrent;
            float bs=(fabsf(keyBendGoal)<fabsf(keyBendCurrent)||keyBendGoal==0)?keyBendReleaseSmooth:keyBendAttackSmooth;
            keyBendCurrent+=bd*bs;

            // Total pitch
            // Vibrato only applies if an IMU axis is actually assigned to it —
            // otherwise a leftover/default depth (e.g. Y defaults to Vibrato
            // Depth) could add unintended pitch modulation.
            bool vibratoActive=(imuAxisX.target==ImuTarget::VIBRATO_DEPTH||imuAxisY.target==ImuTarget::VIBRATO_DEPTH);
            float effVibratoDepth=vibratoActive?params.vibratoDepth:0.f;
            float lfoPitchCents=(lfo.target==LfoTarget::PITCH)?lfoVal*LFO_PITCH_MAX_CENTS:0.f;
            float totalCents=vlfo*effVibratoDepth*VIBRATO_MAX_CENTS
                            +params.pitchBendCents+keyBendCurrent
                            +params.detuneCents+params.fineTuneCents+params.detuneOffset
                            +lfoPitchCents;
            float pr=powf(2.f,totalCents/1200.f);
            float playF=(seqSliding&&seqSlideFreq>0)?seqSlideFreq:((portaEnabled&&portaFreq>0)?portaFreq:playingFreq);
            float phInc=(float)WAVE_TABLE_SIZE*(playF*pr)/SAMPLE_RATE;

            int idx=((int)phase)%WAVE_TABLE_SIZE;if(idx<0)idx+=WAVE_TABLE_SIZE;

            // General LFO -> Timbre / PWM (applied locally, doesn't touch
            // the stored params so the VCO menu values stay untouched)
            float modMorph=params.timbreMorph;
            if(lfo.target==LfoTarget::TIMBRE)
                modMorph=constrain(modMorph+lfoVal*LFO_TIMBRE_MAX,0.f,3.f);
            float modPwm=pwmBase;
            if(lfo.target==LfoTarget::PWM)
                modPwm=constrain(modPwm+lfoVal*LFO_PWM_MAX,0.1f,0.9f);
            lastModMorph=modMorph;lastModPwm=modPwm;

            // Main oscillator
            int16_t sample=getMorphedSample(idx,modMorph,modPwm);

            // Sub oscillator (sine wave, 1 or 2 octaves below).
            // Uses its own independent phase accumulator (subPhase) rather
            // than being derived from the main oscillator's idx — deriving
            // it from idx caused a discontinuous phase reset every time the
            // main oscillator wrapped (i.e. every main cycle), which got
            // audibly worse the lower the octave (more main-cycles pass per
            // sub-cycle), producing periodic clicks/warble that got more
            // prominent the higher the sub level.
            // effSubLevel/subOctaveRatio are computed once per buffer above —
            // the powf() call in particular is too expensive to repeat every
            // sample (44100x/sec).
            if(effSubLevel>0.001f && playF>0){
                float subPhInc=phInc*subOctaveRatio;
                int subIdx=((int)subPhase)%WAVE_TABLE_SIZE;if(subIdx<0)subIdx+=WAVE_TABLE_SIZE;
                int16_t sub=sineTable[subIdx];
                sample=(int16_t)(sample*(1.f-effSubLevel)+sub*effSubLevel);
                subPhase+=subPhInc;
                if(subPhase>=WAVE_TABLE_SIZE)subPhase-=WAVE_TABLE_SIZE;
            }

            // Noise blend (effNoiseLevel computed once per buffer above)
            if(effNoiseLevel>0.001f){
                int16_t noise=nextNoise();
                sample=(int16_t)(sample*(1.f-effNoiseLevel)+noise*effNoiseLevel);
            }

            phase+=phInc;
            if(phase>=WAVE_TABLE_SIZE)phase-=WAVE_TABLE_SIZE;

            sample=applyBitcrush(sample,params.bitcrush);
            sample=applyFilter(sample);

            float vol=constrain(params.keyVolume*params.volumeScale*seqVelocityMult,0.f,1.f);
            bool tremoloActive=(imuAxisX.target==ImuTarget::TREMOLO||imuAxisY.target==ImuTarget::TREMOLO);
            float effTremoloDepth=tremoloActive?params.tremoloDepth:0.f;
            float tg=constrain(1-effTremoloDepth+effTremoloDepth*tlfo*2,0.f,2.f);
            float lfoVolMult=(lfo.target==LfoTarget::VOLUME)?constrain(1.0f+lfoVal,0.f,2.f):1.0f;
            buf[i]=(int16_t)(sample*vol*tg*envLevel*lfoVolMult);
        }
        M5Cardputer.Speaker.playRaw(buf,BUF,SAMPLE_RATE,false,1,CH,false);
        // Diagnostic: one buffer's real-time budget is BUF/SAMPLE_RATE ≈ 23220us.
        // Only accumulate simple counters here (fast, no I/O) — the actual
        // Serial.printf() summary line happens once a second from loop() on
        // Core 1 instead (see diagPrintPending below). Even a single,
        // infrequent Serial write can occasionally block for some time on
        // USB CDC (particularly with a host actively reading, e.g. a serial
        // monitor open) — enough to delay the next buffer's start. Moving
        // it off Core 0 entirely removes that as a possible contributor.
        unsigned long bufUs=micros()-bufStartUs;
        diagCount++;
        diagSumUs+=bufUs;
        if(bufUs>diagMaxUs)diagMaxUs=bufUs;
        if(bufUs>23220)diagOverCount++;
        unsigned long nowMs=millis();
        if(diagWindowStartMs==0)diagWindowStartMs=nowMs;
        if(nowMs-diagWindowStartMs>=1000&&!diagPrintPending){
            diagPrintCount=diagCount;diagPrintSumUs=diagSumUs;diagPrintMaxUs=diagMaxUs;diagPrintOverCount=diagOverCount;
            diagPrintPending=true;
            diagWindowStartMs=nowMs;diagCount=0;diagOverCount=0;diagSumUs=0;diagMaxUs=0;
        }
        // Yield briefly, but not every single buffer — vTaskDelay(1) can
        // actually take longer than 1ms depending on the system tick rate,
        // and doing it every ~23ms buffer adds up. Since we only need to
        // give Core 0's idle task/watchdog a chance far more often than its
        // ~5s timeout, yielding every 8th buffer (~every 185ms) is still a
        // huge safety margin while cutting this overhead 8x.
        static uint8_t yieldCounter=0;
        if(++yieldCounter>=8){yieldCounter=0;vTaskDelay(1);}
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
        case ImuTarget::VOLUME:        params.volumeScaleTarget=constrain(1.0f-fabsf(value),0.f,1.f);break;
        case ImuTarget::PITCH_BEND:    params.pitchBendCentsTarget=value*keyBendMaxCents;break;
        case ImuTarget::BEND_UP:       params.pitchBendCentsTarget=fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BEND_DOWN:     params.pitchBendCentsTarget=-fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BITCRUSH:      params.bitcrushTarget=value;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffsetTarget=value;break;
        case ImuTarget::PWM:           params.pwmOffsetTarget=value*0.4f;break;
        case ImuTarget::DETUNE:        params.detuneOffsetTarget=value*50.f;break;
        case ImuTarget::NOISE:         params.noiseOffsetTarget=value;break;
        case ImuTarget::SUB_LEVEL:     params.subLevelOffsetTarget=value;break;
        case ImuTarget::RESONANCE:     params.resonanceOffsetTarget=value*3.f;break;
        case ImuTarget::LFO_RATE:      lfoRateOffsetTarget=value*LFO_RATE_MAX;break;
        case ImuTarget::LFO_DEPTH:     lfoDepthOffsetTarget=value;break;
        case ImuTarget::ARP_TEMPO:
            if(lastMainMode==AppMode::SEQ)seqTempoOffsetTarget=value*100.f;
            else                          arpTempoOffsetTarget=value*100.f;
            break; // +/-100 BPM
        case ImuTarget::ARP_SWING:
            if(lastMainMode==AppMode::SEQ)seqSwingOffsetTarget=value*50.f;
            else                          arpSwingOffsetTarget=value*50.f;
            break; // +/-50%
        default:break;
    }
}

void resetParamToDefault(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        params.timbreMorph=params.timbreMorphTarget=0;break;
        case ImuTarget::VIBRATO_DEPTH: params.vibratoDepth=params.vibratoDepthTarget=0;break;
        case ImuTarget::VIBRATO_RATE:  params.vibratoRateHz=params.vibratoRateHzTarget=5;break;
        case ImuTarget::TREMOLO:       params.tremoloDepth=params.tremoloDepthTarget=0;break;
        case ImuTarget::VOLUME:        params.volumeScale=params.volumeScaleTarget=1.0f;break;
        case ImuTarget::PITCH_BEND:
        case ImuTarget::BEND_UP:
        case ImuTarget::BEND_DOWN:     params.pitchBendCents=params.pitchBendCentsTarget=0;break;
        case ImuTarget::BITCRUSH:      params.bitcrush=params.bitcrushTarget=0;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffset=params.filterCutoffOffsetTarget=0;break;
        case ImuTarget::PWM:           params.pwmOffset=params.pwmOffsetTarget=0;break;
        case ImuTarget::DETUNE:        params.detuneOffset=params.detuneOffsetTarget=0;break;
        case ImuTarget::NOISE:         params.noiseOffset=params.noiseOffsetTarget=0;break;
        case ImuTarget::SUB_LEVEL:     params.subLevelOffset=params.subLevelOffsetTarget=0;break;
        case ImuTarget::RESONANCE:     params.resonanceOffset=params.resonanceOffsetTarget=0;break;
        case ImuTarget::LFO_RATE:      lfoRateOffset=lfoRateOffsetTarget=0;break;
        case ImuTarget::LFO_DEPTH:     lfoDepthOffset=lfoDepthOffsetTarget=0;break;
        case ImuTarget::ARP_TEMPO:
            arpTempoOffset=arpTempoOffsetTarget=0;
            seqTempoOffset=seqTempoOffsetTarget=0;
            break;
        case ImuTarget::ARP_SWING:
            arpSwingOffset=arpSwingOffsetTarget=0;
            seqSwingOffset=seqSwingOffsetTarget=0;
            break;
        default:break;
    }
}

// Applies deadzone remapping (rescales the remaining range so there's no
// jump at the boundary) then an optional exponential response curve.
float applyDeadzoneAndCurve(float n,float deadzone,bool exponential){
    float s=(n<0.f)?-1.f:1.f;
    float a=fabsf(n);
    if(deadzone>0.001f){
        a=(a<deadzone)?0.f:(a-deadzone)/(1.f-deadzone);
    }
    if(exponential)a=a*a;
    return s*constrain(a,0.f,1.f);
}

// Full per-axis processing: calibration offset -> sensitivity -> invert -> deadzone/curve.
float computeAxisNorm(float angleDeg,const ImuAxisConfig &cfg){
    float adj=angleDeg-cfg.calOffsetDeg;
    float n=constrain((adj/TILT_MAX_DEGREES)*cfg.sensitivity,-1.f,1.f);
    if(cfg.invert)n=-n;
    return applyDeadzoneAndCurve(n,cfg.deadzone,cfg.exponential);
}

bool imuBipolarAuto(ImuTarget t,bool cfg){
    if(t==ImuTarget::PITCH_BEND)return true;
    if(t==ImuTarget::BEND_UP||t==ImuTarget::BEND_DOWN)return false;
    if(t==ImuTarget::ARP_TEMPO||t==ImuTarget::ARP_SWING)return true; // always +/-, like Pitch Bend
    return cfg;
}

// Original Cardputer has no IMU, so tilt is substituted with key input:
// ';'/'.' move a virtual Y axis up/down, ','/'/' move a virtual X axis
// left/right — moves toward the extreme while held, springs back to
// center on release (unless that axis's Hold is toggled on via A/S,
// matching the IMU hold behavior). Deadzone and Calibration don't apply
// to a key-driven virtual axis, so those items are hidden from the PAD
// sub-menu entirely (see imuMenuItemsOriginal).
float padVirtualX=0.f, padVirtualY=0.f; // -1..1, key-driven substitute for tilt
constexpr float PAD_MOVE_RATE=0.06f;    // fraction of full range moved per poll while held
constexpr float PAD_SPRING_RATE=0.15f;  // fraction of the way back to center per poll

void updatePadVirtualAxes(){
    // ';' '.' ',' '/' double as menu navigation on every other screen, so
    // only let them move the PAD while actually on MAIN — otherwise
    // scrolling through VCO/VCF/etc. would silently drag the PAD position
    // around in the background.
    if(appMode!=AppMode::PLAY)return;
    auto s=M5Cardputer.Keyboard.keysState();
    bool yUp=false,yDown=false,xRight=false,xLeft=false;
    for(char c:s.word){
        if(c==';')yUp=true;   if(c=='.')yDown=true;
        if(c=='/')xRight=true;if(c==',')xLeft=true;
    }
    if(yUp)             padVirtualY=min(padVirtualY+PAD_MOVE_RATE,1.f);
    else if(yDown)      padVirtualY=max(padVirtualY-PAD_MOVE_RATE,-1.f);
    else if(!imuYHeld)  padVirtualY+=(0.f-padVirtualY)*PAD_SPRING_RATE;

    if(xRight)           padVirtualX=min(padVirtualX+PAD_MOVE_RATE,1.f);
    else if(xLeft)       padVirtualX=max(padVirtualX-PAD_MOVE_RATE,-1.f);
    else if(!imuXHeld)   padVirtualX+=(0.f-padVirtualX)*PAD_SPRING_RATE;
}

void updateImu(){
    if(isCardputerAdv){
        if(!M5.Imu.update())return;
        auto data=M5.Imu.getImuData();
        lastAccelX=data.accel.x;lastAccelY=data.accel.y;
        auto clamp1=[](float v){return constrain(v,-1.f,1.f);};
        float aX=asinf(clamp1(lastAccelX))*180/PI;
        float aY=asinf(clamp1(lastAccelY))*180/PI;
        lastAngleXDeg=aX;lastAngleYDeg=aY;
        if(imuAxisX.target!=ImuTarget::NONE&&!imuXHeld){
            float n=computeAxisNorm(aX,imuAxisX);
            float applied=imuBipolarAuto(imuAxisX.target,imuAxisX.bipolar)?n:fabsf(n);
            imuXLastNorm=applied;
            applyImuValue(imuAxisX.target,applied);
        }
        if(imuAxisY.target!=ImuTarget::NONE&&!imuYHeld){
            float n=computeAxisNorm(aY,imuAxisY);
            float applied=imuBipolarAuto(imuAxisY.target,imuAxisY.bipolar)?n:fabsf(n);
            imuYLastNorm=applied;
            applyImuValue(imuAxisY.target,applied);
        }
    } else {
        updatePadVirtualAxes();
        if(imuAxisX.target!=ImuTarget::NONE){
            float n=constrain(padVirtualX*imuAxisX.sensitivity,-1.f,1.f);
            if(imuAxisX.invert)n=-n;
            if(imuAxisX.exponential){float sgn=(n<0.f)?-1.f:1.f;n=sgn*n*n;}
            float applied=imuBipolarAuto(imuAxisX.target,imuAxisX.bipolar)?n:fabsf(n);
            imuXLastNorm=applied;
            applyImuValue(imuAxisX.target,applied);
        }
        if(imuAxisY.target!=ImuTarget::NONE){
            float n=constrain(padVirtualY*imuAxisY.sensitivity,-1.f,1.f);
            if(imuAxisY.invert)n=-n;
            if(imuAxisY.exponential){float sgn=(n<0.f)?-1.f:1.f;n=sgn*n*n;}
            float applied=imuBipolarAuto(imuAxisY.target,imuAxisY.bipolar)?n:fabsf(n);
            imuYLastNorm=applied;
            applyImuValue(imuAxisY.target,applied);
        }
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
        case ImuTarget::PWM:       return "PWM";
        case ImuTarget::DETUNE:    return "Detune";
        case ImuTarget::NOISE:     return "Noise";
        case ImuTarget::SUB_LEVEL: return "SubLevel";
        case ImuTarget::RESONANCE: return "Resonance";
        case ImuTarget::LFO_RATE:  return "LFO Rate";
        case ImuTarget::LFO_DEPTH: return "LFO Depth";
        case ImuTarget::ARP_TEMPO:   return (lastMainMode==AppMode::SEQ)?"SeqTempo":"ArpTempo";
        case ImuTarget::ARP_SWING: return (lastMainMode==AppMode::SEQ)?"SeqSwing":"ArpSwing";
        default:return "?";
    }
}

// ==========================================================
// Key helpers
// ==========================================================
float resolveFreqFromKeys(){
    auto s=M5Cardputer.Keyboard.keysState();
    for(auto it=s.word.rbegin();it!=s.word.rend();++it){
        for(int i=0;i<12;i++)
            if(ROW1_KEYS[i]==*it)
                return row1Freqs[i]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
        for(int i=0;i<13;i++)
            if(ROW2_KEYS[i]==*it)
                return row2Freqs[i]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    }
    // Row 1's 13th note is the Backspace/Delete key — not a printable
    // character, so KeysState reports it via .del instead of .word.
    // Checked last (lowest priority vs. any note key).
    if(s.del)
        return row1Freqs[12]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    return 0.f;
}

// ---------------------------------------------------------
// Arpeggiator (CardputerADV only — original Cardputer's 3-key rollover
// limit can't reliably support the multi-key chord holding this needs)
// ---------------------------------------------------------
enum class ArpType : uint8_t { UP, DOWN, UP_DOWN, AS_PLAYED, RANDOM };
constexpr int ARP_MAX_NOTES=6;

bool arpEnabled=false;
ArpType arpType=ArpType::UP;
float arpTempoBpm=120.0f; // 40-240
float arpSwing=0.0f;    // -100 to +100%: positive delays the off-beat step (laid-back), negative pushes it earlier (anticipated/"pushed" feel)

float arpHeldFreqs[ARP_MAX_NOTES];   // press order
float arpSortedFreqs[ARP_MAX_NOTES]; // pitch order, for Up/Down/Up-Down
int   arpHeldCount=0;
int   arpStepIndex=0;
unsigned long arpLastStepMs=0;

// Latch mode ('V' key): on a cramped keyboard, reliably holding several
// keys down at once is hard — a finger lifting even briefly drops that
// note from the chord. With Latch on, each note-key press TOGGLES that
// note's membership in the held chord instead, so a chord can be built
// up one tap at a time without needing continuous physical holds.
bool arpLatchEnabled=false;
constexpr char ARP_DEL_SENTINEL='\x7F'; // stands in for the Backspace/del "key" in the latched-keys list
char arpLatchedKeys[ARP_MAX_NOTES];
int  arpLatchedCount=0;
char arpPrevWord[16]; int arpPrevWordLen=0;
bool arpPrevDel=false;

void arpLatchToggle(){
    arpLatchEnabled=!arpLatchEnabled;
    arpLatchedCount=0; // always start fresh, whichever direction we're toggling
}

bool noteKeyBaseFreq(char c,float &freqOut){
    for(int i=0;i<12;i++)if(ROW1_KEYS[i]==c){freqOut=row1Freqs[i];return true;}
    for(int i=0;i<13;i++)if(ROW2_KEYS[i]==c){freqOut=row2Freqs[i];return true;}
    return false;
}

// Scans every currently-held note key (up to ARP_MAX_NOTES) across both
// rows, in press order, and builds a pitch-sorted copy for the
// pitch-ordered arp types. In Latch mode, builds the chord from toggled
// membership instead of continuous physical hold (see arpLatchToggle).
void updateArpHeldNotes(){
    auto s=M5Cardputer.Keyboard.keysState();
    float mult=powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);

    if(arpLatchEnabled){
        for(char c:s.word){
            bool wasPressed=false;
            for(int i=0;i<arpPrevWordLen;i++)if(arpPrevWord[i]==c){wasPressed=true;break;}
            if(wasPressed)continue;
            float dummy;
            if(!noteKeyBaseFreq(c,dummy))continue;
            int foundIdx=-1;
            for(int i=0;i<arpLatchedCount;i++)if(arpLatchedKeys[i]==c){foundIdx=i;break;}
            if(foundIdx>=0){
                for(int j=foundIdx;j<arpLatchedCount-1;j++)arpLatchedKeys[j]=arpLatchedKeys[j+1];
                arpLatchedCount--;
            } else if(arpLatchedCount<ARP_MAX_NOTES){
                arpLatchedKeys[arpLatchedCount++]=c;
            }
        }
        if(s.del&&!arpPrevDel){
            int foundIdx=-1;
            for(int i=0;i<arpLatchedCount;i++)if(arpLatchedKeys[i]==ARP_DEL_SENTINEL){foundIdx=i;break;}
            if(foundIdx>=0){
                for(int j=foundIdx;j<arpLatchedCount-1;j++)arpLatchedKeys[j]=arpLatchedKeys[j+1];
                arpLatchedCount--;
            } else if(arpLatchedCount<ARP_MAX_NOTES){
                arpLatchedKeys[arpLatchedCount++]=ARP_DEL_SENTINEL;
            }
        }
        arpPrevWordLen=0;
        for(char c:s.word){if(arpPrevWordLen<16)arpPrevWord[arpPrevWordLen++]=c;}
        arpPrevDel=s.del;

        arpHeldCount=0;
        for(int k=0;k<arpLatchedCount&&arpHeldCount<ARP_MAX_NOTES;k++){
            char c=arpLatchedKeys[k];
            float freq=0.f;
            if(c==ARP_DEL_SENTINEL)freq=row1Freqs[12];
            else noteKeyBaseFreq(c,freq);
            if(freq>0.f)arpHeldFreqs[arpHeldCount++]=freq*mult;
        }
    } else {
        arpHeldCount=0;
        for(char c:s.word){
            if(arpHeldCount>=ARP_MAX_NOTES)break;
            float freq=0.f;
            if(noteKeyBaseFreq(c,freq))arpHeldFreqs[arpHeldCount++]=freq*mult;
        }
        if(s.del&&arpHeldCount<ARP_MAX_NOTES)arpHeldFreqs[arpHeldCount++]=row1Freqs[12]*mult;
    }

    for(int i=0;i<arpHeldCount;i++)arpSortedFreqs[i]=arpHeldFreqs[i];
    for(int i=1;i<arpHeldCount;i++){ // insertion sort, n<=6
        float key=arpSortedFreqs[i];int j=i-1;
        while(j>=0&&arpSortedFreqs[j]>key){arpSortedFreqs[j+1]=arpSortedFreqs[j];j--;}
        arpSortedFreqs[j+1]=key;
    }
}

int nextArpIndex(){
    int n=arpHeldCount;
    if(n<=1)return 0;
    switch(arpType){
        case ArpType::UP:        return arpStepIndex%n;
        case ArpType::DOWN:      return n-1-(arpStepIndex%n);
        case ArpType::UP_DOWN: {
            int cycle=2*(n-1);
            int pos=arpStepIndex%cycle;
            return (pos<n)?pos:(cycle-pos);
        }
        case ArpType::AS_PLAYED: return arpStepIndex%n;
        case ArpType::RANDOM:    return random(0,n);
    }
    return 0;
}

// Force-retriggers the envelope for this step, even if the frequency
// happens to repeat — that's what gives the arpeggio its percussive,
// stepped character rather than a smooth glide between notes.
void triggerArpStep(float freq){
    currentFreq=freq;
    if(envPhase==EnvPhase::IDLE)envLevel=0.f;
    envPhase=EnvPhase::ATTACK;
    filterEnvPhase=EnvPhase::ATTACK;
    if(portaEnabled&&portaFreq<=0.f)portaFreq=freq;
}

// Rate: note length each arp step represents, relative to BPM's quarter
// note (e.g. Rate=1/8 means 2 steps per beat instead of 1).
struct ArpRateOption { const char *label; float mult; };
const ArpRateOption ARP_RATES[]={
    {"1/1",  4.0f},{"1/2",  2.0f},{"1/4",  1.0f},{"1/8",  0.5f},
    {"1/16", 0.25f},{"1/32",0.125f},{"1/8T", 1.f/3.f},{"1/16T",1.f/6.f},
};
constexpr int NUM_ARP_RATES=sizeof(ARP_RATES)/sizeof(ARP_RATES[0]);
int arpRateIndex=2; // default 1/4, matching the original fixed behavior

float arpLastTriggeredFreq=0.f; // for MAIN screen: which held note is currently sounding

void updateArpTiming(){
    if(arpHeldCount==0){
        if(currentFreq!=0.f)currentFreq=0.f; // let it release naturally
        return;
    }
    unsigned long now=millis();
    float bpm=constrain(arpTempoBpm+arpTempoOffset,40.f,240.f);
    float baseStepMs=60000.0f/bpm*ARP_RATES[arpRateIndex].mult;
    float swingFactor=constrain(arpSwing+arpSwingOffset,-100.f,100.f)/100.f;
    bool isOffBeat=(arpStepIndex%2==1);
    // Positive Swing delays the off-beat step (classic long-short swing
    // feel); negative pushes it earlier instead (anticipated/"pushed" feel).
    float stepMs=isOffBeat?baseStepMs*(1.f+swingFactor*0.5f):baseStepMs*(1.f-swingFactor*0.5f);
    if(now-arpLastStepMs>=(unsigned long)stepMs){
        arpLastStepMs=now;
        int idx=nextArpIndex();
        bool pitchOrdered=(arpType==ArpType::UP||arpType==ArpType::DOWN||arpType==ArpType::UP_DOWN);
        float freq=pitchOrdered?arpSortedFreqs[idx]:arpHeldFreqs[idx];
        triggerArpStep(freq);
        arpLastTriggeredFreq=freq;
        arpStepIndex++;
    }
}

// ---------------------------------------------------------
// Step Sequencer (16 steps, CardputerADV only for hardware auto-detect
// purposes only — the Sequencer itself works on both boards). Has its
// own independent Tempo/Swing, separate from the Arpeggiator's.
// ---------------------------------------------------------
struct SeqStep {
    float freq=0.f;       // 0 = rest
    uint8_t velocity=100; // base velocity; Accent boosts this further
    bool tie=false;       // extend the previous note — no retrigger, no pitch change
    bool slide=false;     // glide from the previous pitch to this one — no retrigger
    bool accent=false;    // boost velocity + filter cutoff for this step
};
constexpr int SEQ_NUM_STEPS=16;
SeqStep seqSteps[SEQ_NUM_STEPS];
int  seqCursorStep=0;    // which step is being edited
int  seqPlayStep=0;      // current playback position
bool seqPlaying=false;
// Two separate editing "focuses": STEP (this step's Velocity/Gate) and
// PATTERN (the whole sequence's Tempo/Swing) — kept conceptually separate
// per user feedback, rather than one flat 4-way cycle. 'b' toggles focus;
// 'g' cycles the 2-way choice within whichever focus is currently active.
// All four values (Vel/Gate/Tempo/Swing) are always shown on screen
// regardless of focus; only the active one is marked.
enum class SeqFocus : uint8_t { STEP, PATTERN };
SeqFocus seqFocus=SeqFocus::STEP;
enum class SeqStepTarget : uint8_t { VELOCITY, TIE, SLIDE, ACCENT };
SeqStepTarget seqStepTarget=SeqStepTarget::VELOCITY;
enum class SeqPatternTarget : uint8_t { TEMPO, SWING };
SeqPatternTarget seqPatternTarget=SeqPatternTarget::TEMPO;
bool prevSeqFocusKeyPressed=false;
unsigned long seqLastStepMs=0;
float prevSeqEntryFreq=0.f;
bool prevSeqDelPressed=false;
bool prevSeqCursorLeftPressed=false, prevSeqCursorRightPressed=false;
bool prevSeqVelIncPressed=false, prevSeqVelDecPressed=false;
bool prevSeqGateKeyPressed=false, prevSeqPlayKeyPressed=false;
float seqTempoBpm=120.0f; // 40-240, independent of Arp's Tempo
uint8_t seqLastUsedVelocity=100; // carries forward to new note entries, so you're not stuck starting at 100 every time

// Song playback (declared early — updateSeqTiming() below reads these to
// apply per-entry Transpose and detect when to advance to the next
// entry; the rest of Song's state/logic lives further down near its
// data model and editor UI).
bool  songPlaying=false;
float songTransposeMult=1.0f; // current entry's chromatic Transpose, applied on top of each step's stored freq
void  songAdvanceOnPassComplete(); // forward declaration — defined near the rest of Song's playback logic below

// Copy/Cut/Paste: 'V' marks/confirms a step-range selection, Shift+C
// copies it, Shift+X cuts it, Enter pastes at the cursor. seqSelStart/End
// are -1 when there's no selection; while actively marking (seqSelMarking)
// they track [min(anchor,cursor), max(anchor,cursor)] live as the cursor
// moves, then freeze in place once confirmed.
int  seqSelAnchor=-1, seqSelStart=-1, seqSelEnd=-1;
bool seqSelMarking=false;
SeqStep seqClipboard[SEQ_NUM_STEPS];
int  seqClipboardLen=0;
bool prevSeqMarkKeyPressed=false, prevSeqCopyKeyPressed=false, prevSeqCutKeyPressed=false, prevSeqPasteKeyPressed=false;
float seqSwing=0.0f;      // -100 to +100%, independent of Arp's Swing

void seqTogglePlay(){
    seqPlaying=!seqPlaying;
    if(seqPlaying){
        seqPlayStep=0;
        seqLastStepMs=millis();
    } else {
        currentFreq=0.f;
        seqSliding=false;
        seqAccentCutoffBoostTarget=0.f;
        seqAccentResoBoostTarget=0.f;
        seqVelocityMult=1.0f;
    }
}

// Reuses the existing note-key tables directly (not resolveFreqFromKeys(),
// since that treats Backspace as a 13th note — here Backspace instead
// means "clear the selected step", handled separately below).
float seqResolveFreqExcludingDel(){
    auto s=M5Cardputer.Keyboard.keysState();
    float mult=powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    for(auto it=s.word.rbegin();it!=s.word.rend();++it){
        float base;
        if(noteKeyBaseFreq(*it,base))return base*mult;
    }
    return 0.f;
}

bool prevSeqOctUpPressed=false, prevSeqOctDownPressed=false;
bool prevSeqTrUpPressed=false, prevSeqTrDownPressed=false;

void updateSeqEditing(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool curL=false,curR=false,vInc=false,vDec=false,gateKey=false,playKey=false,focusKey=false;
    bool octUp=false,octDown=false,trUp=false,trDown=false;
    bool markKey=false,copyKey=false,cutKey=false;
    bool pasteKey=s.enter; // Enter: paste clipboard at cursor (checked directly, not in the s.word loop below, since Enter alone wouldn't otherwise populate s.word)
    for(char c:s.word){
        if(s.shift){
            // Shift + the same physical keys PLAY uses for Octave/Transpose,
            // so the muscle memory carries over even though SEQ needs
            // those same keys (unshifted) for its own step/vel/gate controls.
            if(c==';')octUp=true;   if(c=='.')octDown=true;
            if(c=='/')trUp=true;    if(c==',')trDown=true;
            if(c=='c'||c=='C')copyKey=true; // Shift+C: copy selection
            if(c=='x'||c=='X')cutKey=true;  // Shift+X: cut selection
        } else {
            if(c==',')curL=true;    if(c=='/')curR=true;
            if(c==';')vInc=true;    if(c=='.')vDec=true;
            if(c=='v')markKey=true; // V: mark/confirm/clear a step selection
        }
        // Defensive fallback in case the keyboard reports the shifted
        // symbol directly instead of a separate shift flag + base char.
        if(c==':')octUp=true; if(c=='>')octDown=true;
        if(c=='?')trUp=true;  if(c=='<')trDown=true;
        if(c=='g')gateKey=true;
        if(c=='f')focusKey=true; // toggle STEP <-> PATTERN focus
        if(c==' ')playKey=true;
    }

    if(curL&&!prevSeqCursorLeftPressed) seqCursorStep=(seqCursorStep-1+SEQ_NUM_STEPS)%SEQ_NUM_STEPS;
    if(curR&&!prevSeqCursorRightPressed)seqCursorStep=(seqCursorStep+1)%SEQ_NUM_STEPS;
    prevSeqCursorLeftPressed=curL;prevSeqCursorRightPressed=curR;

    // Copy/Cut/Paste/Mark: kept independent of STEP/PATTERN focus, since
    // selection is inherently about steps regardless of what ;/. adjusts.
    if(seqSelMarking){
        seqSelStart=min(seqSelAnchor,seqCursorStep);
        seqSelEnd=max(seqSelAnchor,seqCursorStep);
    }
    if(markKey&&!prevSeqMarkKeyPressed){
        if(seqSelAnchor<0){
            // No selection yet — start marking from here.
            seqSelAnchor=seqCursorStep; seqSelStart=seqSelEnd=seqCursorStep; seqSelMarking=true;
        } else if(seqSelMarking){
            // Currently marking — confirm/freeze the current range.
            seqSelMarking=false;
        } else {
            // Already confirmed — clear it.
            seqSelAnchor=seqSelStart=seqSelEnd=-1;
        }
    }
    prevSeqMarkKeyPressed=markKey;

    if(copyKey&&!prevSeqCopyKeyPressed&&seqSelStart>=0){
        seqClipboardLen=seqSelEnd-seqSelStart+1;
        for(int i=0;i<seqClipboardLen;i++)seqClipboard[i]=seqSteps[seqSelStart+i];
    }
    prevSeqCopyKeyPressed=copyKey;

    if(cutKey&&!prevSeqCutKeyPressed&&seqSelStart>=0){
        seqClipboardLen=seqSelEnd-seqSelStart+1;
        for(int i=0;i<seqClipboardLen;i++){
            seqClipboard[i]=seqSteps[seqSelStart+i];
            seqSteps[seqSelStart+i]=SeqStep();
        }
    }
    prevSeqCutKeyPressed=cutKey;

    if(pasteKey&&!prevSeqPasteKeyPressed&&seqClipboardLen>0){
        int n=min(seqClipboardLen,SEQ_NUM_STEPS-seqCursorStep); // truncate at step 16
        for(int i=0;i<n;i++)seqSteps[seqCursorStep+i]=seqClipboard[i];
    }
    prevSeqPasteKeyPressed=pasteKey;

    if(focusKey&&!prevSeqFocusKeyPressed)seqFocus=(seqFocus==SeqFocus::STEP)?SeqFocus::PATTERN:SeqFocus::STEP;
    prevSeqFocusKeyPressed=focusKey;

    if(gateKey&&!prevSeqGateKeyPressed){
        if(seqFocus==SeqFocus::STEP)seqStepTarget=(SeqStepTarget)(((uint8_t)seqStepTarget+1)%4);
        else                        seqPatternTarget=(seqPatternTarget==SeqPatternTarget::TEMPO)?SeqPatternTarget::SWING:SeqPatternTarget::TEMPO;
    }
    prevSeqGateKeyPressed=gateKey;

    if(seqFocus==SeqFocus::STEP){
        SeqStep &st=seqSteps[seqCursorStep];
        switch(seqStepTarget){
            case SeqStepTarget::VELOCITY:
                if(vInc&&!prevSeqVelIncPressed){st.velocity=min((int)st.velocity+5,100);seqLastUsedVelocity=st.velocity;}
                if(vDec&&!prevSeqVelDecPressed){st.velocity=max((int)st.velocity-5,0);seqLastUsedVelocity=st.velocity;}
                break;
            case SeqStepTarget::TIE:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.tie=!st.tie;
                break;
            case SeqStepTarget::SLIDE:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.slide=!st.slide;
                break;
            case SeqStepTarget::ACCENT:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.accent=!st.accent;
                break;
        }
    } else {
        if(seqPatternTarget==SeqPatternTarget::TEMPO){
            if(vInc&&!prevSeqVelIncPressed)seqTempoBpm=min(seqTempoBpm+5.f,240.f);
            if(vDec&&!prevSeqVelDecPressed)seqTempoBpm=max(seqTempoBpm-5.f,40.f);
        } else {
            if(vInc&&!prevSeqVelIncPressed)seqSwing=min(seqSwing+5.f,100.f);
            if(vDec&&!prevSeqVelDecPressed)seqSwing=max(seqSwing-5.f,-100.f);
        }
    }
    prevSeqVelIncPressed=vInc;prevSeqVelDecPressed=vDec;

    if(octUp&&!prevSeqOctUpPressed&&params.octaveShift<2)     params.octaveShift++;
    if(octDown&&!prevSeqOctDownPressed&&params.octaveShift>-2) params.octaveShift--;
    prevSeqOctUpPressed=octUp;prevSeqOctDownPressed=octDown;
    if(trUp&&!prevSeqTrUpPressed&&transposeSemitones<TRANSPOSE_MAX)    transposeSemitones++;
    if(trDown&&!prevSeqTrDownPressed&&transposeSemitones>TRANSPOSE_MIN)transposeSemitones--;
    prevSeqTrUpPressed=trUp;prevSeqTrDownPressed=trDown;

    if(playKey&&!prevSeqPlayKeyPressed)seqTogglePlay();
    prevSeqPlayKeyPressed=playKey;

    // Backspace/del clears the selected step entirely (note + tie/slide/
    // accent flags) back to a plain rest. Shift+Backspace clears the
    // whole 16-step pattern at once.
    if(s.del&&!prevSeqDelPressed){
        if(s.shift){
            for(int i=0;i<SEQ_NUM_STEPS;i++)seqSteps[i]=SeqStep();
        } else {
            seqSteps[seqCursorStep]=SeqStep();
        }
    }
    prevSeqDelPressed=s.del;

    // Any note key press assigns that pitch to the selected step, plays
    // a brief preview so you can hear what you're entering, and auto-
    // advances the cursor to the next step (use ,// to skip a step and
    // leave it as a rest, rather than pressing a note key for it).
    float curFreq=seqResolveFreqExcludingDel();
    if(curFreq>0.f&&curFreq!=prevSeqEntryFreq){
        seqSteps[seqCursorStep].freq=curFreq;
        seqSteps[seqCursorStep].velocity=seqLastUsedVelocity;
        if(!seqPlaying){
            currentFreq=curFreq;
            portaFreq=curFreq; // preview should be instantly accurate, not mid-glide
            seqVelocityMult=1.0f;
            if(envPhase==EnvPhase::IDLE)envLevel=0.f;
            envPhase=EnvPhase::ATTACK;
            filterEnvPhase=EnvPhase::ATTACK;
        }
        seqCursorStep=(seqCursorStep+1)%SEQ_NUM_STEPS;
    } else if(curFreq<=0.f&&prevSeqEntryFreq>0.f&&!seqPlaying){
        // Key released — stop the preview.
        currentFreq=0.f;
    }
    prevSeqEntryFreq=curFreq;
}

void updateSeqTiming(){
    if(!seqPlaying)return;
    unsigned long now=millis();
    float bpm=constrain(seqTempoBpm+seqTempoOffset,40.f,240.f);
    float baseStepMs=60000.0f/bpm/4.0f; // 16th notes: 16 steps = one bar at this tempo
    float swingFactor=constrain(seqSwing+seqSwingOffset,-100.f,100.f)/100.f;
    bool isOffBeat=(seqPlayStep%2==1);
    float stepMs=isOffBeat?baseStepMs*(1.f+swingFactor*0.5f):baseStepMs*(1.f-swingFactor*0.5f);
    if(now-seqLastStepMs>=(unsigned long)stepMs){
        seqLastStepMs=now;
        SeqStep &st=seqSteps[seqPlayStep];
        float tMult=songPlaying?songTransposeMult:1.0f; // Song's per-entry chromatic Transpose; 1.0 (no-op) outside Song playback
        if(st.tie&&currentFreq>0.f){
            // Extend the currently-sounding note — no retrigger, no pitch
            // change, no envelope reset. Checked before the Rest check
            // below so a Tie step doesn't need its own note assigned —
            // it just continues whatever's already sounding.
        } else if(st.freq<=0.f){
            // Rest — silence, let the amp envelope's own Release handle the tail.
            currentFreq=0.f;
            seqSliding=false;
            seqAccentCutoffBoostTarget=0.f;
            seqAccentResoBoostTarget=0.f;
        } else {
            float vel=st.velocity;
            if(st.accent)vel=min(100.f,vel*SEQ_ACCENT_VELOCITY_MULT);
            seqVelocityMult=constrain(vel/100.f,0.f,1.f);
            seqAccentCutoffBoostTarget=st.accent?SEQ_ACCENT_CUTOFF_BOOST:0.f;
            seqAccentResoBoostTarget=st.accent?SEQ_ACCENT_RESO_BOOST:0.f;
            if(st.slide&&currentFreq>0.f){
                // Glide from the current pitch to the new one — no
                // retrigger, so the envelope/amplitude just continues.
                seqSlideFreq=currentFreq;
                seqSliding=true;
                currentFreq=st.freq*tMult;
            } else {
                // Normal note-on (or a tie/slide with nothing previously
                // sounding to extend/glide from) — full retrigger.
                seqSliding=false;
                currentFreq=st.freq*tMult;
                if(envPhase==EnvPhase::IDLE)envLevel=0.f;
                envPhase=EnvPhase::ATTACK;
                filterEnvPhase=EnvPhase::ATTACK;
            }
        }
        seqPlayStep=(seqPlayStep+1)%SEQ_NUM_STEPS;
        if(songPlaying&&seqPlayStep==0)songAdvanceOnPassComplete();
    }
}

void updateOctaveAndVolume(){
    if(appMode==AppMode::PATCH||appMode==AppMode::PATTERN)return;
    auto s=M5Cardputer.Keyboard.keysState();
    bool oU=false,oD=false,vU=false,vD=false,bD=false,bU=false;
    bool iXH=false,iYH=false,nH=false,pOn=false,hKey=false,seqPlayKey=false;
    bool trU=false,trD=false,latchKey=false,arpToggleKey=false;
    for(char c:s.word){
        if(isCardputerAdv){
            // ';'/'.'/'/'/',' double as SEQ's own step-cursor/velocity/gate
            // keys, so only let them mean octave/transpose while on PLAY.
            if(appMode==AppMode::PLAY){
                if(c==';')oU=true;  if(c=='.')oD=true;
                if(c=='/')trU=true; if(c==',')trD=true;
            }
            if(s.shift&&c=='v')arpToggleKey=true; // Shift+V: Arp on/off, from anywhere except Patch
            else if(c=='v'&&appMode!=AppMode::SEQ)latchKey=true; // V (unshifted): Arp latch toggle — in SEQ, plain V instead marks/confirms a step selection (see updateSeqEditing())
            if(c=='V')arpToggleKey=true;          // defensive: in case shifted letters are reported uppercase directly
        } else {
            // Original Cardputer: ;/./,// are reserved for PAD (virtual
            // tilt) control instead, so octave/transpose move to keys
            // that don't collide with that — nor with SEQ's own keys.
            if(c=='j')oU=true;  if(c=='n')oD=true;
            if(c=='m')trU=true; if(c=='b')trD=true;
        }
        // Shift+L/Shift+S are reserved for SONG's Load/Save (see
        // updateSongEditor()) — skip the plain volume/IMU-hold meaning
        // there so they don't also fire alongside Load/Save.
        if(c=='l'&&!(appMode==AppMode::SONG&&s.shift))vU=true;  if(c=='k')vD=true;
        // Shift+C/Shift+X are reserved for SEQ's Copy/Cut (see
        // updateSeqEditing()) — skip the plain portamento/bend meaning
        // there so they don't also fire alongside Copy/Cut.
        if(c=='z')bD=true;
        if(c=='x'&&!(appMode==AppMode::SEQ&&s.shift))bU=true;
        if(c=='a')iXH=true; if(c=='s'&&!(appMode==AppMode::SONG&&s.shift))iYH=true;
        if(c=='d')nH=true;
        if(c=='c'&&!(appMode==AppMode::SEQ&&s.shift))pOn=true;
        if(c=='h')hKey=true;
        // Space: Sequencer Play/Stop from anywhere except Patch. SEQ and
        // SONG each already handle Space themselves (updateSeqEditing()/
        // updateSongEditor()), so skip it here to avoid double-toggling
        // in the same frame.
        if(c==' '&&appMode!=AppMode::SEQ&&appMode!=AppMode::SONG)seqPlayKey=true;
    }

    // Octave (edge-triggered)
    if(oU&&!prevOctaveUpPressed   &&params.octaveShift<2) params.octaveShift++;
    if(oD&&!prevOctaveDownPressed &&params.octaveShift>-2)params.octaveShift--;
    prevOctaveUpPressed=oU;prevOctaveDownPressed=oD;

    // Transpose (edge-triggered): ',' down / '/' up
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

    // Arp latch toggle (V key, ADV only)
    if(latchKey&&!prevArpLatchPressed)arpLatchToggle();
    prevArpLatchPressed=latchKey;

    // Arp on/off toggle (Shift+V, ADV only) — usable anywhere except
    // Patch. This is now the ONLY way to toggle it (the redundant
    // SETTING > Arp entry was removed once this covered every screen).
    if(arpToggleKey&&!prevArpToggleKeyPressed)arpToggle();
    prevArpToggleKeyPressed=arpToggleKey;

    // Sequencer Play/Stop (Space) — usable anywhere except Patch/SEQ
    // itself (which has its own handling), so playback can be started
    // or stopped while tweaking VCO/VCF/etc without going back to SEQ.
    if(seqPlayKey&&!prevSeqPlayKeyPressedGlobal)seqTogglePlay();
    prevSeqPlayKeyPressedGlobal=seqPlayKey;

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

// Writes just the Sequencer pattern fields (tempo/swing/16 steps) to an
// already-open file — shared by the main settings save and Pattern Bank
// slot saves, so the exact field format only needs to be defined once.
void writeSeqPatternFields(File &f){
    f.printf("  \"seq_tempo\": %.1f,\n",seqTempoBpm);
    f.printf("  \"seq_swing\": %.1f,\n",seqSwing);
    for(int i=0;i<SEQ_NUM_STEPS;i++){
        f.printf("  \"seq%d_freq\": %.2f,\n",i,seqSteps[i].freq);
        f.printf("  \"seq%d_vel\": %d,\n",i,(int)seqSteps[i].velocity);
        f.printf("  \"seq%d_tie\": %d,\n",i,(int)seqSteps[i].tie);
        f.printf("  \"seq%d_slide\": %d,\n",i,(int)seqSteps[i].slide);
        f.printf("  \"seq%d_accent\": %d%s\n",i,(int)seqSteps[i].accent,(i==SEQ_NUM_STEPS-1)?"":",");
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
    f.printf("  \"imu_x_sens\": %.2f,\n",imuAxisX.sensitivity);
    f.printf("  \"imu_x_invert\": %d,\n",(int)imuAxisX.invert);
    f.printf("  \"imu_x_curve\": %d,\n",(int)imuAxisX.exponential);
    f.printf("  \"imu_x_deadzone\": %.3f,\n",imuAxisX.deadzone);
    f.printf("  \"imu_x_cal\": %.3f,\n",imuAxisX.calOffsetDeg);
    f.printf("  \"imu_y_sens\": %.2f,\n",imuAxisY.sensitivity);
    f.printf("  \"imu_y_invert\": %d,\n",(int)imuAxisY.invert);
    f.printf("  \"imu_y_curve\": %d,\n",(int)imuAxisY.exponential);
    f.printf("  \"imu_y_deadzone\": %.3f,\n",imuAxisY.deadzone);
    f.printf("  \"imu_y_cal\": %.3f,\n",imuAxisY.calOffsetDeg);
    f.printf("  \"imu_calibrated\": %d,\n",(int)imuCalibrated);
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
    f.printf("  \"lfo_target\": %u,\n",(unsigned)lfo.target);
    f.printf("  \"key_volume\": %.3f,\n",params.keyVolume);
    f.printf("  \"play_mode\": %u,\n",(unsigned)playMode);
    f.printf("  \"scale\": %d,\n",currentScaleIndex);
    f.printf("  \"arp_enabled\": %d,\n",(int)arpEnabled);
    f.printf("  \"arp_type\": %u,\n",(unsigned)arpType);
    f.printf("  \"arp_tempo\": %.1f,\n",arpTempoBpm);
    f.printf("  \"arp_swing\": %.1f,\n",arpSwing);
    f.printf("  \"arp_rate\": %d,\n",arpRateIndex);
    writeSeqPatternFields(f);
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
    else if(key=="imu_x_sens")imuAxisX.sensitivity=constrain(v,0.3f,3.0f);
    else if(key=="imu_x_invert")imuAxisX.invert=(bool)(int)v;
    else if(key=="imu_x_curve")imuAxisX.exponential=(bool)(int)v;
    else if(key=="imu_x_deadzone")imuAxisX.deadzone=constrain(v,0.f,0.3f);
    else if(key=="imu_x_cal")imuAxisX.calOffsetDeg=v;
    else if(key=="imu_y_sens")imuAxisY.sensitivity=constrain(v,0.3f,3.0f);
    else if(key=="imu_y_invert")imuAxisY.invert=(bool)(int)v;
    else if(key=="imu_y_curve")imuAxisY.exponential=(bool)(int)v;
    else if(key=="imu_y_deadzone")imuAxisY.deadzone=constrain(v,0.f,0.3f);
    else if(key=="imu_y_cal")imuAxisY.calOffsetDeg=v;
    else if(key=="imu_calibrated")imuCalibrated=(bool)(int)v;
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
    else if(key=="key_volume")params.keyVolume=constrain(v,0.f,1.f);
    else if(key=="play_mode"){uint8_t u=(uint8_t)v;if(u<=(uint8_t)PlayMode::PRO)playMode=(PlayMode)u;}
    else if(key=="scale"){int idx=(int)v;if(idx>=0&&idx<NUM_SCALES)currentScaleIndex=idx;}
    else if(key=="arp_enabled")arpEnabled=(bool)(int)v;
    else if(key=="arp_type"){uint8_t u=(uint8_t)v;if(u<=(uint8_t)ArpType::RANDOM)arpType=(ArpType)u;}
    else if(key=="arp_tempo")arpTempoBpm=constrain(v,40.f,240.f);
    else if(key=="arp_swing")arpSwing=constrain(v,-100.f,100.f);
    else if(key=="arp_rate"){int idx=(int)v;if(idx>=0&&idx<NUM_ARP_RATES)arpRateIndex=idx;}
    else if(key=="seq_tempo")seqTempoBpm=constrain(v,40.f,240.f);
    else if(key=="seq_swing")seqSwing=constrain(v,-100.f,100.f);
    else if(key.startsWith("seq")){
        // "seqN_freq" / "seqN_vel" / "seqN_gate"
        int us=key.indexOf('_');
        if(us>3){
            int idx=key.substring(3,us).toInt();
            if(idx>=0&&idx<SEQ_NUM_STEPS){
                String field=key.substring(us+1);
                if(field=="freq")seqSteps[idx].freq=v;
                else if(field=="vel")seqSteps[idx].velocity=(uint8_t)constrain(v,0.f,100.f);
                else if(field=="tie")seqSteps[idx].tie=(bool)(int)v;
                else if(field=="slide")seqSteps[idx].slide=(bool)(int)v;
                else if(field=="accent")seqSteps[idx].accent=(bool)(int)v;
            }
        }
    }
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

// ---------------------------------------------------------
// Pattern Bank: 8 lettered banks (A-H) x 8 numbered slots (1-8) = 64
// pattern slots, each a saved 16-step Sequencer pattern (steps + tempo +
// swing — the same fields as the main settings file's seq_*/seqN_*
// entries, just scoped to one file per slot under /CPS/Pattern instead
// of the whole synth's live state). Save reuses writeSeqPatternFields();
// load reuses parseSettingLine() line-by-line, since the field names are
// identical — a pattern file is really just a tiny settings file that
// only touches Sequencer fields.
// ---------------------------------------------------------
static const char *PATTERN_FOLDER_PATH = "/CPS/Pattern";
constexpr int NUM_PATTERN_BANKS = 8;         // A-H
constexpr int NUM_PATTERNS_PER_BANK = 8;     // 1-8

bool ensurePatternFolder(){return SD.exists(PATTERN_FOLDER_PATH)||SD.mkdir(PATTERN_FOLDER_PATH);}

String patternFilePath(int bank,int slot){
    char buf[40];
    snprintf(buf,sizeof(buf),"%s/%c%d.json",PATTERN_FOLDER_PATH,'A'+bank,slot+1);
    return String(buf);
}

bool patternSlotExists(int bank,int slot){
    return SD.exists(patternFilePath(bank,slot));
}

bool savePatternToSlot(int bank,int slot){
    if(!ensurePatternFolder()){Serial.println("[Pattern] folder create failed");return false;}
    File f=SD.open(patternFilePath(bank,slot),FILE_WRITE);
    if(!f){Serial.println("[Pattern] open failed");return false;}
    f.println("{");
    writeSeqPatternFields(f);
    f.println("}");
    f.close();
    Serial.printf("[Pattern] saved to %s\n",patternFilePath(bank,slot).c_str());
    return true;
}

bool loadPatternFromSlot(int bank,int slot){
    String path=patternFilePath(bank,slot);
    if(!SD.exists(path)){Serial.println("[Pattern] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    while(f.available())parseSettingLine(f.readStringUntil('\n'));
    f.close();
    Serial.printf("[Pattern] loaded from %s\n",path.c_str());
    return true;
}

bool deletePatternSlot(int bank,int slot){
    return SD.remove(patternFilePath(bank,slot));
}

// Song UI preview: reads a pattern's step shape (freq/tie/accent/slide
// only — velocity doesn't matter for a tiny preview) into a dedicated
// buffer, separate from the live seqSteps[] so scrolling through Song
// entries in the editor never disturbs whatever's actually loaded or
// playing. Cached by (bank,slot) so it only re-reads the SD card when
// the previewed entry actually changes.
SeqStep songPreviewSteps[SEQ_NUM_STEPS];
bool songPreviewValid=false;
int  songPreviewLoadedBank=-1, songPreviewLoadedSlot=-1;

void loadPatternPreview(int bank,int slot){
    if(songPreviewValid&&songPreviewLoadedBank==bank&&songPreviewLoadedSlot==slot)return;
    for(int i=0;i<SEQ_NUM_STEPS;i++)songPreviewSteps[i]=SeqStep();
    songPreviewLoadedBank=bank;songPreviewLoadedSlot=slot;
    String path=patternFilePath(bank,slot);
    if(!SD.exists(path)){songPreviewValid=false;return;}
    File f=SD.open(path,FILE_READ);
    if(!f){songPreviewValid=false;return;}
    while(f.available()){
        String line=f.readStringUntil('\n');
        line.trim();
        if(line.length()<3)continue;
        int c=line.indexOf(':');
        if(c<0)continue;
        String key=line.substring(0,c);key.trim();key.replace("\"","");
        String valStr=line.substring(c+1);valStr.trim();
        if(valStr.endsWith(","))valStr=valStr.substring(0,valStr.length()-1);
        float v=valStr.toFloat();
        if(key.length()>3&&key.startsWith("seq")&&key[3]>='0'&&key[3]<='9'){
            int us=key.indexOf('_');
            if(us<4)continue;
            int idx=key.substring(3,us).toInt();
            String field=key.substring(us+1);
            if(idx<0||idx>=SEQ_NUM_STEPS)continue;
            if(field=="freq")songPreviewSteps[idx].freq=v;
            else if(field=="tie")songPreviewSteps[idx].tie=(bool)(int)v;
            else if(field=="accent")songPreviewSteps[idx].accent=(bool)(int)v;
            else if(field=="slide")songPreviewSteps[idx].slide=(bool)(int)v;
        }
    }
    f.close();
    songPreviewValid=true;
}

// One fixed color per Bank letter (A-H), used by the Song timeline so
// each block is recognizable by pattern bank at a glance. Placeholder
// values here; recomputed properly in setup() via color565().
uint16_t songBankColors[NUM_PATTERN_BANKS];

enum class PatternBankMode : uint8_t { LOAD, SAVE };
PatternBankMode patternBankMode = PatternBankMode::LOAD;
int  patternSelBank=0;   // 0-7 (A-H)
int  patternSelSlot=0;   // 0-7 (1-8)
bool patternConfirmDelete=false;
bool patternConfirmOverwrite=false;
bool prevPatternUpPressed=false, prevPatternDownPressed=false;
bool prevPatternLeftPressed=false, prevPatternRightPressed=false;
bool prevPatternConfirmPressed=false, prevPatternDeleteKeyPressed=false, prevPatternTabPressed=false;

void enterPatternBank(PatternBankMode mode){
    patternBankMode=mode;
    patternConfirmDelete=false;patternConfirmOverwrite=false;
    // Seed edge-trackers with whatever is CURRENTLY held so the key press
    // that opened this screen isn't immediately re-read as a fresh press
    // inside the browser (same reasoning as enterPatchBrowser()).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldLeft=false,heldRight=false;
    for(char c:s.word){
        if(c==';')heldUp=true;   if(c=='.')heldDown=true;
        if(c==',')heldLeft=true; if(c=='/')heldRight=true;
    }
    prevPatternUpPressed=heldUp; prevPatternDownPressed=heldDown;
    prevPatternLeftPressed=heldLeft; prevPatternRightPressed=heldRight;
    prevPatternConfirmPressed=s.enter;
    prevPatternDeleteKeyPressed=s.del;
    prevPatternTabPressed=s.tab;
    appMode=AppMode::PATTERN;
}
void patternBankSaveEnter(){enterPatternBank(PatternBankMode::SAVE);}
void patternBankLoadEnter(){enterPatternBank(PatternBankMode::LOAD);}
const char *patternBankEnterLabel(){return "Select>";}

void updatePatternBank(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false;
    for(char c:s.word){
        if(c==';')up=true;   if(c=='.')down=true;
        if(c==',')left=true; if(c=='/')right=true;
    }
    bool confirmKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(patternConfirmDelete){
        for(char c:s.word){
            if(c=='y'||c=='Y'){deletePatternSlot(patternSelBank,patternSelSlot);patternConfirmDelete=false;}
            if(c=='n'||c=='N')patternConfirmDelete=false;
        }
        return;
    }
    if(patternConfirmOverwrite){
        for(char c:s.word){
            if(c=='y'||c=='Y'){savePatternToSlot(patternSelBank,patternSelSlot);patternConfirmOverwrite=false;appMode=lastMainMode;}
            if(c=='n'||c=='N')patternConfirmOverwrite=false;
        }
        return;
    }

    if(up&&!prevPatternUpPressed)patternSelBank=(patternSelBank-1+NUM_PATTERN_BANKS)%NUM_PATTERN_BANKS;
    if(down&&!prevPatternDownPressed)patternSelBank=(patternSelBank+1)%NUM_PATTERN_BANKS;
    if(left&&!prevPatternLeftPressed)patternSelSlot=(patternSelSlot-1+NUM_PATTERNS_PER_BANK)%NUM_PATTERNS_PER_BANK;
    if(right&&!prevPatternRightPressed)patternSelSlot=(patternSelSlot+1)%NUM_PATTERNS_PER_BANK;
    prevPatternUpPressed=up;prevPatternDownPressed=down;
    prevPatternLeftPressed=left;prevPatternRightPressed=right;

    if(confirmKey&&!prevPatternConfirmPressed){
        if(patternBankMode==PatternBankMode::LOAD){
            if(patternSlotExists(patternSelBank,patternSelSlot)){
                loadPatternFromSlot(patternSelBank,patternSelSlot);
                appMode=lastMainMode;
            }
        } else {
            if(patternSlotExists(patternSelBank,patternSelSlot))patternConfirmOverwrite=true;
            else{savePatternToSlot(patternSelBank,patternSelSlot);appMode=lastMainMode;}
        }
    }
    prevPatternConfirmPressed=confirmKey;

    if(deleteKey&&!prevPatternDeleteKeyPressed){
        if(patternSlotExists(patternSelBank,patternSelSlot))patternConfirmDelete=true;
    }
    prevPatternDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevPatternTabPressed)appMode=AppMode::CATEGORY;
    prevPatternTabPressed=tabKey;
}

// ==========================================================
// Song mode: arranges saved Pattern Bank patterns into a sequence for
// playback (playback engine is a later step — this covers the data
// model, save/load, and the arrangement editor). Each entry references
// a Pattern Bank slot (bank+slot) plus a per-entry Transpose (semitones,
// chromatic — consistent with how Octave/Transpose already works
// elsewhere rather than a scale-aware diatonic shift) and Repeat count
// (how many times through before advancing). Two global settings:
// whether each entry uses its own saved pattern's Tempo/Swing
// (songInheritTempoSwing) or keeps whatever the Sequencer's current
// Tempo/Swing already is throughout the whole song, and whether the
// song loops back to the start after the last entry or stops there.
// ==========================================================
constexpr int SONG_MAX_ENTRIES=32;
struct SongEntry {
    uint8_t bank=0;       // 0-7 (A-H), which Pattern Bank slot this entry plays
    uint8_t slot=0;       // 0-7 (1-8)
    int8_t  transpose=0;  // semitones, chromatic, -24..+24
    uint8_t repeat=1;     // 1-16 times through the 16 steps before advancing
};
SongEntry songEntries[SONG_MAX_ENTRIES];
int  songLen=0;
bool songInheritTempoSwing=true; // true: each entry uses its own saved pattern's Tempo/Swing; false: keep the Sequencer's current Tempo/Swing throughout
bool songLoopAtEnd=true;         // true: loop back to entry 0 after the last one; false: stop there
float songTempoBpm=120.0f, songSwing=0.0f; // used instead of each pattern's own saved Tempo/Swing whenever songInheritTempoSwing is off

static const char *SONG_FOLDER_PATH="/CPS/Song";
constexpr int NUM_SONG_SLOTS=8; // 1-8, no lettered banks needed — fewer songs than patterns expected

bool ensureSongFolder(){return SD.exists(SONG_FOLDER_PATH)||SD.mkdir(SONG_FOLDER_PATH);}

String songFilePath(int slot){
    char buf[32];
    snprintf(buf,sizeof(buf),"%s/%d.json",SONG_FOLDER_PATH,slot+1);
    return String(buf);
}

bool songSlotExists(int slot){return SD.exists(songFilePath(slot));}

bool saveSongToSlot(int slot){
    if(!ensureSongFolder()){Serial.println("[Song] folder create failed");return false;}
    File f=SD.open(songFilePath(slot),FILE_WRITE);
    if(!f){Serial.println("[Song] open failed");return false;}
    f.println("{");
    f.printf("  \"song_len\": %d,\n",songLen);
    f.printf("  \"song_inherit\": %d,\n",(int)songInheritTempoSwing);
    f.printf("  \"song_loop\": %d,\n",(int)songLoopAtEnd);
    f.printf("  \"song_tempo\": %.1f,\n",songTempoBpm);
    f.printf("  \"song_swing\": %.1f,\n",songSwing);
    for(int i=0;i<songLen;i++){
        f.printf("  \"song%d_bank\": %d,\n",i,songEntries[i].bank);
        f.printf("  \"song%d_slot\": %d,\n",i,songEntries[i].slot);
        f.printf("  \"song%d_transpose\": %d,\n",i,songEntries[i].transpose);
        f.printf("  \"song%d_repeat\": %d%s\n",i,songEntries[i].repeat,(i==songLen-1)?"":",");
    }
    f.println("}");
    f.close();
    Serial.printf("[Song] saved to %s\n",songFilePath(slot).c_str());
    return true;
}

bool loadSongFromSlot(int slot){
    String path=songFilePath(slot);
    if(!SD.exists(path)){Serial.println("[Song] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    songLen=0;
    while(f.available()){
        String line=f.readStringUntil('\n');
        line.trim();
        if(line.length()<3)continue;
        int c=line.indexOf(':');
        if(c<0)continue;
        String key=line.substring(0,c); key.trim(); key.replace("\"","");
        String valStr=line.substring(c+1); valStr.trim();
        if(valStr.endsWith(","))valStr=valStr.substring(0,valStr.length()-1);
        float v=valStr.toFloat();
        if(key=="song_len"){songLen=constrain((int)v,0,SONG_MAX_ENTRIES);continue;}
        if(key=="song_inherit"){songInheritTempoSwing=(bool)(int)v;continue;}
        if(key=="song_loop"){songLoopAtEnd=(bool)(int)v;continue;}
        if(key=="song_tempo"){songTempoBpm=constrain(v,40.f,240.f);continue;}
        if(key=="song_swing"){songSwing=constrain(v,-100.f,100.f);continue;}
        if(key.startsWith("song")){
            int us=key.indexOf('_');
            if(us<4)continue;
            int idx=key.substring(4,us).toInt();
            String field=key.substring(us+1);
            if(idx<0||idx>=SONG_MAX_ENTRIES)continue;
            if(field=="bank")songEntries[idx].bank=(uint8_t)constrain((int)v,0,NUM_PATTERN_BANKS-1);
            else if(field=="slot")songEntries[idx].slot=(uint8_t)constrain((int)v,0,NUM_PATTERNS_PER_BANK-1);
            else if(field=="transpose")songEntries[idx].transpose=(int8_t)constrain((int)v,-24,24);
            else if(field=="repeat")songEntries[idx].repeat=(uint8_t)constrain((int)v,1,16);
        }
    }
    f.close();
    Serial.printf("[Song] loaded from %s\n",path.c_str());
    return true;
}

bool deleteSongSlot(int slot){return SD.remove(songFilePath(slot));}

// ---- Song editor UI ----
enum class SongField : uint8_t { BANK, SLOT, TRANSPOSE, REPEAT };
enum class SongFocus : uint8_t { ENTRY, SETTINGS };
SongFocus songFocus=SongFocus::ENTRY;
enum class SongSettingsField : uint8_t { TEMPO, SWING };
SongSettingsField songSettingsField=SongSettingsField::TEMPO;
bool prevSongFocusKeyPressed=false;
int  songCursorEntry=0;
SongField songField=SongField::BANK;
enum class SongIoMode : uint8_t { SAVE, LOAD };
bool songIoPickerOpen=false;
SongIoMode songIoMode=SongIoMode::SAVE;
int  songIoSelSlot=0;
bool songIoConfirmDelete=false, songIoConfirmOverwrite=false;
bool prevSongUpPressed=false, prevSongDownPressed=false, prevSongLeftPressed=false, prevSongRightPressed=false;
bool prevSongInsertKeyPressed=false, prevSongDeleteKeyPressed=false, prevSongFieldKeyPressed=false;
bool prevSongSaveKeyPressed=false, prevSongLoadKeyPressed=false, prevSongInheritKeyPressed=false, prevSongLoopKeyPressed=false;
bool prevSongIoConfirmPressed=false, prevSongIoDeleteKeyPressed=false, prevSongIoTabPressed=false;
bool prevSongIoUpPressed=false, prevSongIoDownPressed=false;
bool prevSongTabPressed=false;
bool prevSongPlayKeyPressed=false;

void updateSongIoPicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false;
    for(char c:s.word){if(c==';')up=true; if(c=='.')down=true;}
    bool confirmKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(songIoConfirmDelete){
        for(char c:s.word){
            if(c=='y'||c=='Y'){deleteSongSlot(songIoSelSlot);songIoConfirmDelete=false;}
            if(c=='n'||c=='N')songIoConfirmDelete=false;
        }
        return;
    }
    if(songIoConfirmOverwrite){
        for(char c:s.word){
            if(c=='y'||c=='Y'){saveSongToSlot(songIoSelSlot);songIoConfirmOverwrite=false;songIoPickerOpen=false;}
            if(c=='n'||c=='N')songIoConfirmOverwrite=false;
        }
        return;
    }

    if(up&&!prevSongIoUpPressed)songIoSelSlot=(songIoSelSlot-1+NUM_SONG_SLOTS)%NUM_SONG_SLOTS;
    if(down&&!prevSongIoDownPressed)songIoSelSlot=(songIoSelSlot+1)%NUM_SONG_SLOTS;
    prevSongIoUpPressed=up;prevSongIoDownPressed=down;

    if(confirmKey&&!prevSongIoConfirmPressed){
        if(songIoMode==SongIoMode::LOAD){
            if(songSlotExists(songIoSelSlot)){loadSongFromSlot(songIoSelSlot);songIoPickerOpen=false;}
        } else {
            if(songSlotExists(songIoSelSlot))songIoConfirmOverwrite=true;
            else{saveSongToSlot(songIoSelSlot);songIoPickerOpen=false;}
        }
    }
    prevSongIoConfirmPressed=confirmKey;

    if(deleteKey&&!prevSongIoDeleteKeyPressed){
        if(songSlotExists(songIoSelSlot))songIoConfirmDelete=true;
    }
    prevSongIoDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevSongIoTabPressed)songIoPickerOpen=false;
    prevSongIoTabPressed=tabKey;
}

// ---- Song playback engine ----
// Reuses SEQ's own step-timing engine (updateSeqTiming(), seqPlaying)
// entirely — Song just loads each entry's pattern into seqSteps[] in
// turn, applies that entry's Transpose as a multiplier songTransposeMult
// (read by updateSeqTiming()), and advances entries via
// songAdvanceOnPassComplete() (called from updateSeqTiming() whenever a
// full 16-step pass completes). Deliberately simple for a first pass —
// each entry transition does a synchronous SD card read via
// loadPatternFromSlot(), no pre-fetch/double-buffering — flagged as a
// possible source of a small timing hiccup right at entry boundaries;
// worth listening for specifically there once this is on hardware.
int   songPlayEntry=0;
int   songPlayRepeatsDone=0;

void songLoadEntry(int idx){
    if(idx<0||idx>=songLen){songPlaying=false;seqPlaying=false;return;}
    SongEntry &e=songEntries[idx];
    loadPatternFromSlot(e.bank,e.slot); // also sets seqTempoBpm/seqSwing from the pattern file
    if(!songInheritTempoSwing){seqTempoBpm=songTempoBpm;seqSwing=songSwing;}
    songTransposeMult=powf(2.f,(float)e.transpose/12.f);
    songPlayEntry=idx;
    songPlayRepeatsDone=0;
    seqPlayStep=0;
    seqLastStepMs=millis();
}

void songAdvanceOnPassComplete(){
    songPlayRepeatsDone++;
    if(songPlayRepeatsDone<songEntries[songPlayEntry].repeat)return; // keep repeating the current entry
    int next=songPlayEntry+1;
    if(next>=songLen){
        if(songLoopAtEnd){next=0;}
        else{
            songPlaying=false;
            seqPlaying=false;
            currentFreq=0.f;
            return;
        }
    }
    songLoadEntry(next);
}

void songTogglePlay(){
    if(songLen==0)return;
    if(songPlaying){
        songPlaying=false;
        seqPlaying=false;
        currentFreq=0.f;
        seqSliding=false;
        seqAccentCutoffBoostTarget=0.f;
        seqAccentResoBoostTarget=0.f;
        seqVelocityMult=1.0f;
    } else {
        songPlaying=true;
        seqPlaying=true;
        songLoadEntry(0);
    }
}

void updateSongEditor(){
    if(songIoPickerOpen){updateSongIoPicker();return;}
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false,fieldKey=false,focusKey=false;
    bool saveKey=false,loadKey=false,inheritKey=false,loopKey=false,hKey=false,playKey=false;
    for(char c:s.word){
        if(c==',')up=true;   if(c=='/')down=true;
        if(c==';')left=true; if(c=='.')right=true;
        if(c=='g')fieldKey=true;
        if(c=='f')focusKey=true;
        if(s.shift&&(c=='s'||c=='S'))saveKey=true;
        if(s.shift&&(c=='l'||c=='L'))loadKey=true;
        if(c=='i'||c=='I')inheritKey=true;
        if(c=='o'||c=='O')loopKey=true;
        if(c=='h'||c=='H')hKey=true;
        if(c==' ')playKey=true;
    }
    helpVisible=hKey; // hold to show, same as PLAY/SEQ
    bool insertKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(playKey&&!prevSongPlayKeyPressed)songTogglePlay();
    prevSongPlayKeyPressed=playKey;

    if(saveKey&&!prevSongSaveKeyPressed){songIoMode=SongIoMode::SAVE;songIoPickerOpen=true;}
    prevSongSaveKeyPressed=saveKey;
    if(loadKey&&!prevSongLoadKeyPressed){songIoMode=SongIoMode::LOAD;songIoPickerOpen=true;}
    prevSongLoadKeyPressed=loadKey;
    if(inheritKey&&!prevSongInheritKeyPressed)songInheritTempoSwing=!songInheritTempoSwing;
    prevSongInheritKeyPressed=inheritKey;
    if(loopKey&&!prevSongLoopKeyPressed)songLoopAtEnd=!songLoopAtEnd;
    prevSongLoopKeyPressed=loopKey;

    if(up&&!prevSongUpPressed&&songLen>0)songCursorEntry=(songCursorEntry-1+songLen)%songLen;
    if(down&&!prevSongDownPressed&&songLen>0)songCursorEntry=(songCursorEntry+1)%songLen;
    prevSongUpPressed=up;prevSongDownPressed=down;

    if(focusKey&&!prevSongFocusKeyPressed)songFocus=(songFocus==SongFocus::ENTRY)?SongFocus::SETTINGS:SongFocus::ENTRY;
    prevSongFocusKeyPressed=focusKey;

    if(fieldKey&&!prevSongFieldKeyPressed){
        if(songFocus==SongFocus::ENTRY)songField=(SongField)(((uint8_t)songField+1)%4);
        else                           songSettingsField=(songSettingsField==SongSettingsField::TEMPO)?SongSettingsField::SWING:SongSettingsField::TEMPO;
    }
    prevSongFieldKeyPressed=fieldKey;

    if(songFocus==SongFocus::SETTINGS){
        bool edgeInc=right&&!prevSongRightPressed, edgeDec=left&&!prevSongLeftPressed;
        if(songSettingsField==SongSettingsField::TEMPO){
            if(edgeInc)songTempoBpm=min(240.f,songTempoBpm+5.f);
            if(edgeDec)songTempoBpm=max(40.f,songTempoBpm-5.f);
        } else {
            if(edgeInc)songSwing=min(100.f,songSwing+5.f);
            if(edgeDec)songSwing=max(-100.f,songSwing-5.f);
        }
    } else if(songLen>0&&(left||right)){
        SongEntry &e=songEntries[songCursorEntry];
        bool inc=right,dec=left;
        bool edgeInc=inc&&!prevSongRightPressed, edgeDec=dec&&!prevSongLeftPressed;
        switch(songField){
            case SongField::BANK:
                if(edgeInc)e.bank=(e.bank+1)%NUM_PATTERN_BANKS;
                if(edgeDec)e.bank=(e.bank-1+NUM_PATTERN_BANKS)%NUM_PATTERN_BANKS;
                break;
            case SongField::SLOT:
                if(edgeInc)e.slot=(e.slot+1)%NUM_PATTERNS_PER_BANK;
                if(edgeDec)e.slot=(e.slot-1+NUM_PATTERNS_PER_BANK)%NUM_PATTERNS_PER_BANK;
                break;
            case SongField::TRANSPOSE:
                if(edgeInc)e.transpose=(int8_t)min(24,(int)e.transpose+1);
                if(edgeDec)e.transpose=(int8_t)max(-24,(int)e.transpose-1);
                break;
            case SongField::REPEAT:
                if(edgeInc)e.repeat=(uint8_t)min(16,(int)e.repeat+1);
                if(edgeDec)e.repeat=(uint8_t)max(1,(int)e.repeat-1);
                break;
        }
    }
    prevSongLeftPressed=left;prevSongRightPressed=right;

    // Enter inserts a new entry right after the cursor (or as the first
    // entry if the song is empty), inheriting the Bank/Slot the cursor
    // was already on (so building up a song from similar patterns is
    // quick) but resetting Transpose/Repeat to their defaults, since
    // those are specific to whichever entry they came from and carrying
    // them forward was more surprising than helpful.
    if(insertKey&&!prevSongInsertKeyPressed&&songLen<SONG_MAX_ENTRIES){
        int insertAt=(songLen>0)?songCursorEntry+1:0;
        SongEntry newEntry;
        if(songLen>0){
            newEntry.bank=songEntries[songCursorEntry].bank;
            newEntry.slot=songEntries[songCursorEntry].slot;
        }
        for(int i=songLen;i>insertAt;i--)songEntries[i]=songEntries[i-1];
        songEntries[insertAt]=newEntry;
        songLen++;
        songCursorEntry=insertAt;
    }
    prevSongInsertKeyPressed=insertKey;

    if(deleteKey&&!prevSongDeleteKeyPressed&&songLen>0){
        for(int i=songCursorEntry;i<songLen-1;i++)songEntries[i]=songEntries[i+1];
        songLen--;
        if(songCursorEntry>=songLen)songCursorEntry=max(0,songLen-1);
    }
    prevSongDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevSongTabPressed)appMode=lastMainMode;
    prevSongTabPressed=tabKey;
}

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
                appMode=AppMode::CATEGORY;
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

    if(cancel&&!prevPatchTabPressed)appMode=AppMode::CATEGORY;
    prevPatchTabPressed=cancel;
}

// ==========================================================
// SETTING menu items
// ==========================================================
const char *imuXLabel(){return imuTargetName(imuAxisX.target);}
const char *imuYLabel(){return imuTargetName(imuAxisY.target);}

// ---------------------------------------------------------
// IMU target picker (scrollable list w/ category dividers)
// ---------------------------------------------------------
// Display order for the picker, grouped by category. This is independent
// from the ImuTarget enum's numeric order (which must stay stable for
// backward-compatible save files) — it's purely a visual arrangement.
struct ImuPickerRow { ImuTarget target; bool divider; }; // divider = draw a line above this row
const ImuPickerRow IMU_PICKER_ORDER[]={
    {ImuTarget::NONE,          false},
    {ImuTarget::VIBRATO_DEPTH, true },
    {ImuTarget::VIBRATO_RATE,  false},
    {ImuTarget::PITCH_BEND,    false},
    {ImuTarget::BEND_UP,       false},
    {ImuTarget::BEND_DOWN,     false},
    {ImuTarget::DETUNE,        false},
    {ImuTarget::VOLUME,        true },
    {ImuTarget::TREMOLO,       false},
    {ImuTarget::TIMBRE,        true },
    {ImuTarget::PWM,           false},
    {ImuTarget::NOISE,         false},
    {ImuTarget::SUB_LEVEL,     false},
    {ImuTarget::FILTER_CUTOFF, true },
    {ImuTarget::RESONANCE,     false},
    {ImuTarget::LFO_RATE,      true },
    {ImuTarget::LFO_DEPTH,     false},
    {ImuTarget::BITCRUSH,      true },
    {ImuTarget::ARP_TEMPO,     true },
    {ImuTarget::ARP_SWING,   false},
};
constexpr int IMU_PICKER_COUNT=sizeof(IMU_PICKER_ORDER)/sizeof(IMU_PICKER_ORDER[0]);

bool imuPickerOpen=false;
int  imuPickerAxis=0;   // 0 = X, 1 = Y
int  imuPickerIndex=0;  // row index into IMU_PICKER_ORDER

bool prevImuPickerUpPressed=false, prevImuPickerDownPressed=false;
bool prevImuPickerConfirmPressed=false, prevImuPickerTabPressed=false;

int imuPickerRowForTarget(ImuTarget t){
    for(int i=0;i<IMU_PICKER_COUNT;i++)if(IMU_PICKER_ORDER[i].target==t)return i;
    return 0;
}

void openImuPicker(int axis){
    imuPickerAxis=axis;
    imuPickerIndex=imuPickerRowForTarget(axis==0?imuAxisX.target:imuAxisY.target);
    // Seed edge-trackers with whatever is CURRENTLY held, so the same '/'
    // keypress that opened this picker isn't immediately re-read as a
    // fresh confirm (same fix as the Patch Bank carry-over bug).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter;
    for(char c:s.word){if(c==';')heldUp=true;if(c=='.')heldDown=true;if(c=='/')heldConfirm=true;}
    prevImuPickerUpPressed=heldUp;prevImuPickerDownPressed=heldDown;
    prevImuPickerConfirmPressed=heldConfirm;prevImuPickerTabPressed=s.tab;
    imuPickerOpen=true;
}
void imuXOpenPicker(){openImuPicker(0);}
void imuYOpenPicker(){openImuPicker(1);}

void updateImuPicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool mU=false,mD=false,confirm=s.enter,cancel=s.tab;
    for(char c:s.word){if(c==';')mU=true;if(c=='.')mD=true;if(c=='/')confirm=true;}

    if(mU&&!prevImuPickerUpPressed)  imuPickerIndex=(imuPickerIndex-1+IMU_PICKER_COUNT)%IMU_PICKER_COUNT;
    if(mD&&!prevImuPickerDownPressed)imuPickerIndex=(imuPickerIndex+1)%IMU_PICKER_COUNT;
    prevImuPickerUpPressed=mU;prevImuPickerDownPressed=mD;

    if(confirm&&!prevImuPickerConfirmPressed){
        ImuTarget newTarget=IMU_PICKER_ORDER[imuPickerIndex].target;
        if(imuPickerAxis==0){resetParamToDefault(imuAxisX.target);imuAxisX.target=newTarget;}
        else                {resetParamToDefault(imuAxisY.target);imuAxisY.target=newTarget;}
        imuPickerOpen=false;
    }
    prevImuPickerConfirmPressed=confirm;

    if(cancel&&!prevImuPickerTabPressed)imuPickerOpen=false;
    prevImuPickerTabPressed=cancel;
}

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

// ---------------------------------------------------------
// IMU per-axis fine controls (v0.8 Phase 3)
// ---------------------------------------------------------
void imuXSensInc(){imuAxisX.sensitivity=min(imuAxisX.sensitivity+0.1f,3.0f);}
void imuXSensDec(){imuAxisX.sensitivity=max(imuAxisX.sensitivity-0.1f,0.3f);}
char imuXSensBuf[8];const char *imuXSensLabel(){snprintf(imuXSensBuf,sizeof(imuXSensBuf),"%.1fx",imuAxisX.sensitivity);return imuXSensBuf;}
void imuXInvertToggle(){imuAxisX.invert=!imuAxisX.invert;}
const char *imuXInvertLabel(){return imuAxisX.invert?"Inverted":"Normal";}
void imuXCurveToggle(){imuAxisX.exponential=!imuAxisX.exponential;}
const char *imuXCurveLabel(){return imuAxisX.exponential?"Expo":"Linear";}
void imuXDeadzoneInc(){imuAxisX.deadzone=min(imuAxisX.deadzone+0.05f,0.3f);}
void imuXDeadzoneDec(){imuAxisX.deadzone=max(imuAxisX.deadzone-0.05f,0.f);}
char imuXDzBuf[8];const char *imuXDzLabel(){snprintf(imuXDzBuf,sizeof(imuXDzBuf),"%.0f%%",imuAxisX.deadzone*100);return imuXDzBuf;}

void imuYSensInc(){imuAxisY.sensitivity=min(imuAxisY.sensitivity+0.1f,3.0f);}
void imuYSensDec(){imuAxisY.sensitivity=max(imuAxisY.sensitivity-0.1f,0.3f);}
char imuYSensBuf[8];const char *imuYSensLabel(){snprintf(imuYSensBuf,sizeof(imuYSensBuf),"%.1fx",imuAxisY.sensitivity);return imuYSensBuf;}
void imuYInvertToggle(){imuAxisY.invert=!imuAxisY.invert;}
const char *imuYInvertLabel(){return imuAxisY.invert?"Inverted":"Normal";}
void imuYCurveToggle(){imuAxisY.exponential=!imuAxisY.exponential;}
const char *imuYCurveLabel(){return imuAxisY.exponential?"Expo":"Linear";}
void imuYDeadzoneInc(){imuAxisY.deadzone=min(imuAxisY.deadzone+0.05f,0.3f);}
void imuYDeadzoneDec(){imuAxisY.deadzone=max(imuAxisY.deadzone-0.05f,0.f);}
char imuYDzBuf[8];const char *imuYDzLabel(){snprintf(imuYDzBuf,sizeof(imuYDzBuf),"%.0f%%",imuAxisY.deadzone*100);return imuYDzBuf;}

// Calibrate: zeroes both axes to whatever tilt the device currently has.
// Gated behind a confirmation overlay (same pattern as Patch Bank's delete).
bool imuCalibrateConfirmOpen=false;
bool prevImuCalConfirmPressed=false, prevImuCalTabPressed=false;

void openCalibrateConfirm(){
    imuCalibrateConfirmOpen=true;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldConfirm=s.enter;
    for(char c:s.word)if(c=='/')heldConfirm=true;
    prevImuCalConfirmPressed=heldConfirm;
    prevImuCalTabPressed=s.tab;
}
const char *calibrateEnterLabel(){return "Select>";}

void calibrateToggle(){
    if(imuCalibrated){
        // Turn OFF: immediately reset, no confirmation needed (non-destructive,
        // just returns both axes to their raw zero point)
        imuAxisX.calOffsetDeg=0.f;
        imuAxisY.calOffsetDeg=0.f;
        imuCalibrated=false;
    } else {
        // Turn ON: open the confirmation dialog; calibration is actually
        // applied when the user confirms (see updateImuCalibrateConfirm)
        openCalibrateConfirm();
    }
}
const char *calibrateOnOffLabel(){return imuCalibrated?"ON":"OFF";}

void updateImuCalibrateConfirm(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool confirm=s.enter,cancel=s.tab;
    for(char c:s.word)if(c=='/')confirm=true;
    if(confirm&&!prevImuCalConfirmPressed){
        imuAxisX.calOffsetDeg=lastAngleXDeg;
        imuAxisY.calOffsetDeg=lastAngleYDeg;
        imuCalibrated=true;
        imuCalibrateConfirmOpen=false;
    }
    prevImuCalConfirmPressed=confirm;
    if(cancel&&!prevImuCalTabPressed)imuCalibrateConfirmOpen=false;
    prevImuCalTabPressed=cancel;
}

// ---------------------------------------------------------
// Category sub-menus (Patch / IMU / Bend / Portamento)
// ---------------------------------------------------------
// SETTING is now just an entry point into these four dedicated screens,
// each following the same list-of-SettingItem pattern used elsewhere.
enum class SettingsCategory : uint8_t { PATCH, IMU, BEND, PORTAMENTO, PLAYMODE, ARP, PATTERN };
SettingsCategory currentCategory = SettingsCategory::PATCH;
int selectedCategoryIndex = 0;

// ---------------------------------------------------------
// Reset-to-default (Phase 4)
// ---------------------------------------------------------
// Shared confirmation overlay for Patch(tone)/Bend/Portamento resets —
// same pattern as the Patch Bank delete-confirm and IMU Calibrate-confirm.
enum class ResetKind : uint8_t { PATCH_TONE, BEND, PORTAMENTO, PATCH_RANDOM, PATTERN_RANDOM };
bool resetConfirmOpen=false;
ResetKind resetConfirmKind=ResetKind::PATCH_TONE;
bool prevResetConfirmPressed=false, prevResetTabPressed=false;

void openResetConfirm(ResetKind kind){
    resetConfirmKind=kind;
    resetConfirmOpen=true;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldConfirm=s.enter;
    for(char c:s.word)if(c=='/')heldConfirm=true;
    prevResetConfirmPressed=heldConfirm;
    prevResetTabPressed=s.tab;
}
void openPatchToneReset(){openResetConfirm(ResetKind::PATCH_TONE);}
void openPatchRandomize(){openResetConfirm(ResetKind::PATCH_RANDOM);}
void openPatternRandomize(){openResetConfirm(ResetKind::PATTERN_RANDOM);}
void openBendReset(){openResetConfirm(ResetKind::BEND);}
void openPortaReset(){openResetConfirm(ResetKind::PORTAMENTO);}
const char *resetEnterLabel(){return "Select>";}

float randRange(float lo,float hi){
    return lo+(hi-lo)*(float)random(0,10001)/10000.0f;
}

// Resets everything tone-related (VCO/VCF/VCA/LFO/IMU + note hold) to a
// simple, predictable starting point. Performance-time state (Bend,
// Portamento, octave, transpose) is deliberately NOT touched here — those
// get their own separate resets in their own category screens.
void performPatchToneReset(){
    // VCO
    params.timbreMorph=params.timbreMorphTarget=0.f;
    params.pwmWidth=0.5f; params.pwmOffset=0.f; params.pwmOffsetTarget=0.f;
    params.detuneCents=0.f; params.detuneOffset=0.f; params.detuneOffsetTarget=0.f;
    params.fineTuneCents=0.f;
    params.subOscLevel=0.f; params.subLevelOffset=0.f; params.subLevelOffsetTarget=0.f;
    params.subOscOctave=-1;
    params.noiseLevel=0.f; params.noiseOffset=0.f; params.noiseOffsetTarget=0.f;
    // VCF
    filterParams.type=FilterType::NONE;
    filterParams.cutoffHz=2000.0f;
    filterParams.resonanceQ=0.707f;
    filterParams.keyTracking=0.0f;
    params.resonanceOffset=0.f; params.resonanceOffsetTarget=0.f;
    filterEnv.depth=0.f; filterEnv.attackTime=0.1f; filterEnv.decayTime=0.3f;
    filterEnv.sustainLvl=0.0f; filterEnv.releaseTime=0.3f;
    // VCA (ADSR) — simplest usable starting point: full sustain, quick release
    adsr.attackTime=0.f; adsr.decayTime=0.f; adsr.sustainLevel=1.0f; adsr.releaseTime=0.2f;
    // LFO
    lfo.target=LfoTarget::NONE; lfo.wave=LfoWave::SINE; lfo.rateHz=2.0f; lfo.depth=0.f;
    lfoRateOffset=0.f; lfoRateOffsetTarget=0.f; lfoDepthOffset=0.f; lfoDepthOffsetTarget=0.f;
    // IMU
    imuAxisX.target=ImuTarget::TIMBRE;
    imuAxisY.target=ImuTarget::VOLUME;
    imuAxisX.sensitivity=1.0f; imuAxisY.sensitivity=1.0f;
    imuAxisX.invert=false; imuAxisY.invert=false;
    imuAxisX.exponential=false; imuAxisY.exponential=false;
    imuAxisX.deadzone=0.f; imuAxisY.deadzone=0.f;
    imuAxisX.calOffsetDeg=0.f; imuAxisY.calOffsetDeg=0.f;
    imuCalibrated=false;
    imuXHeld=false; imuYHeld=false;
    params.volumeScale=1.0f; params.volumeScaleTarget=1.0f;
    // Note hold
    noteHeld=false; heldFreq=0.f;
    updateFilterCoefficients();
}

// Randomizes every tone-related parameter (same scope as the tone reset,
// including "type" parameters like filter type / LFO target+wave / IMU
// targets, per user's request). Calibration is left alone — it's a
// physical setup thing, not a creative/tone parameter.
void performPatchRandomize(){
    // VCO
    params.timbreMorph=params.timbreMorphTarget=randRange(0.f,3.f);
    params.pwmWidth=randRange(0.1f,0.9f); params.pwmOffset=0.f; params.pwmOffsetTarget=0.f;
    params.detuneCents=randRange(-50.f,50.f); params.detuneOffset=0.f; params.detuneOffsetTarget=0.f;
    params.fineTuneCents=randRange(-100.f,100.f);
    params.subOscLevel=randRange(0.f,1.f); params.subLevelOffset=0.f; params.subLevelOffsetTarget=0.f;
    params.subOscOctave=(random(0,2)==0)?-1:-2;
    // Noise tends to mask pitch clarity noticeably even at fairly low
    // levels, so it's much rarer and much subtler here than the other
    // randomized parameters — most random patches should have none at all.
    params.noiseLevel=(random(0,100)<15)?randRange(0.05f,0.2f):0.f;
    params.noiseOffset=0.f; params.noiseOffsetTarget=0.f;
    // VCF
    filterParams.type=(FilterType)random(0,5); // LPF/HPF/BPF/NOTCH/NONE
    filterParams.cutoffHz=randRange(FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX);
    filterParams.resonanceQ=randRange(FILTER_Q_MIN,FILTER_Q_MAX);
    filterParams.keyTracking=randRange(0.f,1.f);
    params.resonanceOffset=0.f; params.resonanceOffsetTarget=0.f;
    filterEnv.depth=randRange(-3900.f,3900.f);
    filterEnv.attackTime=randRange(0.f,1.5f);
    filterEnv.decayTime=randRange(0.f,1.5f);
    filterEnv.sustainLvl=randRange(0.f,1.f);
    filterEnv.releaseTime=randRange(0.f,1.5f);
    // VCA (kept within a musically reasonable range, not the full 0-5s max)
    adsr.attackTime=randRange(0.f,1.5f);
    adsr.decayTime=randRange(0.f,1.5f);
    adsr.sustainLevel=randRange(0.f,1.f);
    adsr.releaseTime=randRange(0.f,1.5f);
    // LFO
    lfo.target=(LfoTarget)random(0,(int)LfoTarget::TARGET_COUNT);
    lfo.wave=(LfoWave)random(0,4);
    lfo.rateHz=randRange(LFO_RATE_MIN,LFO_RATE_MAX);
    lfo.depth=randRange(0.f,1.f);
    lfoRateOffset=0.f; lfoRateOffsetTarget=0.f; lfoDepthOffset=0.f; lfoDepthOffsetTarget=0.f;
    // IMU
    imuAxisX.target=(ImuTarget)random(0,(int)ImuTarget::TARGET_COUNT);
    imuAxisY.target=(ImuTarget)random(0,(int)ImuTarget::TARGET_COUNT);
    imuAxisX.sensitivity=randRange(0.3f,3.0f);
    imuAxisY.sensitivity=randRange(0.3f,3.0f);
    imuAxisX.invert=(random(0,2)==1);
    imuAxisY.invert=(random(0,2)==1);
    imuAxisX.exponential=(random(0,2)==1);
    imuAxisY.exponential=(random(0,2)==1);
    imuAxisX.deadzone=randRange(0.f,0.3f);
    imuAxisY.deadzone=randRange(0.f,0.3f);
    imuXHeld=false; imuYHeld=false;
    params.volumeScale=1.0f; params.volumeScaleTarget=1.0f;
    // Note hold
    noteHeld=false; heldFreq=0.f;
    updateFilterCoefficients();
}

// Generates a random 16-step pattern using the CURRENT scale's notes (so
// it's always in-key, EZ or Pro), with Tie/Accent/Slide included so the
// result actually shows off the TB-303-style features, not just pitches.
// Tempo/Swing are left untouched — those are pattern-level, not part of
// what "randomize the steps" implies.
void performPatternRandomize(){
    bool inNote=false;
    for(int i=0;i<SEQ_NUM_STEPS;i++){
        seqSteps[i]=SeqStep();
        int roll=random(0,100);
        if(inNote&&roll<30){
            // Continue the previous note via Tie.
            seqSteps[i].tie=true;
        } else if(roll<(inNote?80:65)){
            // New note, picked from the current scale's key tables (25
            // notes total across both rows) so it's always in-key.
            int idx=random(0,12+13);
            float freq=(idx<12)?row1Freqs[idx]:row2Freqs[idx-12];
            seqSteps[i].freq=freq;
            seqSteps[i].velocity=(uint8_t)random(40,101);
            seqSteps[i].accent=(random(0,100)<18);
            seqSteps[i].slide=(random(0,100)<15);
            inNote=true;
        } else {
            // Rest.
            inNote=false;
        }
    }
    seqCursorStep=0;
}

void performBendReset(){
    keyBendMaxCents=200.0f;
    keyBendAttackSmooth=KEY_BEND_ATTACK_SMOOTH_DEFAULT;
    keyBendReleaseSmooth=KEY_BEND_RELEASE_SMOOTH_DEFAULT;
}

void performPortaReset(){
    portaEnabled=false;
    portaSpeed=0.005f;
    portaFreq=0.f;
}

void updateResetConfirm(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool confirm=s.enter,cancel=s.tab;
    for(char c:s.word)if(c=='/')confirm=true;
    if(confirm&&!prevResetConfirmPressed){
        switch(resetConfirmKind){
            case ResetKind::PATCH_TONE:   performPatchToneReset(); break;
            case ResetKind::PATCH_RANDOM: performPatchRandomize(); break;
            case ResetKind::PATTERN_RANDOM: performPatternRandomize(); break;
            case ResetKind::BEND:         performBendReset(); break;
            case ResetKind::PORTAMENTO:   performPortaReset(); break;
        }
        resetConfirmOpen=false;
    }
    prevResetConfirmPressed=confirm;
    if(cancel&&!prevResetTabPressed)resetConfirmOpen=false;
    prevResetTabPressed=cancel;
}

SettingItem patchMenuItems[]={
    {"Save", patchSaveEnter, patchSaveEnter, patchEnterLabel},
    {"Load", patchLoadEnter, patchLoadEnter, patchEnterLabel},
    {"Reset", openPatchToneReset, openPatchToneReset, resetEnterLabel},
    {"Random", openPatchRandomize, openPatchRandomize, resetEnterLabel},
};
// 2-column layout (splitCol=5 in getCategoryItems): left = X's 5 items,
// right = Y's 5 items + Calibrate as a 6th row. (ADV only — see
// imuMenuItemsOriginal for the reduced original-Cardputer "PAD" version.)
SettingItem imuMenuItemsAdv[]={
    {"IMU X",   imuXOpenPicker,    imuXOpenPicker,    imuXLabel},
    {"X Sens",  imuXSensInc,       imuXSensDec,       imuXSensLabel},
    {"X Invert",imuXInvertToggle,  imuXInvertToggle,  imuXInvertLabel},
    {"X Curve", imuXCurveToggle,   imuXCurveToggle,   imuXCurveLabel},
    {"X Dead",  imuXDeadzoneInc,   imuXDeadzoneDec,   imuXDzLabel},
    {"IMU Y",   imuYOpenPicker,    imuYOpenPicker,    imuYLabel},
    {"Y Sens",  imuYSensInc,       imuYSensDec,       imuYSensLabel},
    {"Y Invert",imuYInvertToggle,  imuYInvertToggle,  imuYInvertLabel},
    {"Y Curve", imuYCurveToggle,   imuYCurveToggle,   imuYCurveLabel},
    {"Y Dead",  imuYDeadzoneInc,   imuYDeadzoneDec,   imuYDzLabel},
    {"Calibrate",calibrateToggle,calibrateToggle,calibrateOnOffLabel},
};
// Original Cardputer: no Deadzone (nothing to filter out — it's a clean
// key-driven signal, not a noisy sensor) and no Calibrate (no physical
// zero-point to correct on a virtual axis). 2-column, splitCol=4.
SettingItem imuMenuItemsOriginal[]={
    {"PAD X",   imuXOpenPicker,    imuXOpenPicker,    imuXLabel},
    {"X Sens",  imuXSensInc,       imuXSensDec,       imuXSensLabel},
    {"X Invert",imuXInvertToggle,  imuXInvertToggle,  imuXInvertLabel},
    {"X Curve", imuXCurveToggle,   imuXCurveToggle,   imuXCurveLabel},
    {"PAD Y",   imuYOpenPicker,    imuYOpenPicker,    imuYLabel},
    {"Y Sens",  imuYSensInc,       imuYSensDec,       imuYSensLabel},
    {"Y Invert",imuYInvertToggle,  imuYInvertToggle,  imuYInvertLabel},
    {"Y Curve", imuYCurveToggle,   imuYCurveToggle,   imuYCurveLabel},
};
SettingItem bendMenuItems[]={
    {"Bend wid", bendWInc, bendWDec, bendWLabel},
    {"Bend atk", bendAInc, bendADec, bendALabel},
    {"Bend rel", bendRInc, bendRDec, bendRLabel},
    {"Reset",    openBendReset, openBendReset, resetEnterLabel},
};
SettingItem portaMenuItems[]={
    {"Portamento", portaToggle,  portaToggle,  portaOnOffLabel},
    {"Porta spd",  portaSpdInc,  portaSpdDec,  portaSpdLabel},
    {"Reset",      openPortaReset, openPortaReset, resetEnterLabel},
};

// Returns the item array/count/title for the currently-open category screen.
void playModeToggle(){playMode=(playMode==PlayMode::EZ)?PlayMode::PRO:PlayMode::EZ;recomputeKeyNotes();}
const char *playModeLabel(){return playMode==PlayMode::EZ?"EZ":"Pro";}

// ---------------------------------------------------------
// Scale picker (Play Mode > Scale) — a genuine 2-level menu: pick a
// category first, then a scale within it. Unlike the IMU target picker's
// single flat list, this uses two full steps since the scale count is
// large enough that even category-grouped flat scrolling would be a lot
// to page through. Selecting a scale takes effect IMMEDIATELY (live
// preview) so the user can hold a note key while scrolling to hear it;
// Tab from the category list fully cancels back to whatever scale was
// active before the picker was opened.
bool scalePickerOpen=false;
int  scalePickerLevel=0;            // 0 = category list, 1 = scale list within a category
int  scalePickerCategoryIndex=0;
int  scalePickerRowIndex=0;
int  scalePickerOriginalScaleIndex=0;

bool prevScalePickerUpPressed=false, prevScalePickerDownPressed=false;
bool prevScalePickerConfirmPressed=false, prevScalePickerTabPressed=false;

int getScalesInCategory(int cat,int *outIndices,int maxOut){
    int n=0;
    for(int i=0;i<NUM_SCALES&&n<maxOut;i++)if(SCALES[i].category==cat)outIndices[n++]=i;
    return n;
}

void openScalePicker(){
    scalePickerOpen=true;
    scalePickerLevel=0;
    scalePickerRowIndex=0;
    scalePickerCategoryIndex=0;
    scalePickerOriginalScaleIndex=currentScaleIndex;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter;
    for(char c:s.word){if(c==';')heldUp=true;if(c=='.')heldDown=true;if(c=='/')heldConfirm=true;}
    prevScalePickerUpPressed=heldUp;prevScalePickerDownPressed=heldDown;
    prevScalePickerConfirmPressed=heldConfirm;prevScalePickerTabPressed=s.tab;
}
const char *scalePickerEnterLabel(){return "Select>";}
const char *currentScaleLabel(){return SCALES[currentScaleIndex].name;}

void updateScalePicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool mU=false,mD=false,confirm=s.enter,cancel=s.tab;
    for(char c:s.word){if(c==';')mU=true;if(c=='.')mD=true;if(c=='/')confirm=true;}

    if(scalePickerLevel==0){
        if(mU&&!prevScalePickerUpPressed)  scalePickerRowIndex=(scalePickerRowIndex-1+NUM_SCALE_CATEGORIES)%NUM_SCALE_CATEGORIES;
        if(mD&&!prevScalePickerDownPressed)scalePickerRowIndex=(scalePickerRowIndex+1)%NUM_SCALE_CATEGORIES;
        prevScalePickerUpPressed=mU;prevScalePickerDownPressed=mD;
        if(confirm&&!prevScalePickerConfirmPressed){
            scalePickerCategoryIndex=scalePickerRowIndex;
            int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
            scalePickerRowIndex=0;
            for(int i=0;i<n;i++)if(indices[i]==currentScaleIndex){scalePickerRowIndex=i;break;}
            // Actually apply the highlighted scale now (not just move the
            // cursor) — otherwise confirming immediately without scrolling
            // first (the only option for a single-item category, e.g.
            // Chromatic) would close the picker without ever having
            // switched currentScaleIndex, silently keeping the old scale.
            currentScaleIndex=indices[scalePickerRowIndex];
            recomputeKeyNotes();
            scalePickerLevel=1;
        }
        prevScalePickerConfirmPressed=confirm;
        if(cancel&&!prevScalePickerTabPressed){
            currentScaleIndex=scalePickerOriginalScaleIndex;
            recomputeKeyNotes();
            scalePickerOpen=false;
        }
        prevScalePickerTabPressed=cancel;
    } else {
        int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
        if(mU&&!prevScalePickerUpPressed){
            scalePickerRowIndex=(scalePickerRowIndex-1+n)%n;
            currentScaleIndex=indices[scalePickerRowIndex];recomputeKeyNotes();
        }
        if(mD&&!prevScalePickerDownPressed){
            scalePickerRowIndex=(scalePickerRowIndex+1)%n;
            currentScaleIndex=indices[scalePickerRowIndex];recomputeKeyNotes();
        }
        prevScalePickerUpPressed=mU;prevScalePickerDownPressed=mD;
        if(confirm&&!prevScalePickerConfirmPressed){
            scalePickerOpen=false; // keep currentScaleIndex — already live-previewed
        }
        prevScalePickerConfirmPressed=confirm;
        if(cancel&&!prevScalePickerTabPressed){
            scalePickerLevel=0; // one level back, keeping the current preview
            scalePickerRowIndex=scalePickerCategoryIndex;
        }
        prevScalePickerTabPressed=cancel;
    }
}

SettingItem playModeMenuItemsEZ[]={
    {"Mode",  playModeToggle, playModeToggle, playModeLabel},
};
SettingItem playModeMenuItemsPro[]={
    {"Mode",  playModeToggle, playModeToggle, playModeLabel},
    {"Scale", openScalePicker,openScalePicker,currentScaleLabel},
};

void arpToggle(){
    arpEnabled=!arpEnabled;
    if(!arpEnabled){currentFreq=0.f;arpHeldCount=0;}
    else{arpStepIndex=0;arpLastStepMs=millis();}
}
void arpTypeNext(){arpType=(ArpType)(((uint8_t)arpType+1)%5);}
void arpTypePrev(){arpType=(ArpType)(((uint8_t)arpType+4)%5);}
const char *arpTypeLabel(){
    switch(arpType){
        case ArpType::UP:        return "Up";
        case ArpType::DOWN:      return "Down";
        case ArpType::UP_DOWN:   return "Up-Down";
        case ArpType::AS_PLAYED: return "As Played";
        case ArpType::RANDOM:    return "Random";
    }
    return "?";
}
void arpTempoInc(){arpTempoBpm=min(arpTempoBpm+5.f,240.f);}
void arpTempoDec(){arpTempoBpm=max(arpTempoBpm-5.f,40.f);}
char arpTempoBuf[8];
const char *arpTempoLabel(){snprintf(arpTempoBuf,sizeof(arpTempoBuf),"%.0f",arpTempoBpm);return arpTempoBuf;}
void arpRateNext(){arpRateIndex=(arpRateIndex+1)%NUM_ARP_RATES;}
void arpRatePrev(){arpRateIndex=(arpRateIndex+NUM_ARP_RATES-1)%NUM_ARP_RATES;}
const char *arpRateLabel(){return ARP_RATES[arpRateIndex].label;}
void arpSwingInc(){arpSwing=min(arpSwing+5.f,100.f);}
void arpSwingDec(){arpSwing=max(arpSwing-5.f,-100.f);}
char arpSwingBuf[8];
const char *arpSwingLabel(){snprintf(arpSwingBuf,sizeof(arpSwingBuf),"%+.0f%%",arpSwing);return arpSwingBuf;}

SettingItem arpMenuItems[]={
    {"Type",    arpTypeNext,  arpTypePrev,  arpTypeLabel},
    {"Tempo",   arpTempoInc,  arpTempoDec,  arpTempoLabel},
    {"Rate",    arpRateNext,  arpRatePrev,  arpRateLabel},
    {"Swing", arpSwingInc,arpSwingDec,arpSwingLabel},
};

SettingItem patternMenuItems[]={
    {"Save", patternBankSaveEnter, patternBankSaveEnter, patternBankEnterLabel},
    {"Load", patternBankLoadEnter, patternBankLoadEnter, patternBankEnterLabel},
    {"Random", openPatternRandomize, openPatternRandomize, resetEnterLabel},
};

SettingItem *getCategoryItems(int &count,const char *&title){
    switch(currentCategory){
        case SettingsCategory::PATCH:
            count=sizeof(patchMenuItems)/sizeof(patchMenuItems[0]);
            title="PATCH";
            return patchMenuItems;
        case SettingsCategory::IMU:
            if(isCardputerAdv){
                count=sizeof(imuMenuItemsAdv)/sizeof(imuMenuItemsAdv[0]);
                title="IMU";
                return imuMenuItemsAdv;
            } else {
                count=sizeof(imuMenuItemsOriginal)/sizeof(imuMenuItemsOriginal[0]);
                title="PAD";
                return imuMenuItemsOriginal;
            }
        case SettingsCategory::BEND:
            count=sizeof(bendMenuItems)/sizeof(bendMenuItems[0]);
            title="BEND";
            return bendMenuItems;
        case SettingsCategory::PORTAMENTO:
            count=sizeof(portaMenuItems)/sizeof(portaMenuItems[0]);
            title="PORTAMENTO";
            return portaMenuItems;
        case SettingsCategory::PLAYMODE:
            title="PLAY MODE";
            if(playMode==PlayMode::PRO){
                count=sizeof(playModeMenuItemsPro)/sizeof(playModeMenuItemsPro[0]);
                return playModeMenuItemsPro;
            } else {
                count=sizeof(playModeMenuItemsEZ)/sizeof(playModeMenuItemsEZ[0]);
                return playModeMenuItemsEZ;
            }
        case SettingsCategory::ARP:
            count=sizeof(arpMenuItems)/sizeof(arpMenuItems[0]);
            title="ARP";
            return arpMenuItems;
        case SettingsCategory::PATTERN:
            count=sizeof(patternMenuItems)/sizeof(patternMenuItems[0]);
            title="PATTERN";
            return patternMenuItems;
    }
    count=0;title="";return nullptr;
}

void openCategory(SettingsCategory c){
    currentCategory=c;
    selectedCategoryIndex=0;
    appMode=AppMode::CATEGORY;
}
void openPatchCategory(){openCategory(SettingsCategory::PATCH);}
void openImuCategory(){openCategory(SettingsCategory::IMU);}
void openBendCategory(){openCategory(SettingsCategory::BEND);}
void openPortaCategory(){openCategory(SettingsCategory::PORTAMENTO);}
void openPlayModeCategory(){openCategory(SettingsCategory::PLAYMODE);}
void openArpCategory(){openCategory(SettingsCategory::ARP);}
void openPatternCategory(){openCategory(SettingsCategory::PATTERN);}
const char *categoryEnterLabel(){return "Select>";}

// Top-level SETTING screen: just the category entry points. Arp is
// CardputerADV only (see the Arpeggiator section above for why), so
// original-Cardputer builds get a shorter list without it.
SettingItem settingItemsAdv[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"IMU",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Mode",  openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
    {"Arp",        openArpCategory,   openArpCategory,   categoryEnterLabel},
};
// Arp only makes sense from PLAY (it needs live chord-holding, which SEQ
// mode suppresses while a pattern is playing) — hidden when SEQ is the
// active home mode. Pattern is the inverse — only meaningful from SEQ
// (it saves/loads Sequencer step patterns) — hidden when PLAY is home.
SettingItem settingItemsAdvNoArp[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Pattern",    openPatternCategory, openPatternCategory, categoryEnterLabel},
    {"IMU",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Mode",  openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
};
// Original Cardputer has no Arp at all, but still needs the same
// Pattern-only-from-SEQ split as the ADV lists above.
SettingItem settingItemsOriginalPlay[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"PAD",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Mode",  openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
};
SettingItem settingItemsOriginalSeq[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Pattern",    openPatternCategory, openPatternCategory, categoryEnterLabel},
    {"PAD",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Mode",  openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
};
// Selected in setup() and whenever lastMainMode changes (see refreshSettingItems());
// kept as a plain pointer + variable (not compile-time const) so all the
// existing settingItems[i]/NUM_SETTING_ITEMS call sites elsewhere keep
// working unchanged.
SettingItem *settingItems=settingItemsAdv;
int NUM_SETTING_ITEMS=sizeof(settingItemsAdv)/sizeof(settingItemsAdv[0]);
int selectedSettingIndex=0;

void refreshSettingItems(){
    if(!isCardputerAdv){
        if(lastMainMode==AppMode::SEQ){
            settingItems=settingItemsOriginalSeq;
            NUM_SETTING_ITEMS=sizeof(settingItemsOriginalSeq)/sizeof(settingItemsOriginalSeq[0]);
        } else {
            settingItems=settingItemsOriginalPlay;
            NUM_SETTING_ITEMS=sizeof(settingItemsOriginalPlay)/sizeof(settingItemsOriginalPlay[0]);
        }
    } else if(lastMainMode==AppMode::SEQ){
        settingItems=settingItemsAdvNoArp;
        NUM_SETTING_ITEMS=sizeof(settingItemsAdvNoArp)/sizeof(settingItemsAdvNoArp[0]);
    } else {
        settingItems=settingItemsAdv;
        NUM_SETTING_ITEMS=sizeof(settingItemsAdv)/sizeof(settingItemsAdv[0]);
    }
    if(selectedSettingIndex>=NUM_SETTING_ITEMS)selectedSettingIndex=0;
}

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
    if(tab&&!prevTabPressed){
        if(appMode==AppMode::CATEGORY){
            if(!imuPickerOpen&&!imuCalibrateConfirmOpen&&!resetConfirmOpen&&!scalePickerOpen)appMode=AppMode::SETTINGS;
            // if the picker/confirm overlay IS open, its own Tab-cancel is
            // handled by updateImuPicker()/updateImuCalibrateConfirm()/updateResetConfirm()
        } else if(appMode!=AppMode::PATCH&&appMode!=AppMode::PATTERN&&appMode!=AppMode::SONG){
            AppMode prev=appMode;
            if(s.shift){
                // Shift+Tab: same cycle, reverse direction.
                switch(appMode){
                    case AppMode::PLAY:     appMode=AppMode::SETTINGS; currentFreq=0;break;
                    case AppMode::SEQ:      appMode=AppMode::SETTINGS; currentFreq=0;break;
                    case AppMode::VCO:      appMode=lastMainMode;      break;
                    case AppMode::VCF:      appMode=AppMode::VCO;      break;
                    case AppMode::VCA:      appMode=AppMode::VCF;      break;
                    case AppMode::LFO:      appMode=AppMode::VCA;      break;
                    case AppMode::SETTINGS: appMode=AppMode::LFO;      break;
                    default: break;
                }
            } else {
                switch(appMode){
                    case AppMode::PLAY:     appMode=AppMode::VCO;      currentFreq=0;break;
                    case AppMode::SEQ:      appMode=AppMode::VCO;      currentFreq=0;break;
                    case AppMode::VCO:      appMode=AppMode::VCF;      break;
                    case AppMode::VCF:      appMode=AppMode::VCA;      break;
                    case AppMode::VCA:      appMode=AppMode::LFO;      break;
                    case AppMode::LFO:      appMode=AppMode::SETTINGS; break;
                    case AppMode::SETTINGS: appMode=lastMainMode;      break;
                    default: break;
                }
            }
            if(prev==AppMode::SETTINGS||prev==AppMode::SEQ)saveSettings();
        }
        // AppMode::PATCH's and AppMode::PATTERN's Tab are each handled
        // entirely by their own updatePatchBrowser()/updatePatternBank()
    }
    prevTabPressed=tab;

    if(appMode==AppMode::SETTINGS){
        if(mU&&!prevMenuUpPressed)  selectedSettingIndex=(selectedSettingIndex-1+NUM_SETTING_ITEMS)%NUM_SETTING_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedSettingIndex=(selectedSettingIndex+1)%NUM_SETTING_ITEMS;
        if(mI&&!prevMenuIncPressed) settingItems[selectedSettingIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)settingItems[selectedSettingIndex].onDecrement();
    }
    else if(appMode==AppMode::CATEGORY&&!imuPickerOpen&&!imuCalibrateConfirmOpen&&!resetConfirmOpen&&!scalePickerOpen){
        int count;const char *title;SettingItem *items=getCategoryItems(count,title);
        if(mU&&!prevMenuUpPressed)  selectedCategoryIndex=(selectedCategoryIndex-1+count)%count;
        if(mD&&!prevMenuDownPressed)selectedCategoryIndex=(selectedCategoryIndex+1)%count;
        if(mI&&!prevMenuIncPressed) items[selectedCategoryIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)items[selectedCategoryIndex].onDecrement();
    }
    else if(appMode==AppMode::VCO){
        if(mU&&!prevMenuUpPressed)  selectedVcoIndex=(selectedVcoIndex-1+NUM_VCO_ITEMS)%NUM_VCO_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcoIndex=(selectedVcoIndex+1)%NUM_VCO_ITEMS;
        if(mI&&!prevMenuIncPressed) vcoItems[selectedVcoIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcoItems[selectedVcoIndex].onDecrement();
    }
    else if(appMode==AppMode::VCF){
        if(mU&&!prevMenuUpPressed)  selectedVcfIndex=(selectedVcfIndex-1+NUM_VCF_ITEMS)%NUM_VCF_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcfIndex=(selectedVcfIndex+1)%NUM_VCF_ITEMS;
        if(mI&&!prevMenuIncPressed) vcfItems[selectedVcfIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcfItems[selectedVcfIndex].onDecrement();
    }
    else if(appMode==AppMode::VCA){
        if(mU&&!prevMenuUpPressed)  selectedVcaIndex=(selectedVcaIndex-1+NUM_VCA_ITEMS)%NUM_VCA_ITEMS;
        if(mD&&!prevMenuDownPressed)selectedVcaIndex=(selectedVcaIndex+1)%NUM_VCA_ITEMS;
        if(mI&&!prevMenuIncPressed) vcaItems[selectedVcaIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)vcaItems[selectedVcaIndex].onDecrement();
    }
    else if(appMode==AppMode::LFO){
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
void drawTabBar(LovyanGFX &gfx,AppMode cur){
    gfx.fillRect(0,0,240,11,BLACK);
    struct{const char *l;AppMode m;int x;}tabs[]={
        {"PLAY",AppMode::PLAY,0},   {"VCO",AppMode::VCO,40},
        {"VCF",AppMode::VCF,80},    {"VCA",AppMode::VCA,120},
        {"LFO",AppMode::LFO,160},   {"SET",AppMode::SETTINGS,200}
    };
    constexpr int TW=40;
    for(auto &t:tabs){
        bool act=(cur==t.m)||(t.m==AppMode::PLAY&&cur==AppMode::SEQ);
        gfx.drawRect(t.x,0,TW,11,uiColor);
        if(act){gfx.fillRect(t.x+1,1,TW-2,9,WHITE);gfx.setTextColor(BLACK,WHITE);}
        else    gfx.setTextColor(uiColor,BLACK);
        int lx=t.x+(TW-(int)strlen(t.l)*6)/2;
        gfx.setCursor(lx,2);gfx.print(t.l);
    }
    gfx.setTextColor(uiColor,BLACK);
}

void drawAdsrGraph(){
    constexpr int GX=0,GY=12,GW=240,GH=60;
    canvas.fillRect(GX,GY,GW,GH,BLACK);
    constexpr float FIXED=ADSR_MAX_TIME,SF=0.15f;
    float sx=(float)GW/(FIXED+FIXED*SF);
    int top=GY+4,bot=GY+GH-4,susY=bot-(int)((bot-top)*adsr.sustainLevel);
    int x0=GX;
    int x1=x0+max(1,(int)(adsr.attackTime*sx));
    int x2=x1+max(1,(int)(adsr.decayTime*sx));
    int x3=x2+(int)(FIXED*SF*sx);
    int x4=x3+max(1,(int)(adsr.releaseTime*sx));
    x1=min(x1,GX+GW-3);x2=min(x2,GX+GW-2);x3=min(x3,GX+GW-1);x4=min(x4,GX+GW);
    canvas.drawLine(x0,bot,x1,top,uiColor);
    canvas.drawLine(x1,top,x2,susY,uiColor);
    canvas.drawLine(x2,susY,x3,susY,uiColor);
    canvas.drawLine(x3,susY,x4,bot,uiColor);
    uint16_t yel=canvas.color565(255,255,0);
    for(auto &p:{std::pair<int,int>{x1,top},{x2,susY},{x3,susY}})
        canvas.fillRect(p.first-1,p.second-1,3,3,yel);
    canvas.setCursor(x0+2,GY+GH-10);canvas.print("A");
    canvas.setCursor(x1+2,GY+GH-10);canvas.print("D");
    canvas.setCursor((x2+x3)/2-3,GY+GH-10);canvas.print("S");
    canvas.setCursor(x3+2,GY+GH-10);canvas.print("R");
    uint16_t dim=canvas.color565(0,64,0);
    int m1=GX+(int)(1.f*sx),m25=GX+(int)(2.5f*sx);
    canvas.drawFastVLine(m1,GY,GH,dim);
    canvas.drawFastVLine(m25,GY,GH,dim);
    canvas.setCursor(m1+1,GY+1);canvas.print("1s");
    canvas.setCursor(m25+1,GY+1);canvas.print("2.5s");
    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
}

void drawWaveform(LovyanGFX &gfx,float morph,float pwm){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2,CYCLES=3;
    gfx.fillRect(GX,GY,GW,GH,BLACK);
    gfx.drawFastHLine(GX,CY,GW,gfx.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE*CYCLES)%WAVE_TABLE_SIZE;
        int16_t s=getMorphedSample(idx,morph,pwm);
        int y=constrain(CY-(int)((float)s/32768.f*(GH/2-2)),GY,GY+GH-1);
        if(px>0)gfx.drawLine(px-1,pY,px,y,uiColor);
        pY=y;
    }
    gfx.drawFastHLine(GX,GY+GH,GW,uiColor);
}

// Draw a scrollable item list.
// splitCol: if >= 0, items from index 0..(splitCol-1) go left, rest go right.
// splitCol < 0: single column layout.
void drawItemList(SettingItem *items,int count,int sel,int startY=76,int splitCol=-1,bool showNav=true){
    constexpr int ROW=13;
    constexpr int LX=5,RX=123;
    bool twoCol=(splitCol>0&&splitCol<count);
    canvas.setTextColor(uiColor,BLACK);

    if(twoCol){
        // Vertical divider — sized to the taller of the two columns
        int rows=max(splitCol,count-splitCol);
        canvas.drawFastVLine(119,startY-2,rows*ROW+4,canvas.color565(0,64,0));
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
        canvas.setCursor(x,y);
        if(i==sel)canvas.printf(">%-8s%s",items[i].name,items[i].valueLabel());
        else      canvas.printf(" %-8s%s",items[i].name,items[i].valueLabel());
    }
    if(!showNav)return;
    const char *nav=";/. select  ,// change  Tab:next";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,126);
    canvas.print(nav);
}

void drawVcoScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCO);
    drawWaveform(canvas,params.timbreMorph,params.pwmWidth);
    canvas.fillRect(0,56,240,70,BLACK);
    // Left(0-3): Timbre/PWM/Detune/FineTune  Right(4-6): SubLvl/SubOct/Noise
    drawItemList(vcoItems,NUM_VCO_ITEMS,selectedVcoIndex,57,4);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawVcfScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCF);
    constexpr int GX=0,GY=12,GW=240,GH=60;
    canvas.fillRect(GX,GY,GW,GH+4,BLACK);

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
    uint16_t dim = canvas.color565(0,64,0);
    canvas.drawFastHLine(GX, zeroY, GW, dim);

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
            canvas.drawLine(px-1, prevY, px, y, uiColor);
        prevY = y;
    }

    // Cutoff marker (yellow vertical line)
    // Convert linear cutoff ratio to log-scale x position
    int cx = GX + (int)((log(filterParams.cutoffHz/F_MIN)/log(F_MAX/F_MIN)) * GW);
    cx = constrain(cx, GX, GX+GW-1);
    canvas.drawFastVLine(cx, GY, GH, canvas.color565(255,255,0));

    // Cutoff frequency label
    char fLabel[12]; snprintf(fLabel,sizeof(fLabel),"%.0fHz",filterParams.cutoffHz);
    int lx = (cx+4 < GX+GW-30) ? cx+2 : cx-28;
    canvas.setCursor(lx, GY+2); canvas.print(fLabel);

    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
    canvas.fillRect(0,76,240,50,BLACK);
    // Left(0-3): Filter/Cutoff/Resonance/KeyTrack  Right(4-7): FEnv Dep/Atk/Dec/Rel
    drawItemList(vcfItems,NUM_VCF_ITEMS,selectedVcfIndex,76,4);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawVcaScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCA);
    drawAdsrGraph();
    canvas.fillRect(0,76,240,50,BLACK);
    // Single column, 4 items at 13px intervals: 76,89,102,115 — nav at 126 clears
    drawItemList(vcaItems,NUM_VCA_ITEMS,selectedVcaIndex,76);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// Live LFO waveform: one full cycle drawn across the screen width, scaled
// by the current depth, plus a moving marker showing the LFO's live phase.
void drawLfoWaveform(){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2;
    canvas.fillRect(GX,GY,GW,GH,BLACK);
    canvas.drawFastHLine(GX,CY,GW,canvas.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE)%WAVE_TABLE_SIZE;
        float s=lfoTableSample(lfo.wave,idx)*lfo.depth;
        int y=constrain(CY-(int)(s*(GH/2-2)),GY,GY+GH-1);
        if(px>0)canvas.drawLine(px-1,pY,px,y,uiColor);
        pY=y;
    }
    // Live phase marker
    int mx=GX+(int)((lfoPhase/(float)WAVE_TABLE_SIZE)*GW);
    mx=constrain(mx,GX,GX+GW-1);
    canvas.drawFastVLine(mx,GY,GH,canvas.color565(255,255,0));
    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
}

void drawLfoScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::LFO);
    drawLfoWaveform();
    canvas.fillRect(0,56,240,70,BLACK);
    drawItemList(lfoItems,NUM_LFO_ITEMS,selectedLfoIndex,76);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawSettingsScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::SETTINGS);
    // Clear entire area below tab bar to remove any residual drawing from other screens
    canvas.fillRect(0,12,240,123,BLACK);

    // Single column: Patch / IMU / Bend / Portamento entry points.
    // Each opens its own dedicated category screen (AppMode::CATEGORY).
    drawItemList(settingItems,NUM_SETTING_ITEMS,selectedSettingIndex,24,-1,false);

    const char *n1=";/. select  /:open category";
    const char *n2="Tab: save & return to play";
    canvas.setCursor((240-(int)strlen(n1)*6)/2,115);canvas.print(n1);
    canvas.setCursor((240-(int)strlen(n2)*6)/2,128);canvas.print(n2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawCategoryScreen(bool full){
    canvas.startWrite();
    int count;const char *title;SettingItem *items=getCategoryItems(count,title);
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        char titleBuf[24];snprintf(titleBuf,sizeof(titleBuf),"SETTING > %s",title);
        canvas.setCursor((240-(int)strlen(titleBuf)*6)/2,2);
        canvas.print(titleBuf);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    int splitCol=(currentCategory==SettingsCategory::IMU)?(isCardputerAdv?5:4):-1;
    drawItemList(items,count,selectedCategoryIndex,24,splitCol,false);
    const char *n1=";/. select  ,// change";
    const char *n2="Tab: back";
    canvas.setCursor((240-(int)strlen(n1)*6)/2,115);canvas.print(n1);
    canvas.setCursor((240-(int)strlen(n2)*6)/2,128);canvas.print(n2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawImuPickerScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        char titleBuf[24];snprintf(titleBuf,sizeof(titleBuf),"%s %s Target",isCardputerAdv?"IMU":"PAD",imuPickerAxis==0?"X":"Y");
        canvas.setCursor((240-(int)strlen(titleBuf)*6)/2,2);
        canvas.print(titleBuf);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    constexpr int ROW=13,startY=18,maxRows=7;
    int top=constrain(imuPickerIndex-maxRows/2,0,max(0,IMU_PICKER_COUNT-maxRows));
    uint16_t dim=canvas.color565(0,64,0);
    for(int i=0;i<maxRows&&(top+i)<IMU_PICKER_COUNT;i++){
        int row=top+i;
        int y=startY+i*ROW;
        if(IMU_PICKER_ORDER[row].divider&&row>0)
            canvas.drawFastHLine(4,y-4,232,dim);
        canvas.setCursor(6,y);
        const char *label=imuTargetName(IMU_PICKER_ORDER[row].target);
        if(row==imuPickerIndex)canvas.printf(">%s",label);
        else                   canvas.printf(" %s",label);
    }
    const char *pn1=";/.:Select  / or Enter:OK";
    const char *pn2="Tab:Cancel";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawScalePickerScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(scalePickerLevel==0)?"Scale: Category":SCALE_CATEGORY_NAMES[scalePickerCategoryIndex];
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    constexpr int ROW=13,startY=18,maxRows=7;
    if(scalePickerLevel==0){
        int top=constrain(scalePickerRowIndex-maxRows/2,0,max(0,NUM_SCALE_CATEGORIES-maxRows));
        for(int i=0;i<maxRows&&(top+i)<NUM_SCALE_CATEGORIES;i++){
            int row=top+i;int y=startY+i*ROW;
            canvas.setCursor(6,y);
            if(row==scalePickerRowIndex)canvas.printf(">%s",SCALE_CATEGORY_NAMES[row]);
            else                        canvas.printf(" %s",SCALE_CATEGORY_NAMES[row]);
        }
    } else {
        int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
        int top=constrain(scalePickerRowIndex-maxRows/2,0,max(0,n-maxRows));
        for(int i=0;i<maxRows&&(top+i)<n;i++){
            int row=top+i;int y=startY+i*ROW;
            canvas.setCursor(6,y);
            const char *label=SCALES[indices[row]].name;
            if(row==scalePickerRowIndex)canvas.printf(">%s",label);
            else                        canvas.printf(" %s",label);
        }
    }
    const char *pn1=(scalePickerLevel==0)?";/.:Select  / or Enter:Open":";/.:Preview  / or Enter:OK";
    const char *pn2=(scalePickerLevel==0)?"Tab:Cancel":"Tab:Back";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawImuCalibrateConfirmScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="IMU Calibrate";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    const char *msg1="Set current tilt as zero point?";
    const char *msg2="Hold the device the way you'll play it.";
    canvas.setCursor((240-(int)strlen(msg1)*6)/2,45);canvas.print(msg1);
    canvas.setCursor((240-(int)strlen(msg2)*6)/2,58);canvas.print(msg2);
    const char *nav="/ or Enter:Yes   Tab:No";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,80);canvas.print(nav);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawResetConfirmScreen(bool full){
    canvas.startWrite();
    const char *title, *msg1, *msg2;
    switch(resetConfirmKind){
        case ResetKind::PATCH_TONE:
            title="Reset Patch";
            msg1="Reset VCO/VCF/VCA/LFO/IMU";
            msg2="to default? (unsaved changes lost)";
            break;
        case ResetKind::PATCH_RANDOM:
            title="Randomize Patch";
            msg1="Randomize VCO/VCF/VCA/LFO/IMU?";
            msg2="(unsaved changes lost)";
            break;
        case ResetKind::PATTERN_RANDOM:
            title="Randomize Pattern";
            msg1="Randomize the 16-step Sequencer";
            msg2="pattern? (unsaved changes lost)";
            break;
        case ResetKind::BEND:
            title="Reset Bend";
            msg1="Reset Bend width/attack/release";
            msg2="to default?";
            break;
        case ResetKind::PORTAMENTO:
        default:
            title="Reset Portamento";
            msg1="Reset Portamento on/off + speed";
            msg2="to default?";
            break;
    }
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    canvas.setCursor((240-(int)strlen(msg1)*6)/2,45);canvas.print(msg1);
    canvas.setCursor((240-(int)strlen(msg2)*6)/2,58);canvas.print(msg2);
    const char *nav="/ or Enter:Yes   Tab:No";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,80);canvas.print(nav);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// ==========================================================
// Patch Bank screen
// ==========================================================
void drawPatternBankScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(patternBankMode==PatternBankMode::SAVE)?"PATTERN BANK - SAVE":"PATTERN BANK - LOAD";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    // Grid: 8 banks (rows, A-H) x 8 slots (columns, 1-8). Filled = has
    // data, outline only = empty. Selection shown in WHITE.
    constexpr int GX=20,GY=18,CW=26,CH=13;
    canvas.setCursor(GX,GY-10);
    for(int col=0;col<NUM_PATTERNS_PER_BANK;col++)canvas.printf("%-4d",col+1);
    for(int b=0;b<NUM_PATTERN_BANKS;b++){
        canvas.setCursor(4,GY+b*CH+3);
        canvas.printf("%c",'A'+b);
        for(int sl=0;sl<NUM_PATTERNS_PER_BANK;sl++){
            int cx=GX+sl*CW,cy=GY+b*CH;
            bool occupied=patternSlotExists(b,sl);
            bool isSel=(b==patternSelBank&&sl==patternSelSlot);
            uint16_t col2=isSel?WHITE:uiColor;
            if(occupied)canvas.fillRect(cx,cy,CW-4,CH-3,col2);
            else        canvas.drawRect(cx,cy,CW-4,CH-3,col2);
        }
    }

    if(patternConfirmDelete||patternConfirmOverwrite){
        canvas.fillRect(20,50,200,26,BLACK);
        canvas.drawRect(20,50,200,26,uiColor);
        canvas.setCursor(28,58);
        canvas.printf("%c%d: %s? (Y/N)",'A'+patternSelBank,patternSelSlot+1,
            patternConfirmDelete?"Delete":"Overwrite");
    } else {
        const char *nav=(patternBankMode==PatternBankMode::SAVE)
            ?"Enter:Save  Bksp:Clear  Tab:Cancel"
            :"Enter:Load  Bksp:Clear  Tab:Cancel";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,124);
        canvas.print(nav);
    }
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawSongScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="SONG";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    if(helpVisible){
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }
    canvas.fillRect(0,12,240,123,BLACK);

    if(songIoPickerOpen){
        canvas.setCursor(10,16);
        canvas.print(songIoMode==SongIoMode::SAVE?"SAVE SONG":"LOAD SONG");
        for(int i=0;i<NUM_SONG_SLOTS;i++){
            int y=28+i*11;
            bool occupied=songSlotExists(i);
            bool isSel=(i==songIoSelSlot);
            canvas.setTextColor(isSel?WHITE:uiColor,BLACK);
            canvas.setCursor(20,y);
            canvas.printf("%s%d %s",isSel?">":" ",i+1,occupied?"[X]":"[ ]");
        }
        canvas.setTextColor(uiColor,BLACK);
        if(songIoConfirmDelete||songIoConfirmOverwrite){
            canvas.fillRect(20,50,200,26,BLACK);
            canvas.drawRect(20,50,200,26,uiColor);
            canvas.setCursor(28,58);
            canvas.printf("Song %d: %s? (Y/N)",songIoSelSlot+1,songIoConfirmDelete?"Delete":"Overwrite");
        } else {
            canvas.setCursor(10,120);
            canvas.print("Enter:OK  Bksp:Clear  Tab:Back");
        }
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }

    canvas.setCursor(4,14);
    canvas.printf("Bpm:%3.0f Swg:%+3.0f%%  Vol:%d%%",songTempoBpm,songSwing,(int)(params.keyVolume*100));
    canvas.setCursor(4,23);
    if(songFocus==SongFocus::ENTRY){
        const char *fieldNames[]={"Bank","Slot","Transpose","Repeat"};
        canvas.printf("Focus:Entry Field:%-9s %s",fieldNames[(int)songField],songPlaying?"PLAYING":"STOPPED");
    } else {
        canvas.printf("Focus:Song  Field:%-9s %s",songSettingsField==SongSettingsField::TEMPO?"Tempo":"Swing",songPlaying?"PLAYING":"STOPPED");
    }

    if(songLen==0){
        canvas.setCursor(4,60);
        canvas.print("(empty - Enter adds a step)");
    } else {
        // ---- Timeline (Option A): each entry as a fixed-width block,
        // colored by Bank letter, with a thin bar underneath whose width
        // is proportional to Repeat count. The playing entry is
        // highlighted white; the edit cursor gets a white outline
        // instead (so both can be shown distinctly even on the same
        // block).
        constexpr int TL_Y=33,TL_BH=18,TL_BW=32,TL_GAP=3,TL_STRIDE=TL_BW+TL_GAP;
        int visibleBlocks=240/TL_STRIDE;
        int tlStart=max(0,min(songCursorEntry-visibleBlocks/2,max(0,songLen-visibleBlocks)));
        for(int i=0;i<visibleBlocks&&tlStart+i<songLen;i++){
            int idx=tlStart+i;
            SongEntry &e=songEntries[idx];
            int x=2+i*TL_STRIDE;
            bool isCursor=(idx==songCursorEntry);
            bool isPlaying=(songPlaying&&idx==songPlayEntry);
            uint16_t blockColor=isPlaying?WHITE:songBankColors[e.bank];
            canvas.fillRect(x,TL_Y,TL_BW,TL_BH,blockColor);
            canvas.drawRect(x,TL_Y,TL_BW,TL_BH,isCursor?WHITE:uiColor);
            canvas.setTextColor(BLACK,blockColor);
            canvas.setCursor(x+3,TL_Y+4);
            canvas.printf("%c%d",'A'+e.bank,e.slot+1);
            canvas.setTextColor(uiColor,BLACK);
            int repW=map(e.repeat,1,16,3,TL_BW-2);
            canvas.fillRect(x+1,TL_Y+TL_BH+2,repW,3,isCursor?WHITE:songBankColors[e.bank]);
        }

        // ---- Mini step-grid preview (Option B): shows the actual step
        // shape of whichever entry is playing (or, when stopped, the
        // entry the cursor is on) — filled = note, half-height = tie.
        int previewIdx=songPlaying?songPlayEntry:songCursorEntry;
        SongEntry &pe=songEntries[previewIdx];
        loadPatternPreview(pe.bank,pe.slot);
        constexpr int PG_Y=68,PG_H=26,PG_SW=14,PG_BW=12;
        canvas.setCursor(4,58);
        canvas.printf("Preview: %c%d",'A'+pe.bank,pe.slot+1);
        for(int i=0;i<SEQ_NUM_STEPS;i++){
            int x=4+i*PG_SW;
            SeqStep &ps=songPreviewSteps[i];
            canvas.drawRect(x,PG_Y,PG_BW,PG_H,uiColor);
            if(ps.freq>0.f)canvas.fillRect(x+1,PG_Y+1,PG_BW-2,PG_H-2,uiColor);
            else if(ps.tie)canvas.fillRect(x+1,PG_Y+PG_H/2,PG_BW-2,PG_H/2-1,uiColor);
        }

        // ---- Selected entry's own Transpose/Repeat, since the timeline
        // blocks are too small to show these numbers directly. Kept
        // short (well under 240px/40 chars) — a longer version here
        // previously overflowed the screen width and wrapped onto the
        // line below it.
        SongEntry &ce=songEntries[songCursorEntry];
        canvas.setCursor(4,98);
        canvas.printf("E%d/%d %c%d  T:%+d  x%d",
            songCursorEntry+1,songLen,'A'+ce.bank,ce.slot+1,ce.transpose,ce.repeat);
        canvas.setCursor(4,109);
        canvas.printf("I:Inherit T/S %-3s  O:Loop %-3s",songInheritTempoSwing?"ON":"off",songLoopAtEnd?"ON":"off");
    }

    canvas.setCursor(4,124);
    canvas.print("Space:Play/Stop  H:Help  Tab:Back");

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawPatchScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(patchMode==PatchMode::SAVE)?"PATCH BANK - SAVE":"PATCH BANK - LOAD";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    if(patchUiState==PatchUiState::NAME_ENTRY){
        const char *label=patchRenaming?"Rename to:":(patchDuplicating?"Duplicate as:":"New patch name:");
        canvas.setCursor(6,28);canvas.print(label);
        canvas.drawRect(6,40,228,16,uiColor);
        canvas.setCursor(10,44);
        canvas.printf("%s_",patchNameBuffer.c_str());
        const char *nav="Type name   Enter:OK   Tab:Cancel";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,110);
        canvas.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_DELETE){
        canvas.setCursor(6,40);
        canvas.printf("Delete '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,60);
        canvas.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_OVERWRITE){
        canvas.setCursor(6,40);
        canvas.printf("Overwrite '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,60);
        canvas.print(nav);
    } else {
        int count=patchListCount();
        if(count==0){
            canvas.setCursor(6,40);
            canvas.print("No patches saved yet.");
        } else {
            constexpr int ROW=13,startY=16,maxRows=7;
            int top=constrain(selectedPatchIndex-maxRows/2,0,max(0,count-maxRows));
            for(int i=0;i<maxRows&&(top+i)<count;i++){
                int row=top+i;
                canvas.setCursor(6,startY+i*ROW);
                String label=patchIsNewRow(row)?"<New Patch>":patchNames[patchRowToNameIndex(row)];
                if(row==selectedPatchIndex)canvas.printf(">%s",label.c_str());
                else                       canvas.printf(" %s",label.c_str());
            }
        }
        const char *nav1=";/.:Select  /:OK  r:Rename";
        const char *nav2="c:Duplicate  ,:Delete  Tab:Back";
        canvas.setCursor((240-(int)strlen(nav1)*6)/2,111);
        canvas.print(nav1);
        canvas.setCursor((240-(int)strlen(nav2)*6)/2,122);
        canvas.print(nav2);
    }
    canvas.endWrite();
    canvas.pushSprite(0,0);
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

void drawBendMeter(LovyanGFX &gfx,float bc,float mc,int yOff=0,int xOff=0){
    const int MX=80+xOff; constexpr int MW=12,MH_TOTAL=55; // total footprint matches the other side blocks (57-112)
    const int MY=57+yOff;
    constexpr int LBL_H=8; // vertical space reserved for the UP/DWN labels, top and bottom
    const int GY=MY+LBL_H, GH=MH_TOTAL-2*LBL_H, GCY=GY+GH/2; // the gauge itself, inset between the labels
    const int GCX=MX+MW/2; // gauge's horizontal center, used to center the UP/DWN labels on it
    // Clear the meter's full footprint incl. labels (DWN is the widest at
    // 18px, centered on GCX) — starts at x=76, clearing the left info
    // box's border at x=73 by 3px so the two boxes' borders stay intact.
    gfx.fillRect(76+xOff,MY,20,MH_TOTAL,BLACK);
    gfx.drawRect(MX,GY,MW,GH,uiColor);
    gfx.drawFastHLine(MX,GCY,MW,uiColor);
    if(fabsf(bc)>0.5f){
        float r=constrain(bc/mc,-1.f,1.f);
        int bh=(int)(fabsf(r)*(GH/2-1));
        gfx.fillRect(MX+1,r>0?GCY-bh:GCY,MW-2,bh,uiColor);
    }
    gfx.setCursor(GCX-6,MY);gfx.print("UP");         // "UP" = 2 chars = 12px, centered
    gfx.setCursor(GCX-9,MY+MH_TOTAL-7);gfx.print("DWN"); // "DWN" = 3 chars = 18px, centered
}

float dispAccelX=0.f, dispAccelY=0.f; // display-only smoothing, doesn't affect audio

void drawImuPad(LovyanGFX &gfx,int yOff=0,int xOff=0){
    const int PX=98+xOff; constexpr int PAD_SIZE=44;
    const int PY=57+yOff,cx=PX+PAD_SIZE/2,cy=PY+PAD_SIZE/2;
    // NOTE: the pad's own fillRect below already clears its full interior
    // (including where the "Y" label sits) every redraw, so no external
    // clear-rect is needed. A prior version cleared a rect starting at
    // PY-9 (i.e. reaching up to y=48), which intruded into the waveform
    // plot's row range (y=12-54) directly above this pad and periodically
    // erased part of the waveform curve there — moving the label fully
    // inside the pad's box removes that overlap entirely.
    gfx.fillRect(PX,PY,PAD_SIZE,PAD_SIZE,BLACK);
    gfx.drawRect(PX,PY,PAD_SIZE,PAD_SIZE,uiColor);
    uint16_t dim=gfx.color565(0,64,0);
    gfx.drawFastHLine(PX,cy,PAD_SIZE,dim);
    gfx.drawFastVLine(cx,PY,PAD_SIZE,dim);
    int dX,dY;
    if(isCardputerAdv){
        dispAccelX+=(lastAccelX-dispAccelX)*0.3f;
        dispAccelY+=(lastAccelY-dispAccelY)*0.3f;
        // Same angle conversion as updateImu(), including each axis's
        // calibration offset, so the dot recenters after Calibrate is used.
        float angleXDeg=asinf(constrain(dispAccelX,-1.f,1.f))*180.f/PI-imuAxisX.calOffsetDeg;
        float angleYDeg=asinf(constrain(dispAccelY,-1.f,1.f))*180.f/PI-imuAxisY.calOffsetDeg;
        float nX=constrain(angleXDeg/TILT_MAX_DEGREES,-1.f,1.f);
        float nY=constrain(angleYDeg/TILT_MAX_DEGREES,-1.f,1.f);
        dX=constrain(cx+(int)(nX*(PAD_SIZE/2-3)),PX+2,PX+PAD_SIZE-3);
        dY=constrain(cy+(int)(nY*(PAD_SIZE/2-3)),PY+2,PY+PAD_SIZE-3);
    } else {
        // Original Cardputer: show the key-driven virtual PAD position
        // directly (already -1..1, no calibration/tilt-angle concept here).
        dX=constrain(cx+(int)(padVirtualX*(PAD_SIZE/2-3)),PX+2,PX+PAD_SIZE-3);
        dY=constrain(cy+(int)(padVirtualY*(PAD_SIZE/2-3)),PY+2,PY+PAD_SIZE-3);
    }
    // Hollow circle once Calibrate has been used, filled dot otherwise —
    // an at-a-glance reminder that the zero point has been moved.
    if(imuCalibrated)gfx.drawCircle(dX,dY,3,uiColor);
    else             gfx.fillCircle(dX,dY,3,uiColor);
    gfx.setCursor(cx-3,PY+PAD_SIZE+2);gfx.print("Y");
    gfx.setCursor(PX+PAD_SIZE+2,cy-4);gfx.print("X");
}

float getImuNorm(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        return params.timbreMorph/3.f;
        case ImuTarget::VIBRATO_DEPTH: return params.vibratoDepth;
        case ImuTarget::VIBRATO_RATE:  return (params.vibratoRateHz-1)/9.f;
        case ImuTarget::TREMOLO:       return params.tremoloDepth;
        case ImuTarget::VOLUME:        return 1.0f-params.volumeScale;
        case ImuTarget::PITCH_BEND:    return (params.pitchBendCents+keyBendMaxCents)/(keyBendMaxCents*2);
        case ImuTarget::BEND_UP:       return constrain(params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BEND_DOWN:     return constrain(-params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BITCRUSH:      return params.bitcrush;
        case ImuTarget::FILTER_CUTOFF: return params.filterCutoffOffset;
        case ImuTarget::PWM:           return (params.pwmOffset+0.4f)/0.8f;
        case ImuTarget::DETUNE:        return (params.detuneOffset+50.f)/100.f;
        case ImuTarget::NOISE:         return (params.noiseOffset+1.f)/2.f;
        case ImuTarget::SUB_LEVEL:     return (params.subLevelOffset+1.f)/2.f;
        case ImuTarget::RESONANCE:     return (params.resonanceOffset+3.f)/6.f;
        case ImuTarget::LFO_RATE:      return (lfoRateOffset+LFO_RATE_MAX)/(LFO_RATE_MAX*2.f);
        case ImuTarget::LFO_DEPTH:     return (lfoDepthOffset+1.f)/2.f;
        case ImuTarget::ARP_TEMPO:     return ((lastMainMode==AppMode::SEQ?seqTempoOffset:arpTempoOffset)+100.f)/200.f;
        case ImuTarget::ARP_SWING:   return ((lastMainMode==AppMode::SEQ?seqSwingOffset:arpSwingOffset)+50.f)/100.f;
        default: return 0;
    }
}

String getImuValStr(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:{const char *s[]={"Sine","Tri","Saw","Sq"};return String(s[constrain((int)params.timbreMorph,0,3)])+"("+String(params.timbreMorph,1)+")";}
        case ImuTarget::VIBRATO_DEPTH: return String((int)(params.vibratoDepth*100))+"%";
        case ImuTarget::VIBRATO_RATE:  return String(params.vibratoRateHz,1)+"Hz";
        case ImuTarget::TREMOLO:       return String((int)(params.tremoloDepth*100))+"%";
        case ImuTarget::VOLUME:        return String((int)(params.volumeScale*100))+"%";
        case ImuTarget::PITCH_BEND:    return String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_UP:       return "+"+String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_DOWN:     return String((int)params.pitchBendCents)+"c";
        case ImuTarget::BITCRUSH:      return String((int)(params.bitcrush*100))+"%";
        case ImuTarget::FILTER_CUTOFF:{float c=filterParams.cutoffHz*(1-params.filterCutoffOffset*0.9f);return String((int)constrain(c,FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX))+"Hz";}
        case ImuTarget::PWM:           return String((int)(params.pwmOffset*100))+"%";
        case ImuTarget::DETUNE:        return String((int)params.detuneOffset)+"c";
        case ImuTarget::NOISE:         return String((int)(params.noiseOffset*100))+"%";
        case ImuTarget::SUB_LEVEL:     return String((int)(params.subLevelOffset*100))+"%";
        case ImuTarget::RESONANCE:     return String(params.resonanceOffset,1);
        case ImuTarget::LFO_RATE:      return String(lfoRateOffset,1)+"Hz";
        case ImuTarget::LFO_DEPTH:     return String((int)(lfoDepthOffset*100))+"%";
        case ImuTarget::ARP_TEMPO:     return String(lastMainMode==AppMode::SEQ?seqTempoOffset:arpTempoOffset,0)+"bpm";
        case ImuTarget::ARP_SWING:   return String(lastMainMode==AppMode::SEQ?seqSwingOffset:arpSwingOffset,0)+"%";
        default: return "---";
    }
}

// HELP overlay: shown while H is held
// Drawn over the existing screen content without fillScreen (no flicker)
void drawHelpOverlay(LovyanGFX &gfx){
    gfx.setTextColor(uiColor,BLACK);
    gfx.fillRect(2,13,236,99,BLACK);
    gfx.drawRect(2,13,236,99,uiColor);
    if(appMode==AppMode::SEQ){
        gfx.setCursor(6,17); gfx.print("=== SEQ HELP (hold H) ===");
        gfx.setCursor(6,26); gfx.print(",// :Cursor  Notekey:Set+Preview+Adv");
        gfx.setCursor(6,35); gfx.print("Bksp:Clear  Shift+Bksp:Clear all");
        gfx.setCursor(6,44); gfx.print("Space:Play/Stop  f:Focus toggle");
        gfx.setCursor(6,53); gfx.print(";/.:Adjust/Toggle  g:Cycle target");
        gfx.setCursor(6,62);
        if(isCardputerAdv)gfx.print("Shift+;/./,//:Octave/Transpose");
        else              gfx.print("J/N :Octave    B/M:Transpose");
        gfx.setCursor(6,71); gfx.print("k/l :Volume    Z/X:Bend");
        gfx.setCursor(6,80); gfx.print("V:Mark  Sh+C:Copy Sh+X:Cut Ent:Paste");
        gfx.setCursor(6,89); gfx.print("Tab:Next Sh+Tab:Prev G0:PLAY");
        gfx.setCursor(6,98); gfx.print("release H to close");
        return;
    }
    if(appMode==AppMode::SONG){
        gfx.setCursor(6,17); gfx.print("=== SONG HELP (hold H) ===");
        gfx.setCursor(6,26); gfx.print(",// :Entry cursor  f:Focus Entry/Song");
        gfx.setCursor(6,35); gfx.print("g:Cycle field  ;/.:Adjust field");
        gfx.setCursor(6,44); gfx.print("Enter:Insert entry (copies Bank/Slot)");
        gfx.setCursor(6,53); gfx.print("Bksp:Delete entry  k/l:Volume");
        gfx.setCursor(6,62); gfx.print("Space:Play/Stop Song");
        gfx.setCursor(6,71); gfx.print("Shift+S:Save  Shift+L:Load Song");
        gfx.setCursor(6,80); gfx.print("I:Inherit T/S  O:Loop-at-end");
        gfx.setCursor(6,89); gfx.print("H:Help(hold) Tab:Back to PLAY/SEQ");
        gfx.setCursor(6,98); gfx.print("release H to close");
        return;
    }
    gfx.setCursor(6,17); gfx.print("=== HELP (hold H) ===");
    gfx.setCursor(6,26);
    if(playMode==PlayMode::PRO)gfx.print("1-0-=+Bksp/q..[]\\:Notes(scale)");
    else                       gfx.print("1-0-=+Bksp/q..[]\\:Notes(Major)");
    if(isCardputerAdv){
        gfx.setCursor(6,35); gfx.print(";/. :Octave    ,//:Transpose");
        gfx.setCursor(6,44); gfx.print("k/l :Volume    Z/X:Bend");
        gfx.setCursor(6,53); gfx.print("C:Porta  A:IMU-X  S:IMU-Y");
        gfx.setCursor(6,62); gfx.print("D:NoteHold  V:Latch Sh+V:ArpOn/Off");
        gfx.setCursor(6,71); gfx.print("Space:Seq Play/Stop");
    } else {
        gfx.setCursor(6,35); gfx.print("J/N :Octave    B/M:Transpose");
        gfx.setCursor(6,44); gfx.print("k/l :Volume    Z/X:Bend");
        gfx.setCursor(6,53); gfx.print("C   :Porta     ;/.:PAD Y-axis");
        gfx.setCursor(6,62); gfx.print(",// :PAD X-axis A/S:PAD hold");
        gfx.setCursor(6,71); gfx.print("Space:Seq Play/Stop");
    }
    gfx.setCursor(6,80); gfx.print("H   :This help (hold)");
    gfx.setCursor(6,89); gfx.print("Tab:Next Sh+Tab:Prev G0:SEQ");
    gfx.setCursor(6,99); gfx.print("release H to close");
}

void drawPlayScreen(bool full){
    // HELP overlay: keep using the original full-size canvas — this is a
    // rare, toggle-triggered case, not worth splitting.
    if(helpVisible){
        canvas.startWrite();
        if(full){
            canvas.fillScreen(BLACK);
            canvas.setTextColor(uiColor,BLACK);
        }
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }
    bool forceFullBoth=full; // caller already folds in "help just closed" via modeChanged||forceFullRedraw

    // ---- TOP (y=0-54): tab bar + waveform. Only pushed when the tab bar
    // needs a full redraw or the waveform shape actually changed — most
    // of the time nothing is modulating Timbre/PWM, so this skips a
    // meaningful chunk (26KB) of the old single 63KB transfer entirely.
    static float dispTimbreMorph=lastModMorph, dispPwmWidth=lastModPwm;
    dispTimbreMorph+=(lastModMorph-dispTimbreMorph)*0.15f;
    dispPwmWidth+=(lastModPwm-dispPwmWidth)*0.15f;
    static float lastDrawnMorph=-999.f, lastDrawnPwm=-999.f;
    bool topDirty=forceFullBoth||fabsf(dispTimbreMorph-lastDrawnMorph)>0.004f||fabsf(dispPwmWidth-lastDrawnPwm)>0.002f;
    if(topDirty){
        canvasTop.startWrite();
        if(forceFullBoth){
            canvasTop.fillScreen(BLACK);
            canvasTop.setTextColor(uiColor,BLACK);
            drawTabBar(canvasTop,AppMode::PLAY);
        }
        drawWaveform(canvasTop,dispTimbreMorph,dispPwmWidth);
        lastDrawnMorph=dispTimbreMorph;lastDrawnPwm=dispPwmWidth;
        canvasTop.endWrite();
        canvasTop.pushSprite(0,0);
    }

    // ---- NAME (x=0-73, y=55-112): note info, O/T, P/H. Always pushed
    // when this function runs — this is literally what changes on every
    // note keypress, so there's no meaningful dirty-check to skip it with.
    canvasName.startWrite();
    if(forceFullBoth){
        canvasName.fillScreen(BLACK);
        canvasName.setTextColor(uiColor,BLACK);
        canvasName.drawFastHLine(0,55-BOTTOM_Y_OFFSET,74,uiColor); // this canvas's own segment of the waveform/info divider
    }
    canvasName.fillRect(0,56-BOTTOM_Y_OFFSET,73,57,BLACK);
    canvasName.drawRect(0,56-BOTTOM_Y_OFFSET,73,57,uiColor);
    if(arpEnabled&&arpHeldCount>0){
        // Arp is running: list every held note (press order), small text,
        // highlighting whichever one is currently sounding.
        canvasName.setTextSize(1);
        int x=4,y=60-BOTTOM_Y_OFFSET;
        for(int i=0;i<arpHeldCount;i++){
            char nm[8];
            snprintf(nm,sizeof(nm),"%s",getNoteName(arpHeldFreqs[i]));
            bool isCurrent=fabsf(arpHeldFreqs[i]-arpLastTriggeredFreq)<0.5f;
            int w=(int)strlen(nm)*6+3;
            if(x+w>70){x=4;y+=9;}
            if(y>80-BOTTOM_Y_OFFSET)break; // out of room in this box
            if(isCurrent){
                canvasName.fillRect(x-1,y-1,w,9,uiColor);
                canvasName.setTextColor(BLACK,uiColor);
            } else {
                canvasName.setTextColor(uiColor,BLACK);
            }
            canvasName.setCursor(x,y);
            canvasName.print(nm);
            x+=w;
        }
        canvasName.setTextColor(uiColor,BLACK);
    } else {
        float df=playingFreq>0?playingFreq:currentFreq;
        canvasName.setTextSize(2);
        canvasName.setCursor(4,60-BOTTOM_Y_OFFSET);
        canvasName.printf("%-4s",getNoteName(df));
        canvasName.setTextSize(1);
        canvasName.setCursor(4,84-BOTTOM_Y_OFFSET);
        if(df>0)canvasName.printf("%-9s",(String(df,1)+"Hz").c_str());
        else    canvasName.print("---      ");
    }
    canvasName.setTextSize(1);
    canvasName.setCursor(4,95-BOTTOM_Y_OFFSET);
    canvasName.printf("O:%+d T:%+d %c",params.octaveShift,transposeSemitones,playMode==PlayMode::PRO?'P':'E');
    canvasName.setCursor(4,104-BOTTOM_Y_OFFSET);
    canvasName.printf("P:%-3s H:%-3s",portaEnabled?"ON":"off",noteHeld?"ON":"off");
    canvasName.drawFastHLine(0,112-BOTTOM_Y_OFFSET,73,uiColor); // note block's own bottom border
    canvasName.endWrite();
    canvasName.pushSprite(0,BOTTOM_Y_OFFSET);

    // ---- IMU (x=73-240, y=55-112): bend meter, IMU pad+readout, volume.
    // Only pushed when something in it actually changed — this is the
    // main win: playing notes with IMU=None (and Bend/Volume untouched)
    // skips this ~27KB region entirely.
    static float lastPushedBend=-9999.f, lastPushedVol=-9999.f;
    static float lastPushedImuX=-9999.f, lastPushedImuY=-9999.f;
    static float lastPushedAccelX=-9999.f, lastPushedAccelY=-9999.f;
    float curBend=params.pitchBendCents+keyBendCurrent;
    float curVol=params.keyVolume;
    float curImuX=(imuAxisX.target!=ImuTarget::NONE)?getImuNorm(imuAxisX.target):0.f;
    float curImuY=(imuAxisY.target!=ImuTarget::NONE)?getImuNorm(imuAxisY.target):0.f;
    // The pad DOT's position tracks raw tilt (or the virtual PAD axes on
    // original Cardputer) directly, independent of whether any IMU target
    // is even assigned — curImuX/Y above don't capture this at all (they
    // stay 0 with target=None), so without also checking the raw input,
    // the dot only ever moved when something ELSE (bend/volume/a target
    // value) also happened to change, freezing otherwise.
    float curAccelX=isCardputerAdv?lastAccelX:padVirtualX;
    float curAccelY=isCardputerAdv?lastAccelY:padVirtualY;
    bool imuDirty=forceFullBoth
        ||fabsf(curBend-lastPushedBend)>1.0f
        ||fabsf(curVol-lastPushedVol)>0.001f
        ||fabsf(curImuX-lastPushedImuX)>0.01f
        ||fabsf(curImuY-lastPushedImuY)>0.01f
        ||fabsf(curAccelX-lastPushedAccelX)>0.01f
        ||fabsf(curAccelY-lastPushedAccelY)>0.01f;
    if(imuDirty){
        canvasImu.startWrite();
        if(forceFullBoth){
            canvasImu.fillScreen(BLACK);
            canvasImu.setTextColor(uiColor,BLACK);
            canvasImu.drawFastHLine(0,55-BOTTOM_Y_OFFSET,167,uiColor); // this canvas's own segment of the waveform/info divider
        }
        drawBendMeter(canvasImu,curBend,keyBendMaxCents,-BOTTOM_Y_OFFSET,-IMU_X_OFFSET);
        drawImuPad(canvasImu,-BOTTOM_Y_OFFSET,-IMU_X_OFFSET);

        constexpr int TX=152-IMU_X_OFFSET,TW=88;
        canvasImu.fillRect(TX,56-BOTTOM_Y_OFFSET,TW,57,BLACK);
        canvasImu.setCursor(TX,57-BOTTOM_Y_OFFSET);
        canvasImu.printf("X:%-10s",imuTargetName(imuAxisX.target));
        canvasImu.setCursor(TX,66-BOTTOM_Y_OFFSET);
        {
            String xVal=getImuValStr(imuAxisX.target);
            if(imuXHeld)xVal+="(HOLD)";
            canvasImu.printf("%-12s",xVal.c_str());
        }
        if(imuAxisX.target!=ImuTarget::NONE){
            canvasImu.fillRect(TX,75-BOTTOM_Y_OFFSET,TW-2,4,canvasImu.color565(0,64,0));
            if((int)(curImuX*(TW-4))>0)canvasImu.fillRect(TX,75-BOTTOM_Y_OFFSET,(int)(curImuX*(TW-4)),4,uiColor);
        }
        canvasImu.setCursor(TX,81-BOTTOM_Y_OFFSET);
        canvasImu.printf("Y:%-10s",imuTargetName(imuAxisY.target));
        canvasImu.setCursor(TX,90-BOTTOM_Y_OFFSET);
        {
            String yVal=getImuValStr(imuAxisY.target);
            if(imuYHeld)yVal+="(HOLD)";
            canvasImu.printf("%-12s",yVal.c_str());
        }
        if(imuAxisY.target!=ImuTarget::NONE){
            canvasImu.fillRect(TX,99-BOTTOM_Y_OFFSET,TW-2,4,canvasImu.color565(0,64,0));
            if((int)(curImuY*(TW-4))>0)canvasImu.fillRect(TX,99-BOTTOM_Y_OFFSET,(int)(curImuY*(TW-4)),4,uiColor);
        }
        canvasImu.setCursor(TX,105-BOTTOM_Y_OFFSET);
        canvasImu.printf("VOL:%d%% BND:%dst  ",
            (int)(params.keyVolume*100),(int)(keyBendMaxCents/100));

        canvasImu.endWrite();
        canvasImu.pushSprite(IMU_X_OFFSET,BOTTOM_Y_OFFSET);
        lastPushedBend=curBend;lastPushedVol=curVol;lastPushedImuX=curImuX;lastPushedImuY=curImuY;
        lastPushedAccelX=curAccelX;lastPushedAccelY=curAccelY;
    }

    // ---- NAV (x=0-240, y=113-134): full-width nav divider, scale name,
    // nav text. Only redrawn/pushed on forceFullBoth — this rarely changes.
    if(forceFullBoth){
        canvasNav.startWrite();
        canvasNav.fillScreen(BLACK);
        canvasNav.setTextColor(uiColor,BLACK);
        canvasNav.drawFastHLine(0,0,240,uiColor); // full-width nav divider (absolute y=113)
        if(playMode==PlayMode::PRO){
            char scaleBuf[40];
            snprintf(scaleBuf,sizeof(scaleBuf),"%s: %s",
                SCALE_CATEGORY_NAMES[SCALES[currentScaleIndex].category],
                SCALES[currentScaleIndex].name);
            canvasNav.setCursor((240-(int)strlen(scaleBuf)*6)/2,115-NAV_Y_OFFSET);
            canvasNav.print(scaleBuf);
        }
        const char *nav="Tab:MENU  H:HELP  G0:SEQ mode";
        canvasNav.setCursor((240-(int)strlen(nav)*6)/2,124-NAV_Y_OFFSET);
        canvasNav.print(nav);
        canvasNav.endWrite();
        canvasNav.pushSprite(0,NAV_Y_OFFSET);
    }
}

void drawSeqScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillScreen(BLACK);
        canvas.setTextColor(uiColor,BLACK);
        drawTabBar(canvas,AppMode::SEQ);
        if(playMode==PlayMode::PRO){
            char scaleBuf[40];
            snprintf(scaleBuf,sizeof(scaleBuf),"%s: %s",
                SCALE_CATEGORY_NAMES[SCALES[currentScaleIndex].category],
                SCALES[currentScaleIndex].name);
            canvas.setCursor((240-(int)strlen(scaleBuf)*6)/2,115);
            canvas.print(scaleBuf);
        }
    }

    if(helpVisible){
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }

    // Step grid, where PLAY would show the waveform (SEQ has no waveform).
    // Velocity is shown as a bottom-aligned bar (taller = louder). Tied
    // runs of steps merge into one shape (thick outer border, no internal
    // vertical border at the join) so it reads as "one long note", while
    // each step's own bar segment stays visible (drawn using the run's
    // starting note's velocity) so the playhead/cursor can still pick out
    // individual steps within the run. Accent = pointed/triangle top
    // instead of the usual flat top. Slide = small diagonal notch at the
    // bottom-left corner, showing the pitch "sliding in" from the left.
    canvas.fillRect(0,12,240,43,BLACK);
    constexpr int GY=15,GH=36,STEP_W=15,BOX_W=13;

    // Pass 1: figure out which steps are part of a "sounding" run (a
    // note-on step followed by zero or more ties extending it), what
    // velocity each should display (the run's own starting velocity),
    // and whether the run is accented (also from its starting step).
    int runStart[SEQ_NUM_STEPS];
    uint8_t effVel[SEQ_NUM_STEPS];
    bool effAccent[SEQ_NUM_STEPS];
    {
        int curStart=-1; uint8_t curVel=100; bool curAccent=false;
        for(int i=0;i<SEQ_NUM_STEPS;i++){
            SeqStep &s=seqSteps[i];
            if(s.freq>0.f){ curStart=i; curVel=s.velocity; curAccent=s.accent; runStart[i]=i; effVel[i]=curVel; effAccent[i]=curAccent; }
            else if(s.tie&&curStart>=0){ runStart[i]=curStart; effVel[i]=curVel; effAccent[i]=curAccent; }
            else { runStart[i]=-1; effVel[i]=0; effAccent[i]=false; curStart=-1; }
        }
    }

    for(int i=0;i<SEQ_NUM_STEPS;i++){
        int x=i*STEP_W;
        SeqStep &gs=seqSteps[i];
        bool isCursor=(i==seqCursorStep);
        bool isPlayhead=(seqPlaying&&i==seqPlayStep);
        bool inRun=(runStart[i]>=0);
        bool joinLeft=inRun&&i>0&&runStart[i-1]==runStart[i];
        bool joinRight=inRun&&i<SEQ_NUM_STEPS-1&&runStart[i+1]==runStart[i];
        int thick=inRun?2:1;
        uint16_t lineColor=isCursor?WHITE:seqAccentColor;
        uint16_t barColor=isPlayhead?WHITE:(effAccent[i]?seqAccentNoteColor:seqAccentColor);
        int bx=x+1,by=GY,bw=BOX_W,bh=GH;

        canvas.fillRect(bx,by,bw,bh,BLACK);
        if(inRun){
            int barH=max(1,(int)(bh*(effVel[i]/100.0f)));
            canvas.fillRect(bx,by+bh-barH,bw,barH,barColor);
        }

        for(int t=0;t<thick;t++){
            canvas.drawFastHLine(bx,by+t,bw,lineColor);
            canvas.drawFastHLine(bx,by+bh-1-t,bw,lineColor);
        }
        if(!joinLeft) for(int t=0;t<thick;t++)canvas.drawFastVLine(bx+t,by,bh,lineColor);
        if(!joinRight)for(int t=0;t<thick;t++)canvas.drawFastVLine(bx+bw-1-t,by,bh,lineColor);

        if(gs.slide){
            canvas.drawLine(bx,by+bh-1,bx+5,by+bh-6,WHITE);
            canvas.drawLine(bx,by+bh-2,bx+5,by+bh-7,WHITE);
        }
        // Selection: a small yellow strip above the box, for steps within
        // the current Copy/Cut range (marking or confirmed alike).
        if(seqSelStart>=0&&i>=seqSelStart&&i<=seqSelEnd){
            canvas.fillRect(bx,by-3,bw,2,canvas.color565(255,255,0));
        }
        // Cursor: small inset outline, drawn last so it stays visible even
        // on a step whose own left/right border was skipped (tie-joined).
        if(isCursor)canvas.drawRect(bx+1,by+1,bw-2,bh-2,WHITE);
    }
    canvas.drawFastHLine(0,55,240,uiColor);

    // Left block (matches PLAY's note-info box position/size): everything
    // is always visible — step/note, Velocity+flags, Tempo+Swing,
    // Octave+Transpose, Portamento+Hold (matching PLAY's own "O:/T:" and
    // "P:/H:" lines exactly, since both are active in SEQ too), and a
    // single "Ed:" indicator showing which value ,/. currently adjusts,
    // plus Play state.
    canvas.fillRect(0,56,73,57,BLACK);
    canvas.drawRect(0,56,73,57,uiColor);
    SeqStep &st=seqSteps[seqCursorStep];
    canvas.setCursor(4,58);
    canvas.printf("S%2d %-4s",seqCursorStep+1,getNoteName(st.freq));
    canvas.setCursor(4,67);
    canvas.printf("V%3d%% %s%s%s",st.velocity,
        st.tie?"T":" ",st.slide?"S":" ",st.accent?"A":" ");
    canvas.setCursor(4,76);
    canvas.printf("B%3.0f S%+3.0f%%",seqTempoBpm,seqSwing);
    canvas.setCursor(4,85);
    canvas.printf("O:%+d T:%+d",params.octaveShift,transposeSemitones);
    canvas.setCursor(4,94);
    canvas.printf("P:%-3s H:%-3s",portaEnabled?"ON":"off",noteHeld?"ON":"off");
    canvas.setCursor(4,103);
    const char *editLabel=(seqFocus==SeqFocus::STEP)
        ?(seqStepTarget==SeqStepTarget::VELOCITY?"Vel":seqStepTarget==SeqStepTarget::TIE?"Tie":seqStepTarget==SeqStepTarget::SLIDE?"Sld":"Acc")
        :(seqPatternTarget==SeqPatternTarget::TEMPO?"Bpm":"Swg");
    canvas.printf("Ed:%-3s %s",editLabel,seqPlaying?"PLAY":"STOP");

    // Right block: IMU pad, reused directly from PLAY — same targets,
    // same physical tilt/PAD input, works identically here.
    drawImuPad(canvas);
    drawBendMeter(canvas,params.pitchBendCents+keyBendCurrent,keyBendMaxCents);
    constexpr int TX=152,TW=88;
    canvas.fillRect(TX,56,TW,57,BLACK);
    canvas.setCursor(TX,57);
    canvas.printf("X:%-10s",imuTargetName(imuAxisX.target));
    canvas.setCursor(TX,66);
    {
        String xVal=getImuValStr(imuAxisX.target);
        if(imuXHeld)xVal+="(HOLD)";
        canvas.printf("%-12s",xVal.c_str());
    }
    if(imuAxisX.target!=ImuTarget::NONE){
        static float seqDispNormX=0.f;
        float n=constrain(getImuNorm(imuAxisX.target),0.f,1.f);
        seqDispNormX+=(n-seqDispNormX)*0.3f;
        canvas.fillRect(TX,75,TW-2,4,canvas.color565(0,64,0));
        if((int)(seqDispNormX*(TW-4))>0)canvas.fillRect(TX,75,(int)(seqDispNormX*(TW-4)),4,uiColor);
    }
    canvas.setCursor(TX,81);
    canvas.printf("Y:%-10s",imuTargetName(imuAxisY.target));
    canvas.setCursor(TX,90);
    {
        String yVal=getImuValStr(imuAxisY.target);
        if(imuYHeld)yVal+="(HOLD)";
        canvas.printf("%-12s",yVal.c_str());
    }
    if(imuAxisY.target!=ImuTarget::NONE){
        static float seqDispNormY=0.f;
        float n=constrain(getImuNorm(imuAxisY.target),0.f,1.f);
        seqDispNormY+=(n-seqDispNormY)*0.3f;
        canvas.fillRect(TX,99,TW-2,4,canvas.color565(0,64,0));
        if((int)(seqDispNormY*(TW-4))>0)canvas.fillRect(TX,99,(int)(seqDispNormY*(TW-4)),4,uiColor);
    }
    canvas.setCursor(TX,105);
    canvas.printf("VOL:%d%%  ",(int)(params.keyVolume*100));

    canvas.drawFastHLine(0,112,73,uiColor);
    canvas.drawFastHLine(0,113,240,uiColor);
    const char *nav="Tab:MENU  H:HELP  G0:PLAY mode";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,124);
    canvas.print(nav);

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// ==========================================================
// setup / loop
// ==========================================================
void setup(){
    Serial.begin(921600);
    randomSeed(esp_random());
    auto cfg=M5.config();
    M5Cardputer.begin(cfg,true);
    isCardputerAdv = (M5.getBoard() == m5::board_t::board_M5CardputerADV);
    refreshSettingItems();
    Serial.printf("[Board] %s\n", isCardputerAdv?"CardputerADV":"Cardputer (original)");
    M5Cardputer.Display.setRotation(1);
    seqAccentColor = M5Cardputer.Display.color565(255,140,0); // orange
    seqAccentNoteColor = M5Cardputer.Display.color565(220,30,30); // red — accented steps' velocity bar
    songAccentColor = M5Cardputer.Display.color565(0,220,220); // cyan — SONG mode's own fixed UI color
    // Bank A-H timeline colors: a spread across the color wheel so each
    // bank reads as visually distinct at a glance.
    songBankColors[0]=M5Cardputer.Display.color565(220,60,60);   // A red
    songBankColors[1]=M5Cardputer.Display.color565(230,140,40);  // B orange
    songBankColors[2]=M5Cardputer.Display.color565(220,210,50);  // C yellow
    songBankColors[3]=M5Cardputer.Display.color565(90,210,90);   // D green
    songBankColors[4]=M5Cardputer.Display.color565(60,190,190);  // E teal
    songBankColors[5]=M5Cardputer.Display.color565(80,130,230);  // F blue
    songBankColors[6]=M5Cardputer.Display.color565(160,90,220);  // G purple
    songBankColors[7]=M5Cardputer.Display.color565(220,90,170);  // H pink
    M5Cardputer.Display.setTextColor(uiColor,BLACK);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("C.P.S. CardPuter Synth",10,10);

    canvas.setColorDepth(16);
    // PSRAM: tried internal SRAM as an experiment (v0.9363) to rule out
    // PSRAM-bus contention as the cause of an audible crackle, but it
    // made no difference — the crackle correlated with redraw FREQUENCY
    // during rapid key input, not where the canvas lives (see the
    // MIN_REDRAW_MS throttle below, added to address the real cause).
    // Back to PSRAM so internal SRAM isn't needlessly spent on this.
    canvas.setPsram(true);
    if(!canvas.createSprite(240,135)){
        Serial.println("[Canvas] createSprite FAILED — PLAY/SEQ will show a blank screen until this is fixed");
    }
    canvas.setTextSize(1);

    canvasTop.setColorDepth(16);
    canvasTop.setPsram(true);
    if(!canvasTop.createSprite(240,55)){
        Serial.println("[CanvasTop] createSprite FAILED — PLAY's top region will be blank until this is fixed");
    }
    canvasTop.setTextSize(1);

    canvasName.setColorDepth(16);
    canvasName.setPsram(true);
    if(!canvasName.createSprite(74,58)){ // x=0-73, y=55-112
        Serial.println("[CanvasName] createSprite FAILED — PLAY's note-name region will be blank until this is fixed");
    }
    canvasName.setTextSize(1);

    canvasImu.setColorDepth(16);
    canvasImu.setPsram(true);
    if(!canvasImu.createSprite(167,58)){ // x=73-240, y=55-112
        Serial.println("[CanvasImu] createSprite FAILED — PLAY's IMU/bend region will be blank until this is fixed");
    }
    canvasImu.setTextSize(1);

    canvasNav.setColorDepth(16);
    canvasNav.setPsram(true);
    if(!canvasNav.createSprite(240,22)){ // x=0-240, y=113-134
        Serial.println("[CanvasNav] createSprite FAILED — PLAY's nav/scale text will be blank until this is fixed");
    }
    canvasNav.setTextSize(1);

    buildWaveTables();
    updateFilterCoefficients();

    bool sdOk=initSDCard();
    if(sdOk){ensureCpsFolder();ensurePatchFolder();loadSettings();Serial.println("[SD] OK");}
    else Serial.println("[SD] not found");

    recomputeKeyNotes(); // build the note key frequency tables from the loaded (or default) play mode/scale

    // lastModMorph/lastModPwm only get updated during active note playback
    // (see audioTask), so without this they'd sit at their compile-time
    // defaults — showing a default Sine waveform on the MAIN screen —
    // until the first note was played after boot/load.
    lastModMorph=params.timbreMorph;
    lastModPwm=constrain(params.pwmWidth+params.pwmOffset,0.1f,0.9f);

    bool imuOk=M5.Imu.begin();
    Serial.println(imuOk?"[IMU] OK":"[IMU] not found");

    auto sc=M5Cardputer.Speaker.config();
    sc.sample_rate=SAMPLE_RATE;sc.dma_buf_count=8; // was 4 — more queued headroom so
    // occasional delays in the Speaker's own I2S-feed task (pinned to the
    // same core as the display's SPI-DMA activity, see below) don't force
    // audioTask's playRaw() call to wait as long for a free buffer slot,
    // which showed up as "over budget" buffers correlated with screen
    // redraws (e.g. a key press) — an audible click. Costs a bit more
    // fixed audio latency in exchange for resilience; worth revisiting if
    // the latency becomes noticeable.
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
bool lastImuPickerOpen=false;
bool lastImuCalibrateConfirmOpen=false;
bool lastResetConfirmOpen=false;
bool lastScalePickerOpen=false;
int  lastScalePickerLevel=0;
bool lastHelpVisible=false;

void loop(){
    M5Cardputer.update();

    if(diagPrintPending){
        Serial.printf("[audioTask] %lu buffers, avg %lu us, max %lu us, %lu over budget (budget 23220us)\n",
            diagPrintCount,diagPrintCount?diagPrintSumUs/diagPrintCount:0,diagPrintMaxUs,diagPrintOverCount);
        diagPrintPending=false;
    }

    // G0 button (BtnA): short press toggles PLAY<->SEQ (unchanged
    // behavior, just now resolved on release instead of press-down, so
    // it can be distinguished from a long press); long press (500ms)
    // enters/exits SONG mode instead. Physically separate from the
    // keyboard matrix, so it can't be accidentally triggered while
    // playing/typing.
    constexpr uint32_t G0_LONG_PRESS_MS=500;
    static bool g0LongFired=false;
    if(M5Cardputer.BtnA.isPressed()&&!g0LongFired&&M5Cardputer.BtnA.pressedFor(G0_LONG_PRESS_MS)){
        g0LongFired=true;
        if(appMode==AppMode::SONG){
            appMode=lastMainMode; // back to whichever was home
        } else {
            appMode=AppMode::SONG;
        }
    }
    if(M5Cardputer.BtnA.wasReleased()){
        if(!g0LongFired){
            // Short press: existing PLAY<->SEQ toggle, unchanged.
            lastMainMode=(lastMainMode==AppMode::PLAY)?AppMode::SEQ:AppMode::PLAY;
            refreshSettingItems();
            if(appMode==AppMode::SETTINGS||appMode==AppMode::SEQ)saveSettings();
            appMode=lastMainMode;
            currentFreq=0;
            // PLAY and SEQ are distinct modes — switching between them should
            // silence whatever was sounding rather than carrying it over.
            if(seqPlaying){seqPlaying=false;songPlaying=false;seqSliding=false;seqAccentCutoffBoostTarget=0.f;seqAccentResoBoostTarget=0.f;seqVelocityMult=1.0f;}
            arpHeldCount=0;
            arpLatchedCount=0; // Latch mode rebuilds arpHeldCount from this every update, so it must be cleared too
            noteHeld=false;heldFreq=0.f;
        }
        g0LongFired=false;
    }

    updateImu();
    updateMenuNavigation();
    if(appMode==AppMode::PATCH)updatePatchBrowser();
    if(appMode==AppMode::PATTERN)updatePatternBank();
    if(appMode==AppMode::SONG)updateSongEditor();
    if(appMode==AppMode::CATEGORY&&imuPickerOpen)updateImuPicker();
    if(appMode==AppMode::CATEGORY&&imuCalibrateConfirmOpen)updateImuCalibrateConfirm();
    if(appMode==AppMode::CATEGORY&&resetConfirmOpen)updateResetConfirm();
    if(appMode==AppMode::CATEGORY&&scalePickerOpen)updateScalePicker();
    if(appMode==AppMode::SEQ)updateSeqEditing();

    bool keyChanged=M5Cardputer.Keyboard.isChange();
    // Notes can be triggered on every screen except the Patch Bank and
    // SEQ itself (which has its own dedicated note-entry key handling).
    // Every other screen (VCO/VCF/VCA/LFO/SETTINGS/CATEGORY, including
    // all of CATEGORY's own sub-screens/overlays) only uses ;/./,// for
    // its own navigation, so there's no key conflict — and it means
    // tone/filter/LFO changes can be heard live while editing, not just
    // on PLAY.
    bool notesAllowed=(appMode!=AppMode::PATCH&&appMode!=AppMode::SEQ&&appMode!=AppMode::PATTERN&&appMode!=AppMode::SONG);
    if(keyChanged){
        updateOctaveAndVolume();
        if(seqPlaying){
            // Sequencer has exclusive control of currentFreq while
            // playing (see updateSeqTiming(), called unconditionally
            // below so the pattern keeps looping even on other screens)
            // — don't let normal note-triggering or Arp fight it.
        } else if(notesAllowed&&arpEnabled){
            // Arp mode: track the held chord here; updateArpTiming() below
            // (which runs every loop iteration, not just on keyChanged)
            // drives currentFreq/envelope retriggering from it.
            updateArpHeldNotes();
        } else if(notesAllowed){
            float nf=resolveFreqFromKeys();
            if(nf>0&&currentFreq==0){
                if(envPhase==EnvPhase::IDLE)envLevel=0;
                envPhase=EnvPhase::ATTACK;
                filterEnvPhase=EnvPhase::ATTACK;  // trigger filter envelope
                if(portaEnabled&&portaFreq==0)portaFreq=nf;
            }
            currentFreq=nf;
        } else if(appMode==AppMode::SEQ){
            // Handled by updateSeqEditing() above (note preview + auto-
            // advance) — don't reset currentFreq here, or the preview
            // note gets zeroed the instant it's set.
        } else {
            currentFreq=0;
        }
    }
    if(!seqPlaying&&notesAllowed&&arpEnabled)updateArpTiming();
    if(seqPlaying)updateSeqTiming();

    // Computed here (after all of this frame's mode-changing input —
    // G0, Tab-cycle, SONG/PATCH/PATTERN's own Tab handling — has already
    // been processed above), not at the top of loop(), so a full redraw
    // triggered by a mode change this same frame already sees the RIGHT
    // color instead of yesterday's.
    uiColor=(appMode==AppMode::SONG)?songAccentColor:((lastMainMode==AppMode::SEQ)?seqAccentColor:GREEN);

    bool modeChanged=(appMode!=lastDrawnMode);
    lastDrawnMode=appMode;

    // Non-PLAY screens: only redraw on menu keys or mode change
    bool menuKey=false;
    if(appMode!=AppMode::PLAY&&keyChanged){
        auto st=M5Cardputer.Keyboard.keysState();
        for(char c:st.word)if(c==';'||c=='.'||c==','||c=='/')menuKey=true;
        if(st.tab)menuKey=true;
    }

    // Minimum interval between forced (key-triggered) pushSprite() calls —
    // rapid key repeats/menu navigation could otherwise trigger a full
    // 240x135 canvas push on nearly every loop() iteration, which showed
    // up as an audible "crackle" (occasional audioTask buffers running
    // over budget, correlated with key presses). modeChanged always
    // bypasses this so screen transitions still feel instant.
    constexpr unsigned long MIN_REDRAW_MS=30;
    unsigned long nowMs0=millis();
    bool canForceRedraw=modeChanged||(nowMs0-lastDisplayMs)>=MIN_REDRAW_MS;

    if(appMode==AppMode::VCO){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcoScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::VCF){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcfScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::VCA){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcaScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::LFO){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawLfoScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::SETTINGS){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawSettingsScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::CATEGORY){
        bool pickerChanged=(imuPickerOpen!=lastImuPickerOpen);
        lastImuPickerOpen=imuPickerOpen;
        bool calChanged=(imuCalibrateConfirmOpen!=lastImuCalibrateConfirmOpen);
        lastImuCalibrateConfirmOpen=imuCalibrateConfirmOpen;
        bool resetChanged=(resetConfirmOpen!=lastResetConfirmOpen);
        lastResetConfirmOpen=resetConfirmOpen;
        bool scaleChanged=(scalePickerOpen!=lastScalePickerOpen)||(scalePickerLevel!=lastScalePickerLevel);
        lastScalePickerOpen=scalePickerOpen;lastScalePickerLevel=scalePickerLevel;
        bool full=modeChanged||pickerChanged||calChanged||resetChanged||scaleChanged;
        unsigned long now=millis();
        bool doRedraw=full||((menuKey||(now-lastDisplayMs)>=100)&&canForceRedraw);
        if(doRedraw){
            lastDisplayMs=now;
            if(imuPickerOpen)drawImuPickerScreen(full);
            else if(imuCalibrateConfirmOpen)drawImuCalibrateConfirmScreen(full);
            else if(resetConfirmOpen)drawResetConfirmScreen(full);
            else if(scalePickerOpen)drawScalePickerScreen(full);
            else drawCategoryScreen(full);
        }
        delay(5);return;
    }
    if(appMode==AppMode::PATCH){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawPatchScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::PATTERN){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawPatternBankScreen(modeChanged);}
        delay(5);return;
    }
    if(appMode==AppMode::SONG){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawSongScreen(modeChanged);}
        delay(5);return;
    }
    bool helpChanged=(helpVisible!=lastHelpVisible);
    lastHelpVisible=helpVisible;
    bool forceFullRedraw=(helpChanged&&!helpVisible);

    if(appMode==AppMode::SEQ){
        unsigned long now=millis();
        if(((keyChanged||helpChanged)&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){
            lastDisplayMs=now;
            drawSeqScreen(modeChanged||forceFullRedraw);
        }
        delay(5);return;
    }

    // PLAY screen
    unsigned long now=millis();
    if(((keyChanged||helpChanged)&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){
        lastDisplayMs=now;
        drawPlayScreen(modeChanged||forceFullRedraw);
    }
    delay(5);
}
