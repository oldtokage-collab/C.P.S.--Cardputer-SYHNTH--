# C.P.S. — Cardputer Synth

A feature-rich DIY synthesizer for the **M5Stack CardputerADV**, built with PlatformIO and the Arduino framework.

> Originally developed with the help of Claude (Anthropic) and shared on Reddit — community feedback was the spark for many of the features below!

---

## Features

| Category | Details |
|---|---|
| **Oscillator** | Real-time wavetable synthesis: Sine → Triangle → Sawtooth → Square (morphable) |
| **Keyboard** | Number keys `1`–`0` mapped to C4–E5; monophonic (last key wins) |
| **Octave** | `=` / `-` keys shift ±2 octaves |
| **Volume** | `,` / `.` keys adjust in 5 % steps |
| **Bend** | `Z` key = bend down, `X` key = bend up — guitar-choke feel with asymmetric attack/release |
| **ADSR** | Full Attack / Decay / Sustain / Release envelope with retrigger support |
| **Biquad Filter** | LPF / HPF / BPF / Notch; configurable cutoff (100–8000 Hz) and Q |
| **Bit-crusher** | Lo-Fi effect: reduces bit depth from 16-bit down to ~3-bit |
| **Vibrato** | LFO-driven pitch modulation (rate + depth both controllable) |
| **Tremolo** | LFO-driven volume modulation |
| **IMU mapping** | BMI270 tilt controls any combination of the parameters above |
| **SD settings** | All settings auto-saved to `/CPS/settings.json` on Tab → MAIN |

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

### Option 1 — Pre-built binary (easiest, no compiler needed)

If you are running the **Launcher FW** on your CardputerADV, you can install CPS without building anything yourself.

1. Go to the [Releases](../../releases) page and download the latest `C.P.S.v0.5c.bin` file.
2. Copy the `.bin` file to the **root of your SD card** (not inside a subfolder).
3. Insert the SD card into your CardputerADV and boot into Launcher FW.
4. Navigate to the `.bin` file in the Launcher file browser and select it to flash.
5. CPS will launch automatically after flashing.

> A FAT32-formatted micro-SD card is required both for the Launcher install and for CPS's settings persistence.

---

### Option 2 — Build from source (PlatformIO)

### Requirements

- [VSCode](https://code.visualstudio.com/) with the **PlatformIO IDE** extension
- M5Stack CardputerADV connected via USB-C

### Build & flash

1. Clone or download this repository.
2. Open the `CPS` folder in VSCode (`File › Open Folder`).
3. PlatformIO will auto-detect `platformio.ini` and download the required libraries on the first build.
4. Click **Upload** (→ button in the bottom toolbar).

After a successful build, two files are generated in `.pio/build/cps/`:

| File | Purpose |
|---|---|
| `firmware.bin` | App binary only — used by PlatformIO's Upload button |
| `merge.bin` | **Merged** (bootloader + partitions + app) — use this for M5Burner or any single-file flash tool |

> **Boot-to-flash mode** (if upload fails): power off → hold G0 → power on → release G0.

### First boot (both install methods)

On first boot the app creates `/CPS/` on the SD card (FAT32 micro-SD required).  
Settings are saved to `/CPS/settings.json` automatically whenever you leave the SETTING screen.  
If no SD card is present the app still runs with default settings.

---

## Controls

### MAIN screen

| Key | Action |
|---|---|
| `1` – `0` | Play notes C4 – E5 |
| `=` / `-` | Octave up / down |
| `,` / `.` | Volume down / up |
| `Z` | Bend down (hold) |
| `X` | Bend up (hold) |
| **Tilt device** | Controls whichever parameters are assigned to X / Y IMU axes |
| `Tab` | Cycle to EDIT screen |

### EDIT screen (ADSR + Filter)

| Key | Action |
|---|---|
| `;` / `.` | Select previous / next item |
| `,` / `/` | Decrease / increase value |
| `Tab` | Cycle to SETTING screen |

Items: **Attack · Decay · Sustain · Release · Filter type · Cutoff · Resonance**

### SETTING screen

Same navigation keys as EDIT.

Items: **IMU X axis · IMU Y axis · Bend width · Bend attack speed · Bend release speed**

Leaving SETTING with `Tab` **auto-saves** all settings to the SD card.

---

## Signal path

```
Oscillator (wavetable morph)
    │
    ▼
Bit-crusher
    │
    ▼
Biquad Filter  ◄── IMU FILTER_CUTOFF offset
    │
    ▼
Volume (key vol + IMU VOLUME offset)
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

Pitch modulation (vibrato LFO + IMU bend + key bend) is applied to the oscillator phase increment before sample generation.

---

## Project structure

```
CPS/
├── platformio.ini      # Build configuration
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

- Monophonic only (last note wins); polyphony is a potential future addition
- IMU axis sensitivity is fixed at 1.0; per-axis sensitivity could be added to SETTING
- Settings are a flat JSON file; a preset system (multiple named slots) could be useful
- Display is 240×135 px; layout is tight — a larger device variant could show more info

---

## License

MIT — feel free to use, modify, and share.  
If you build something cool with CPS, consider sharing it with the community!
