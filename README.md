# C.P.S. — CardPuter Synth
[English](README.md) | [日本語](README_ja.md) |

A feature-rich DIY synthesizer for the **M5Stack CardputerADV**, built with PlatformIO and the Arduino framework.

I share my ideas with Claude and have it write the code.
I share development progress on Reddit and my Twitter account — community feedback has been the spark for many of the features below!
Thanks to everyone who's shown interest in this project.

> **This project is currently 100% vibe-coded.**
> My apologies to anyone who isn't comfortable with AI use or vibe coding.

---

## 🆕 v0.7 Update

v0.7 is a major overhaul of the sound-editing experience.

- **Split the old single EDIT menu into three dedicated screens: VCO / VCF / VCA**, for a much more "real synth" editing feel
- Added a **sub oscillator** (-1oct / -2oct, adjustable level)
- Added **noise blend**
- Added **filter key tracking** (cutoff follows the played pitch)
- Added a **dedicated filter envelope** (Depth/Attack/Decay/Release) — independent from vibrato and tremolo, this envelope drives the filter cutoff only
- Added a **general-purpose LFO** as a new tab (Sine/Triangle/Sawtooth/Square, Rate 0.1–20Hz, Depth 0–100%, Target: Pitch/Volume/Timbre/Filter/PWM), fully independent from the existing vibrato and tremolo
- Added a **"None" (bypass) filter type**
- Added a **Patch Bank**: save and recall every parameter — including IMU mapping and hold state — as a named patch. Rename, duplicate, and delete are also supported
- New tab order: `MAIN → VCO → VCF → VCA → LFO → SETTING`
- Added **transpose** (`[` / `]` keys, ±12 semitones)
- Added an at-a-glance indicator for the IMU X/Y axis hold ON/OFF state

---

## Features

| Category | Details |
|---|---|
| **Oscillator** | Real-time wavetable synthesis: Sine → Triangle → Sawtooth → Square (morphable), with PWM |
| **Sub oscillator** | -1oct / -2oct, adjustable level |
| **Noise** | Noise blend (adjustable level) |
| **Keyboard** | Number keys `1`–`0` mapped to C4–E5; monophonic (last key wins) |
| **Octave** | `=` / `-` keys shift ±2 octaves |
| **Transpose** | `[` / `]` keys shift ±12 semitones (independent of octave) |
| **Volume** | `,` / `.` keys adjust in 5% steps |
| **Bend** | `Z` key = bend down, `X` key = bend up — guitar-choke feel with asymmetric attack/release |
| **ADSR** | Full Attack/Decay/Sustain/Release envelope with retrigger support |
| **Biquad Filter** | LPF / HPF / BPF / Notch / **None (bypass)**; configurable cutoff (100–8000Hz), Q, and key tracking |
| **Filter envelope** | Dedicated Depth/Attack/Decay/Sustain/Release envelope for the filter cutoff |
| **General-purpose LFO** | Sine/Triangle/Sawtooth/Square, Rate 0.1–20Hz, Depth 0–100%. Modulates Pitch, Volume, Timbre, Filter, or PWM |
| **Bit-crusher** | Lo-Fi effect: reduces bit depth from 16-bit down to ~3-bit |
| **Vibrato** | LFO-driven pitch modulation (rate + depth adjustable, independent of the general-purpose LFO) |
| **Tremolo** | LFO-driven volume modulation (independent of the general-purpose LFO) |
| **Portamento** | ON/OFF, adjustable glide speed |
| **IMU mapping** | BMI270 tilt controls assignable parameters; each axis (X/Y) can be held ON/OFF independently |
| **Patch Bank** | Save/recall every parameter (including IMU mapping and hold state) as a named patch. Rename, duplicate, and delete supported |
| **SD settings** | Current settings auto-save to `/CPS/settings.json` (when leaving the SETTING screen via Tab) |

### IMU assignable targets

`NONE` · `TIMBRE` · `VIBRATO_DEPTH` · `VIBRATO_RATE` · `TREMOLO` · `VOLUME` · `PITCH_BEND` · `BEND_UP` · `BEND_DOWN` · `BITCRUSH` · `FILTER_CUTOFF`

- **PITCH_BEND** — bipolar: tilt direction controls bend direction (great for random pitch effects)
- **BEND_UP / BEND_DOWN** — absolute: tilt amount always raises / lowers pitch

---

## Hardware

| Item | Value |
|---|---|
| Device | M5Stack CardputerADV |
| MCU | ESP32-S3 (dual-core Xtensa LX7, 240 MHz) |
| Audio | ES8311 codec + NS4150B amp, 1 W speaker, 3.5 mm jack |
| IMU | BMI270 6-axis (CardputerADV only) |
| SD slot | SPI — SCK=GPIO40, MISO=GPIO39, MOSI=GPIO14, CS=GPIO12 |

---

## Getting started

### Option 1 — Install via M5Burner (easiest, recommended)

No compiling required — the quickest way to get CPS on your device.

1. Download and install [M5Burner](https://docs.m5stack.com/en/uiflow/M5Burner) from the official site
2. Connect your M5Stack CardputerADV to your computer via USB-C
3. Search for "C.P.S." (CardPuter Synth) inside M5Burner
4. Select the correct COM port and click "Burn"
5. Once flashing completes, CPS will launch automatically

> Inserting a FAT32-formatted SD card lets you use auto-save and the Patch Bank feature.

---

### Option 2 — Install via Launcher FW

If your CardputerADV runs **Launcher FW**, you can install CPS without building anything yourself.

1. Go to the [Releases](../../releases) page and download the latest `C.P.S.v0.7o.bin` file
2. Copy the `.bin` file to the **root of your SD card** (not inside a subfolder)
3. Insert the SD card into your CardputerADV and boot into Launcher FW
4. Navigate to the `.bin` file in the Launcher file browser and select it to flash
5. CPS will launch automatically after flashing

> A FAT32-formatted micro-SD card is required both for the Launcher install and for CPS's settings persistence.

---

### Option 3 — Build from source (PlatformIO)

#### Requirements

- [VSCode](https://code.visualstudio.com/) with the **PlatformIO IDE** extension
- M5Stack CardputerADV connected via USB-C

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

On first boot the app creates `/CPS/` on the SD card (FAT32 micro-SD required).
Settings are saved to `/CPS/settings.json` automatically whenever you leave the SETTING screen (Tab key).
If no SD card is present the app still runs with default settings.

---

## Controls

### MAIN screen

| Key | Action |
|---|---|
| `1` – `0` | Play notes C4 – E5 |
| `=` / `-` | Octave up / down (±2 octaves) |
| `[` / `]` | Transpose down / up (±12 semitones) |
| `,` / `.` | Volume down / up |
| `Z` | Bend down (hold) |
| `X` | Bend up (hold) |
| `C` | Toggle portamento ON/OFF |
| `A` | Toggle IMU X-axis hold ON/OFF |
| `S` | Toggle IMU Y-axis hold ON/OFF |
| `D` | Toggle note hold ON/OFF |
| `H` (hold) | Show help overlay |
| **Tilt device** | Controls whichever parameters are assigned to the X / Y IMU axes |
| `Tab` | Switch to the VCO screen |

The MAIN screen shows the current note name and frequency, octave/transpose/portamento state, a bend meter, IMU tilt state, and the IMU X/Y target names with their current values — appending **(HOLD)** whenever that axis is held.

### VCO screen

Left column: Timbre · PWM · Detune · FineTune　　Right column: Sub Lvl · Sub Oct · Noise

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |
| `Tab` | Switch to the VCF screen |

### VCF screen

Left column: Filter (type) · Cutoff · Resonance · KeyTrack　　Right column: FEnv Depth · Attack · Decay · Release (dedicated filter envelope)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |
| `Tab` | Switch to the VCA screen |

Filter type can be set to LPF / HPF / BPF / Notch / **None** (bypass).

### VCA screen (ADSR)

Attack · Decay · Sustain · Release

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |
| `Tab` | Switch to the LFO screen |

### LFO screen

Wave · Rate (0.1–20 Hz) · Depth (0–100%) · Target (modulation destination)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |
| `Tab` | Switch to the SETTING screen |

The top of the screen shows the LFO waveform along with a live marker tracking its current phase.

### SETTING screen

Left column: Patch Save · Patch Load · IMU X · IMU Y　　Right column: Bend width · Bend attack speed · Bend release speed · Portamento · Porta speed

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value, or open the Patch Save/Load screen |
| `Tab` | Save settings and return to the MAIN screen |

### Patch Bank screen

Opened from the SETTING screen's "Patch Sv" or "Patch Ld" item by pressing `/` or `,`.

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next patch |
| `/` or `Enter` | Confirm (Load / new Save / confirm overwrite) |
| `r` | Rename the selected patch |
| `c` | Duplicate the selected patch |
| `,` | Delete the selected patch (with confirmation) |
| `Tab` | Go back one level (name entry → list, list → SETTING screen) |

In Save mode, a `<New Patch>` entry appears at the top of the list for creating a new patch. Patches are only ever saved inside `/CPS/Patch/` — the app cannot navigate to any other folder.

---

## Signal path

```
Oscillator (wavetable morph)
    │
    ├── Sub oscillator (-1/-2 oct)
    ├── Noise blend
    │
    ▼
Bit-crusher
    │
    ▼
Biquad Filter  ◄── Filter envelope / Filter key tracking / IMU / General LFO (Filter)
    │
    ▼
Volume (key vol + IMU volume offset + General LFO (Volume))
    │
    ▼
Tremolo (LFO × depth)
    │
    ▼
ADSR Envelope
    │
    ▼
Speaker (ES8311 / I2S)
```

Pitch modulation (vibrato LFO + IMU bend + key bend + General LFO (Pitch)) is applied to the oscillator phase increment before sample generation.
The general-purpose LFO's Timbre/PWM targets are applied as a temporary offset to the oscillator's morph value / PWM width (the stored VCO menu values themselves are left unchanged).

---

## Project structure

```
CPS/
├── platformio.ini      # Build configuration
├── merge_bin.py        # Post-build script: generates merge.bin for M5Burner
└── src/
    └── main.cpp        # All source code (single-file)
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

- Monophonic only (last note wins); polyphony is not planned
- IMU axis sensitivity is fixed at 1.0; per-axis sensitivity could be added to SETTING
- Display is 240×135 px; layout is tight
- More features planned on the road to v1.0

---

## License

MIT — feel free to use, modify, and share.
If you build something cool with CPS, consider sharing it with the community!
