# C.P.S. — CardPuter Synth

*[日本語版はこちら / Japanese version here](README_ja.md)*

A feature-rich DIY synthesizer for the **M5Stack Cardputer family** (CardputerADV / original Cardputer), built with PlatformIO and the Arduino framework.

I share my ideas with Claude and have it write the code.
I share development progress on Reddit and my [Twitter (X) account](https://x.com/Tokagetchi) — community feedback has been the spark for many of the features below!
Thank you so much to everyone who's shown interest in this project!

> **This project is currently 100% vibe-coded.**
> My apologies to anyone who isn't comfortable with AI use or vibe coding.

---

## 🎉 v1.0 Official Release

From v0.9 to this v1.0 release, development has focused on both broadening the range of sounds you can build and expanding how you can perform with them.

### Documentation

New to C.P.S.? Start with the **Quick Start Guide**. Looking up a specific feature? Check the **Reference Guide**. Both are available in Japanese and English, as Markdown and PDF.

| Guide | Japanese | English |
|---|---|---|
| Quick Start Guide | [Markdown](docs/QuickStart_ja.md) / [PDF](docs/QuickStart_ja.pdf) | [Markdown](docs/QuickStart_en.md) / [PDF](docs/QuickStart_en.pdf) |
| Reference Guide | [Markdown](docs/Reference_ja.md) / [PDF](docs/Reference_ja.pdf) | [Markdown](docs/Reference_en.md) / [PDF](docs/Reference_en.pdf) |

### Patch Pack

To get you playing right away, 10 factory patches are available in the [`Patch`](Patch) folder. Copy them into the `/CPS/Patch/` folder on your SD card to use them.

### Waveform library expanded (6 -> 12)

Alongside Sine / Triangle / Sawtooth / Square / Wavefolder / HalfSine, six new waveforms have been added: **Parabolic, ESaw, Squeeze, ESquare, Saw2, and Square2.** All are band-limited (anti-aliased).

### Shape (absorbs and extends the old PWM)

The PWM parameter has been redesigned as **Shape**, now supported across all 12 waveforms. It reshapes the sound continuously in a way that's specific to each waveform (duty cycle for square waves, ramp slope for sawtooth waves, and so on).

### Morphing

Register up to **10 patches into Morph slots** and switch between them instantly — or smoothly, over a set amount of time — with `Shift+1` through `Shift+0`.

### New FX tab (6 effects)

A new **FX** tab joins VCO/VCF/VCA/LFO. Select a pad and press `Enter` to toggle it on/off — stack as many effects together as you like.

| Effect | Description |
|---|---|
| Ring Modulator | Ring modulation |
| Soft Limiter | Soft limiting / saturation |
| Chorus | Chorus |
| Delay | Delay / echo |
| Reverb | Reverb |
| Bitcrush | Bit-crusher (moved here from being IMU-only) |

Ring Mod, Soft Limiter, Chorus, Delay, and Reverb can all be modulated via IMU/LFO.

### MIDI support

Connect to external MIDI gear via a separately sold serial MIDI unit (M5Stack Unit Midi). Supports notes, pitch bend, sustain pedal, mod wheel, Program Change, and MIDI Clock sync.

### Theremin (distance sensor) support

Connect a separately sold distance sensor (ToF) unit (M5Stack Unit ToF4M) for theremin-style, non-contact playing by moving your hand through the air.

### Tap Tempo

`Shift+Enter` lets you set the ARP/SEQ tempo on the fly while playing.

### Improved Help

Hold `H` to show the key commands available on the current screen; `Shift+H` latches the overlay in place.

### ARP, Portamento, and Bend decoupled from patches

These are now treated as performance-style settings rather than tone settings — they're unaffected by switching patches or by Randomize, and live as device-level settings instead.

### Automatic keyboard recovery

The Cardputer ADV's keyboard chip (TCA8418) has a known hardware quirk where fast key-presses can occasionally cause input to stop responding entirely (other engineers working with the same chip have reported the same behavior independently). C.P.S. now **detects this automatically and restarts on its own within about 30 seconds** to recover.

### Drift (a bonus feature)

With Pro Style active, SETTING > Play Mode now offers **Drift**. It's a playful recreation of the instability old analog synths had — pitch wanders slightly, the filter breathes, and volume creeps, each independently. The on-screen knobs themselves don't move; only the sound does. Turning it on shows a note about it on screen.

---

## 🆕 v0.9 Update

v0.9 opens up a whole new way to enjoy CPS beyond just playing live — building and listening to arrangements.
v0.9 opens up a whole new way to enjoy CPS beyond just playing live — building and listening to arrangements.

- **Added a Scale system**: in Pro Mode, choose from 49 scales across 9 categories (Chromatic / Classical / Symmetrical / Pentatonic / Japan / China / India / Middle East / Europe) via a 2-level picker, with live preview while you play
- **Added an Arpeggiator** (CardputerADV only): hold up to 6 notes as a chord, with UP / DOWN / UP-DOWN / AS PLAYED / RANDOM patterns and adjustable Tempo/Rate/Swing. `V` toggles Latch mode (hold notes without holding keys), `Shift+V` toggles the Arp on/off from any screen
- **Added a Step Sequencer (SEQ mode)**: a 16-step, TB-303-style sequencer. Beyond pitch and velocity, each step also supports **Tie** (extend the previous note), **Slide** (glide into the next pitch), and **Accent**. `G0` gives you a one-touch toggle between PLAY and SEQ. Steps can be copied, cut, and pasted
- **Added a Pattern Bank**: save/load your sequences across 8 banks (A–H) × 8 slots each, with a Random Pattern generator too
- **Added Song mode**: arrange saved patterns into a full song, with independent Transpose and Repeat count per pattern. Access it with a long press of `G0`. A timeline + mini-preview UI makes the arrangement easy to read at a glance
- **Reverse Tab-cycling**: `Shift+Tab` now cycles the menu backward
- Major optimization work on display flicker and audio processing, resulting in a noticeably smoother feel overall

---

## 🆕 v0.8 Update

v0.8 is a major overhaul focused on the IMU.

- **SETTING menu redesigned as category launchers**: Patch / IMU(PAD) / Bend / Portamento / Play Mode are now just entry points — selecting one opens its own dedicated screen
- **Expanded IMU targets to 17** (up from 10 — added PWM/Detune/Noise/SubLevel/Resonance/LFO Rate/LFO Depth). Target selection is now a scrollable list with category divider lines
- **Added fine per-axis IMU control**: Sensitivity, axis Invert, response Curve (Linear/Exponential), Deadzone, and Calibrate (ON/OFF toggle)
- **IMU Volume target is now a relative multiplier** of the current volume, so it can no longer exceed the set level
- **Patch Bank gained Reset and Randomize**: one-tap tone reset or full-parameter randomization, both behind a confirmation dialog
- **Bend and Portamento each got their own dedicated reset**
- **Added Play Mode (EZ / Pro)**: EZ Mode is a beginner-friendly diatonic layout, Pro Mode is a full chromatic layout with black keys. Switchable from the SETTING menu
- **Added original Cardputer support**: devices without an IMU get key-driven "PAD" control instead, auto-detected at boot
- Extensive investigation and optimization of audio dropouts/stutter, resulting in substantially improved stability

---

## 🆕 v0.7 Update

v0.7 significantly expanded the sound-editing capabilities.

- **Split the EDIT menu into VCO / VCF / VCA screens** (retiring the old single EDIT menu for a more "real synth" editing feel)
- Added a **sub oscillator** (-1oct / -2oct, adjustable level)
- Added **noise blend**
- Added **filter key tracking** (cutoff follows the played pitch)
- Added a **dedicated filter envelope** (Depth/Attack/Decay/Release)
- Added a **general-purpose LFO** as a new tab (Sine/Triangle/Sawtooth/Square, Rate 0.1–20Hz, Depth 0–100%, Target: Pitch/Volume/Timbre/Filter/PWM)
- Added a **"None" (bypass) filter type**
- Added a **Patch Bank**: save/recall every parameter as a named patch, with rename/duplicate/delete

---

## Features

| Category | Details |
|---|---|
| **Oscillator** | Real-time wavetable synthesis, **12 waveforms** (Sine / Triangle / Sawtooth / Square / Wavefolder / HalfSine / Parabolic / ESaw / Squeeze / ESquare / Saw2 / Square2). Choose up to 6 to morph between |
| **Shape** | Continuously reshapes the waveform in a way specific to each one (duty cycle on square waves, ramp slope on sawtooth, etc.). Supported on all 12 waveforms |
| **Second oscillator (Osc2)** | An independent oscillator — its own waveform, octave, and detune — blended with the main one |
| **Sub oscillator** | -1oct / -2oct, adjustable level |
| **Noise** | Noise blend (adjustable level) |
| **Keyboard (EZ Mode)** | `1234567890-=` + Backspace mapped to a 13-note diatonic scale; monophonic (single voice) |
| **Keyboard (Pro Mode)** | Two physical rows, each a complete chromatic octave including black keys. **49 selectable scales** |
| **Octave** | `;` / `.` keys shift ±2 octaves (`J`/`N` on original Cardputer) |
| **Transpose** | `,` / `/` keys shift ±12 semitones (`B`/`M` on original Cardputer) |
| **Volume** | `k` / `l` keys adjust in 1% steps, hold to accelerate (works on almost every screen except Patch Bank) |
| **Bend** | `Z` key = bend down, `X` key = bend up — guitar-choke feel with asymmetric attack/release. A device-level setting, not saved with the patch |
| **ADSR** | Full Attack/Decay/Sustain/Release envelope with retrigger support |
| **Biquad Filter** | LPF / HPF / BPF / Notch / None (bypass); configurable cutoff, Q, and key tracking |
| **Filter envelope** | Dedicated Depth/Attack/Decay/Sustain/Release envelope for the filter cutoff |
| **General-purpose LFO** | Sine/Triangle/Sawtooth/Square/Sample & Hold, Rate 0.1-20Hz, Depth 0-100%. Modulates pitch, volume, Shape, filter cutoff, and a wide range of FX parameters |
| **FX (6 effects)** | A dedicated tab for stacking Ring Modulator, Soft Limiter, Chorus, Delay, Reverb, and Bitcrush together. Several support IMU/LFO modulation |
| **Vibrato / Tremolo** | Periodic pitch/volume modulation |
| **Portamento** | ON/OFF, adjustable glide speed. A device-level setting, not saved with the patch, with its own dedicated reset |
| **Morphing** | Register up to 10 patches into Morph slots and recall them instantly (or smoothly, over time) with `Shift+1` through `Shift+0` |
| **Arpeggiator** (ADV only) | Up to 6-note chord hold. UP/DOWN/UP-DOWN/AS PLAYED/RANDOM, adjustable Tempo/Rate/Swing (1-unit steps, hold to accelerate), Latch mode. A device-level setting, not saved with the patch |
| **Step Sequencer (SEQ mode)** | 16-step, TB-303-style. Tie/Slide/Accent per step, with Copy/Cut/Paste |
| **Pattern Bank** | Save/load sequences across 8 banks x 8 slots, plus Random Pattern generation |
| **Song mode** | Arrange saved patterns into a song, with independent Transpose/Repeat per entry |
| **Tap Tempo** | `Shift+Enter` sets the ARP/SEQ tempo on the fly while playing |
| **MIDI** | Connect to external MIDI gear via a separately sold serial MIDI unit (M5Stack Unit Midi). Supports notes, pitch bend, sustain pedal, mod wheel, Program Change, and MIDI Clock sync |
| **Theremin (distance sensor)** | Connect a separately sold distance sensor (ToF) unit (M5Stack Unit ToF4M) for theremin-style, non-contact playing. Smooth or Semitone pitch modes |
| **Help** | Hold `H` to show the key commands for the current screen; `Shift+H` latches it in place |
| **IMU / PAD mapping** | A wide range of assignable targets; Sensitivity, axis Invert, response Curve, Deadzone, and Calibration adjustable per axis (Deadzone/Calibration unavailable on original Cardputer) |
| **Patch Bank** | Save/recall tone-related parameters as a named patch. Rename, duplicate, delete, reset, and randomize supported |
| **Play Mode** | EZ Mode (diatonic) / Pro Mode (chromatic, scale-selectable), switchable from the SETTING menu |
| **Automatic keyboard recovery** | If input stops responding due to a known hardware quirk in the Cardputer ADV's keyboard chip, C.P.S. detects it and restarts on its own within about 30 seconds |
| **SD settings** | Current settings auto-save to `/CPS/settings.json` |

### IMU / PAD assignable targets

`TIMBRE` · `VIBRATO_DEPTH` · `VIBRATO_RATE` · `TREMOLO` · `VOLUME` · `PITCH_BEND` · `BEND_UP` · `BEND_DOWN` · `BITCRUSH` · `FILTER_CUTOFF` · `SHAPE` (formerly PWM) · `DETUNE` · `NOISE` · `SUB_LEVEL` · `RESONANCE` · `LFO_RATE` · `LFO_DEPTH` · `RING_MOD_RATE` · `RING_MOD_MIX` · `LIMITER_DRIVE` · `CHORUS_DEPTH` · `CHORUS_MIX` · `DELAY_FEEDBACK` · `DELAY_MIX` · `REVERB_ROOM` · `REVERB_MIX` (+ `NONE`)

- **PITCH_BEND** — bipolar: tilt direction (or PAD press direction) controls bend direction
- **BEND_UP / BEND_DOWN** — absolute: always raises / lowers pitch
- **VOLUME** — a relative multiplier (0-100%) of the current volume; can only attenuate, never exceeds the set level
- **ArpTempo / ArpSwing** — controls the Arpeggiator's Tempo/Swing while PLAY is active, or the Sequencer's own Tempo/Swing while SEQ is active — the same axis assignment automatically applies to whichever is relevant
- **FX targets (Ring Mod / Limiter / Chorus / Delay / Reverb)** — disabled whenever the corresponding effect's Mix is 0 (off). Chorus Rate and Delay Time are excluded from modulation for sound-quality reasons

---

## Hardware

| Item | Value |
|---|---|
| Supported devices | M5Stack CardputerADV, original Cardputer (auto-detected at boot) |
| MCU | ESP32-S3 (dual-core Xtensa LX7, 240 MHz) |
| Audio | ES8311 codec + NS4150B amp (ADV), NS4168+SPM1423 (original), 1 W speaker, 3.5 mm jack |
| IMU | BMI270 6-axis (**CardputerADV only**) |
| SD slot | SPI — SCK=GPIO40, MISO=GPIO39, MOSI=GPIO14, CS=GPIO12 |

> **Regarding the original Cardputer (non-ADV / v1.1)**: auto-detected at boot, with key-driven "PAD" control substituting for the missing IMU. I don't have the hardware myself, so I haven't been able to verify this personally yet. Also, the original's keyboard hardware only officially supports **up to 3 simultaneous key presses** — pressing a 4th key at the same time may cause key ghosting (incorrect detection). This is a hardware limitation that can't be fully corrected in software. Bug reports and feedback from original-Cardputer owners are very welcome.

---

## Getting started

### Option 1 — Install via M5Burner (easiest, recommended)

No compiling required — the quickest way to get CPS on your device.

1. Download and install [M5Burner](https://docs.m5stack.com/en/uiflow/M5Burner) from the official site
2. Connect your Cardputer (ADV or original) to your computer via USB-C
3. Search for "C.P.S." (CardPuter Synth) inside M5Burner
4. Select the correct COM port and click "Burn"
5. Once flashing completes, CPS will launch automatically

> Inserting a FAT32-formatted SD card lets you use auto-save and the Patch Bank / Pattern Bank / Song features.

---

### Option 2 — Install via Launcher FW

If your CardputerADV runs **Launcher FW**, you can install CPS without building anything yourself.

#### Option 2a — Using OTA (recommended, easiest)

1. Boot into Launcher FW on your CardputerADV
2. Search for "C.P.S." (CardPuter Synth) via the OTA feature
3. Select the firmware that appears and download/install it
4. CPS will launch automatically once installed

#### Option 2b — Manually copying to the SD card

1. Go to the [Releases](../../releases) page and download the latest `.bin` file
2. Copy the `.bin` file to the **root of your SD card** (not inside a subfolder)
3. Insert the SD card into your CardputerADV and boot into Launcher FW
4. Navigate to the `.bin` file in the Launcher file browser and select it to flash
5. CPS will launch automatically after flashing

> A FAT32-formatted micro-SD card is required both for a Launcher install via Option 2b and for CPS's settings persistence.

---

### Option 3 — Build from source (PlatformIO)

#### Requirements

- [VSCode](https://code.visualstudio.com/) with the **PlatformIO IDE** extension
- M5Stack Cardputer (ADV or original) connected via USB-C

#### Build & flash

1. Clone or download this repository
2. Open the `CPS` folder in VSCode (`File › Open Folder`)
3. PlatformIO will auto-detect `platformio.ini` and download the required libraries on the first build
4. Click **Upload** (→ button in the bottom toolbar)

After a successful build, two files are generated in `.pio/build/cps/`:

| File | Purpose |
|---|---|
| `firmware.bin` | App binary only — used by PlatformIO's Upload button |
| `merge.bin` | **Merged** (bootloader + partitions + app) — use this for M5Burner or any single-file flash tool |

> **Boot-to-flash mode** (if upload fails): power off → hold G0 → power on → release G0.

### First boot (all install methods)

On first boot the app creates `/CPS/` on the SD card (along with the `Patch` / `Pattern` / `Song` subfolders) — a FAT32 micro-SD is required.
Settings save to `/CPS/settings.json` automatically whenever you leave the SEQ or SETTING screen.
If no SD card is present the app still runs with default settings.

---

## Controls

### Mode-switching quick reference

| Action | Result |
|---|---|
| `Tab` | Cycle menus forward (PLAY/SEQ → VCO → VCF → VCA → LFO → SETTING → PLAY/SEQ) |
| `Shift+Tab` | Cycle menus backward |
| `G0` (short press) | Toggle PLAY mode ⇔ SEQ mode |
| `G0` (long press, ~0.5s) | Enter Song mode (long press again to return) |

### PLAY screen

![PLAY mode keymap](images/play_keymap_en.svg)

| Key | Action |
|---|---|
| Note keys | Play notes (layout differs by EZ/Pro Mode — see Features above). Monophonic (single voice) — pressing multiple keys is mainly used to specify a chord for Arp |
| `;` / `.` (ADV), `J`/`N` (original) | Octave up / down (±2 octaves) |
| `,` / `/` (ADV), `B`/`M` (original) | Transpose down / up (±12 semitones) |
| `k` / `l` | Volume down / up (1% steps, hold to accelerate) |
| `Z` | Bend down (hold) |
| `X` | Bend up (hold) |
| `C` | Toggle portamento ON/OFF |
| `A` | Toggle IMU/PAD X-axis hold ON/OFF |
| `S` | Toggle IMU/PAD Y-axis hold ON/OFF |
| `Shift+A` | Enable/disable the IMU X-axis itself (ADV only) |
| `Shift+S` | Enable/disable the IMU Y-axis itself (ADV only) |
| `D` | Toggle note hold ON/OFF |
| `V` | Toggle Arp Latch ON/OFF (ADV only) |
| `Shift+V` | Toggle the Arpeggiator ON/OFF (ADV only, works from any screen) |
| `Shift+1` through `Shift+0` | Switch to Morph slot 1-10 |
| `Shift+Enter` | Tap Tempo |
| `Space` | Play/Stop the Sequencer's pattern (works even outside the SEQ screen) |
| `H` (hold) | Show help overlay |
| `Shift+H` | Latch the help overlay in place; press again to release |
| Tilt device (ADV) / `;`/`.`/`,`/`/` for PAD (original) | Controls whichever parameters are assigned to the IMU/PAD X/Y axes |

The PLAY screen shows the current note name/frequency, octave/transpose/portamento/note-hold state, a bend meter, IMU/PAD status, and the IMU/PAD X/Y target names with their current values — appending **(HOLD)** whenever that axis is held. While the Arpeggiator is on, every held note is listed instead.

### VCO screen

Osc (waveform) · Timbre · Shape · Detune · FineTune · Vibrato Depth/Rate · Sub Level/Octave · Noise · Osc2 Mix (including waveform/octave/detune)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |

Choose from 12 waveforms (Sine / Triangle / Sawtooth / Square / Wavefolder / HalfSine / Parabolic / ESaw / Squeeze / ESquare / Saw2 / Square2), and use Shape to continuously reshape the tone. Timbre sets your position within the Morph chain (up to 6 waveforms) configured under SETTING > Patch > Morph.

### VCF screen

Left column: Filter (type) · Cutoff · Resonance · KeyTrack　　Right column: FEnv Depth · Attack · Decay · Release (dedicated filter envelope)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |

Filter type can be set to LPF / HPF / BPF / Notch / None (bypass).

> **To hear the Notch filter clearly**: pick a harmonically rich waveform (Saw/Square rather than Sine), set Cutoff to roughly 600–1500Hz and Resonance fairly high, then hold a sustained note while slowly sweeping Cutoff up and down.

### VCA screen (ADSR)

Attack · Decay · Sustain · Release

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |

### LFO screen

Wave · Rate (0.1–20 Hz) · Depth (0–100%) · Target (modulation destination)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |

The top of the screen shows the LFO waveform along with a live marker tracking its current phase.

### FX screen

A dedicated tab for stacking multiple effects together — 6 pads arranged 2 rows x 3 columns.

| Key | Action |
|---|---|
| `,` / `/` | Select a pad |
| `Enter` | Toggle the selected effect ON/OFF |
| `.` | Open the selected effect's own parameter screen |
| `Tab` (from a parameter screen) | Return to the pad selector |

| Effect | Parameters |
|---|---|
| Ring Modulator | Rate, Mix |
| Soft Limiter | Drive, Mix |
| Chorus | Rate, Depth, Mix |
| Delay | Time, Feedback, Mix |
| Reverb | Room, Damping, Mix |
| Bitcrush | Amount |

Setting any effect's Mix to 0 turns it off. Once turned off, its previous Mix level is remembered for the rest of the session and restored when you turn it back on (forgotten on power-off).

### SEQ screen (Step Sequencer)

Toggle here from PLAY with a quick tap of `G0`. A 16-step, TB-303-style sequencer where each step can carry a pitch/velocity plus Tie (extend the previous note), Slide, and Accent.

![SEQ mode keymap](images/seq_keymap_en.svg)

| Key | Action |
|---|---|
| Note keys | Assign a pitch to the selected step (plays a preview and auto-advances to the next step) |
| `,` / `/` | Move the step cursor |
| `f` | Toggle edit focus (Step editing ⇔ Pattern settings) |
| `g` | Cycle the edit field (Step focus: Velocity→Tie→Slide→Accent / Pattern focus: Tempo→Swing) |
| `;` / `.` | Adjust the selected field (numeric fields increase/decrease; Tie/Slide/Accent toggle) |
| `Backspace` | Clear the selected step |
| `Shift+Backspace` | Clear all 16 steps |
| `V` | Mark / confirm / clear a step-range selection |
| `Shift+C` | Copy the selection |
| `Shift+X` | Cut the selection |
| `Enter` | Paste at the cursor |
| `Space` | Play/Stop the pattern |
| `k` / `l` | Volume down / up (1% steps, hold to accelerate) |
| `Shift+Enter` | Tap Tempo |
| `Tab` / `Shift+Tab` | Cycle menus forward / backward |
| `H` (hold) | Show help overlay |
| `Shift+H` | Latch the help overlay in place; press again to release |

On the step grid, velocity is shown as bar height, and a run of Tie-connected steps merges into one shape with a thick outline. Accented steps turn red.

### SETTING screen

Entry points: Patch / Pattern / IMU (PAD on original) / Bend / Portamento / Play Mode / Arp (ADV only) / MIDI / MIDI Out / MIDI In / Theremin / Screen. **Patch/IMU/Bend/Portamento/the MIDI screens/Screen/Theremin show up from either PLAY or SEQ; Pattern only shows up when SEQ is the active home mode, and Arp only when PLAY is.**

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Open the selected category |
| `Tab` | Save settings and return to the PLAY/SEQ screen |

#### Patch sub-menu

Save · Load · Reset (tone reset) · Random (tone randomize) · Morph (Morph slot configuration). Both Reset and Random are behind a confirmation dialog.

Reset, after a confirmation screen, puts VCO, VCF, VCA, LFO, and IMU settings back to their defaults (Bend, Portamento, and Arp are excluded, since they're managed separately from patches — each has its own dedicated Reset instead).

Morph lets you assign up to 10 patches to Morph slots, recalled with `Shift+1` through `Shift+0`. Setting Morph Time to something other than 0 makes switching gradual rather than instant.

#### Pattern sub-menu

Save · Load · Random (pattern randomize). Random draws from the currently active scale and includes Tie/Slide/Accent, not just pitch/velocity.

#### IMU / PAD sub-menu

Target · Sensitivity · Invert · Curve · Deadzone (hidden on original Cardputer) per axis, plus Calibrate (ON/OFF toggle, ADV only).

Target selection opens a scrollable picker via `/`, with category divider lines; `;`/`.` to scroll, `/` or Enter to confirm. See "IMU / PAD assignable targets" above for the full list.

#### Bend sub-menu

Bend width · attack · release · Reset. A device-level setting, not saved with the patch.

#### Portamento sub-menu

ON/OFF · speed · Reset. A device-level setting, not saved with the patch.

#### Play Mode sub-menu

Toggle between EZ Mode and Pro Mode; when in Pro Mode, also select a scale (49 scales across 9 categories).

#### Arp sub-menu (ADV only)

Type (UP/DOWN/UP-DOWN/AS PLAYED/RANDOM) · Tempo · Rate · Swing (Tempo/Swing in 1-unit steps, hold to accelerate). The On/Off toggle itself lives on `Shift+V` (works from any screen) rather than in this menu. A device-level setting, not saved with the patch.

#### MIDI / MIDI Out / MIDI In sub-menus

Settings for connecting to external MIDI gear, split across three screens: general settings, send settings, and receive settings. See the Reference Guide for full details.

#### Theremin sub-menu

Settings for non-contact playing using a separately sold distance sensor (ToF) unit (M5Stack Unit ToF4M) — connection method (Grove / LoRa Cap-shared), pitch mapping mode (Smooth / Semitone), and pitch range (Top / Span). See the Reference Guide for full details.

#### Screen sub-menu

UI theme (5 options), screen brightness, and other display settings.

### Patch Bank screen

Opened from the Patch sub-menu's Save or Load item.

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next patch |
| `/` or `Enter` | Confirm (Load / new Save / confirm overwrite) |
| `r` | Rename the selected patch |
| `c` | Duplicate the selected patch |
| `,` | Delete the selected patch (with confirmation) |
| `Tab` | Go back one level |

In Save mode, a `<New Patch>` entry appears at the top of the list for creating a new patch. Patches are only ever saved inside `/CPS/Patch/` — the app cannot navigate to any other folder.

### Pattern Bank screen

Opened from the Pattern sub-menu's Save or Load item. Choose from a grid of 8 banks (A–H) × 8 slots (1–8).

| Key | Action |
|---|---|
| `;` / `.` / `,` / `/` | Move around the bank/slot grid |
| `Enter` | Confirm (Load / new Save / confirm overwrite) |
| `Backspace` | Clear the selected slot (with confirmation) |
| `Tab` | Go back one level |

Patterns are saved under `/CPS/Pattern/` with a filename combining the bank letter and slot number (e.g. `A1.json`).

### Song mode screen

Enter with a long press of `G0`. Arrange saved patterns into a full song and play them back.

![SONG mode keymap](images/song_keymap_en.svg)

| Key | Action |
|---|---|
| `,` / `/` | Move the entry cursor |
| `f` | Toggle edit focus (Entry editing ⇔ Song-level settings) |
| `g` | Cycle the edit field (Entry focus: Bank→Slot→Transpose→Repeat / Song focus: Tempo→Swing) |
| `;` / `.` | Adjust the selected field |
| `Enter` | Insert a new entry right after the cursor |
| `Backspace` | Delete the selected entry |
| `I` | Toggle whether each entry inherits its own pattern's Tempo/Swing |
| `O` | Toggle whether the song loops back to the start after the last entry |
| `Space` | Play/Stop the song |
| `Shift+S` / `Shift+L` | Save / Load the song |
| `k` / `l` | Volume down / up |
| `Tab` | Return to whichever of PLAY/SEQ was active |
| `H` (hold) | Show help overlay |
| `Shift+H` | Latch the help overlay in place; press again to release |

Entries appear as a horizontal timeline of blocks color-coded by bank letter (the bar beneath each block shows its Repeat count), with a small preview grid showing the shape of whichever pattern is playing (or selected, while stopped). Transpose is chromatic (semitone-based) and independent per entry. Songs save under `/CPS/Song/` by number (1–8).

---

## Differences on the original Cardputer

Auto-detected at boot; the following differ from CardputerADV:

- **PAD control instead of IMU**: `;`/`.` move a virtual Y axis, `,`/`/` move a virtual X axis. Moves toward the extreme while held, springs back to center on release (toggle Hold with `A`/`S` to keep it from springing back)
- **Octave/Transpose keys move**: since PAD control needs those keys, Octave is on `J`/`N` and Transpose is on `B`/`M` instead
- **Deadzone and Calibrate are hidden**: neither concept applies to a key-driven PAD (no sensor noise to filter, no physical zero-point to correct)
- **Arpeggiator is not supported**: it needs multi-key chord holding, which the original's 3-key rollover limit can't reliably support
- **Step Sequencer, Pattern Bank, and Song mode all work on both boards**: entering one note per step doesn't need key rollover, so these aren't affected by the 3-key limit
- **3-key rollover limit**: due to hardware constraints, pressing a 4th key at the same time may cause key ghosting
- **On the automatic keyboard recovery feature**: the known quirk it addresses is specific to the Cardputer ADV's keyboard chip (TCA8418, I2C-connected). The original Cardputer reads its keyboard a different way and isn't affected by this particular issue

---

## Signal path

```
Oscillator (wavetable morph, Shape)
    │
    ├── Second oscillator (Osc2)
    ├── Sub oscillator (-1/-2 oct)
    ├── Noise blend
    │
    ▼
Ring Modulator
    │
    ▼
Bitcrush
    │
    ▼
Biquad Filter  ◄── Filter envelope / Filter key tracking / IMU(PAD) / General LFO (Filter)
    │
    ▼
Volume (key vol × IMU(PAD) volume multiplier + General LFO (Volume))
    │
    ▼
Tremolo (LFO × depth)
    │
    ▼
ADSR Envelope
    │
    ▼
Chorus
    │
    ▼
Delay
    │
    ▼
Reverb
    │
    ▼
Soft Limiter
    │
    ▼
Speaker (I2S)
```

Pitch modulation (vibrato LFO + IMU(PAD) bend + key bend + General LFO (Pitch)) is applied to the oscillator phase increment before sample generation. In SEQ/Song mode, this same path is driven by the Sequencer's own timing engine.

Chorus, Delay, and Reverb keep processing for a short time after a note ends (up to about 8 seconds) so their natural tail continues to ring out rather than cutting off abruptly.

---

## Project structure

```
CPS/
├── platformio.ini      # Build configuration
├── merge_bin.py         # Post-build script: generates merge.bin for M5Burner
└── src/
    └── main.cpp          # All source code (single-file)
```

---

## Dependencies

Managed automatically by PlatformIO:

| Library | Version |
|---|---|
| `m5stack/M5Cardputer` | ≥ 1.1.1 |
| `m5stack/M5Unified` | ≥ 0.2.8 |
| `m5stack/M5GFX` | ≥ 0.2.10 |

`SD` and `SPI` are part of the Arduino-ESP32 core and require no extra entry in `lib_deps`.

---

## Known limitations / future ideas

- Monophonic only (single voice); polyphony is not planned
- Original Cardputer support hasn't been verified on real hardware by the developer yet (bug reports/feedback welcome)
- Display is 240×135 px; layout is tight
- The Cardputer ADV's keyboard chip (TCA8418) has a known hardware quirk where fast key-presses can cause input to stop responding. Automatic detection and recovery mitigate this, but don't fix the underlying cause (see the Reference Guide's Troubleshooting section for details)
- Theremin support with Grove-expansion units other than LoRa Cap is untested
- What comes after v1.0 is still under consideration

---

## License

**CC BY-NC-SA 4.0** (Attribution - NonCommercial - ShareAlike)

- **Attribution (BY)**: please credit the project when you use it
- **NonCommercial (NC)**: commercial use is not permitted
- **ShareAlike (SA)**: if you publish a modified or derivative version, it must be released under the same CC BY-NC-SA 4.0 license

See [https://creativecommons.org/licenses/by-nc-sa/4.0/](https://creativecommons.org/licenses/by-nc-sa/4.0/) for the full terms.

Feel free to use, modify, and share (non-commercial use only). If you build something cool with CPS, consider sharing it with the community!
