/*
 * Tempo Titans - Subdivisions + Tone Synthesis Edition
 *
 * Ported from metronome_logic.dart
 *
 * Key additions vs. the SPIFFS WAV edition
 * ─────────────────────────────────────────
 * • Per-beat sound MODE  (0-4) — replaces the hard-coded beat→WAV mapping
 * • Per-beat SUBDIVISIONS (2-4 sub-ticks per beat, each with its own mode)
 * • Synthesised tones replace WAV files — no SPIFFS upload needed
 * • Beat boxes AND subdivision dots are coloured by their assigned mode
 * • Drift-corrected tick scheduling (mirrors the Dart Stopwatch approach)
 *
 * Mode → Sound / Colour table
 * ───────────────────────────
 *  Mode 0  bell    1400 Hz, 80 ms fade   Yellow  (0xFC)
 *  Mode 1  click   1000 Hz, 30 ms sharp  Green   (0x1C)
 *  Mode 2  tick     700 Hz, 20 ms short  Cyan    (0x1F)
 *  Mode 3  drum     200 Hz, 60 ms thud   Red     (0xE0)
 *  Mode 4  silent  (no sound)            Grey    (0x49)
 *
 * Display Wiring:
 *   AV+ (Yellow) -> GPIO 25
 *   GND          -> GND
 *   5V           -> 5V
 *
 * MAX98357A Wiring:
 *   Vin  -> 5V
 *   GND  -> GND
 *   BCLK -> GPIO 14
 *   LRC  -> GPIO 13
 *   DIN  -> GPIO 27
 *
 * Fixes applied vs. visual_sub_test2.ino
 * ───────────────────────────────────────
 * FIX 1 — Removed videoOut.copyAfterSwap = true
 *          That flag caused intermittent screen blanking when combined with a
 *          full redraw every frame.  Since we call fillScreen + all draw
 *          functions every loop iteration anyway, the copy is unnecessary.
 *
 * FIX 2 — Clamp tone duration to sub-interval in playTone()
 *          The blocking i2s_write loop was able to run longer than one
 *          sub-interval at high BPM, causing missed ticks.  The duration is
 *          now capped to (subInterval - 5 ms) so timing is always recovered.
 *
 * FIX 3 — getNextIntervalUs() called AFTER advanceSubdivision()
 *          Previously the interval was sampled before the beat state advanced,
 *          so a beat boundary tick used the old beat's subdivision count.
 *          Moving the sample after the advance ensures the correct interval is
 *          stored into nextTickUs.
 */

#include "ESP_8_BIT_GFX.h"
#include "driver/i2s.h"
#include <math.h>

// ── Video ────────────────────────────────────────────────────────────────────
ESP_8_BIT_GFX videoOut(true, 8);   // NTSC, 8-bit colour
// NOTE: copyAfterSwap intentionally NOT set — it caused random screen blanking.

// ── I2S pins ─────────────────────────────────────────────────────────────────
#define I2S_BCLK  14
#define I2S_LRC   13
#define I2S_DOUT  27
#define SAMPLE_RATE 16000

// ── Beat / subdivision limits ─────────────────────────────────────────────────
#define MAX_BEATS     4
#define MAX_SUBS      4   // max subdivisions per beat (2-4 allowed)
#define NUM_MODES     5   // modes 0-4

// ── Mode colour table (8-bit NTSC palette) ────────────────────────────────────
//  Index matches the mode integer sent from the Flutter app / set locally.
const uint8_t MODE_COLORS[NUM_MODES] = {
  0xFC,   // 0 bell    — Yellow
  0x1C,   // 1 click   — Green
  0x1F,   // 2 tick    — Cyan
  0xE0,   // 3 drum    — Red
  0x49,   // 4 silent  — Dark grey
};

// ── Tone parameters per mode ──────────────────────────────────────────────────
struct ToneParams { int freqHz; int durationMs; };
const ToneParams TONE_TABLE[NUM_MODES] = {
  {1400, 80 },   // 0 bell
  {1000, 30 },   // 1 click
  { 700, 20 },   // 2 tick
  { 200, 60 },   // 3 drum
  {   0,  0 },   // 4 silent
};

// ── Metronome state ───────────────────────────────────────────────────────────
int  bpm              = 100;
int  meterBeats       = 4;     // beats per measure (2, 3, or 4)

int  activeBeat       = 0;     // 0-indexed current beat
int  activeSubdivision= 0;     // 0-indexed subdivision within activeBeat

// beatModes[b]         : sound mode for beat b  (0-4)
// subdivisionModes[b][s]: sound mode for subdivision s of beat b
// showSubdivisions[b]  : whether beat b plays sub-ticks
// beatSubdivisions[b]  : how many sub-ticks beat b has (2-4)
int  beatModes[MAX_BEATS]                   = {0, 1, 1, 1};
int  subdivisionModes[MAX_BEATS][MAX_SUBS]  = {
       {2, 2, 2, 2},
       {2, 2, 2, 2},
       {2, 2, 2, 2},
       {2, 2, 2, 2},
     };
bool showSubdivisions[MAX_BEATS]            = {true, false, true, false};
int  beatSubdivisions[MAX_BEATS]            = {4, 4, 4, 4};

// ── Drift-corrected timing (mirrors Dart Stopwatch approach) ──────────────────
// Instead of scheduling each tick relative to the previous callback time
// (which drifts), we track an absolute target in microseconds since boot.
// Each tick advances the target by the fixed interval, so a late callback
// simply schedules the next one sooner rather than sliding the whole timeline.
unsigned long nextTickUs  = 0;   // absolute target (micros())
bool          firstBeat   = true;

// ── Display layout constants ──────────────────────────────────────────────────
#define SCREEN_W   256
#define SCREEN_H   240

#define BOX_W       50
#define BOX_H       50
#define BOX_GAP     10
#define BOX_START_X 13
#define BOX_Y       20

#define SUB_DOT_R   5          // subdivision dot radius
#define SUB_ROW_Y   (BOX_Y + BOX_H + 14)  // vertical centre of sub dots

// ── Colours ───────────────────────────────────────────────────────────────────
#define COL_BG      0x02   // dark blue background
#define COL_BOX     0x03   // inactive beat box
#define COL_TEXT    0xFF   // white text
#define COL_ACTIVE_BORDER 0xFF  // white border on active box

// ─────────────────────────────────────────────────────────────────────────────
//  Interval helpers  (mirrors Dart beatInterval / subdivisionInterval)
//  Declared before playTone so the duration clamp can call subIntervalUs().
// ─────────────────────────────────────────────────────────────────────────────
unsigned long beatIntervalUs() {
  return (unsigned long)(60000000UL / bpm);
}

unsigned long subIntervalUs(int beatIndex) {
  int subs = beatSubdivisions[beatIndex];
  return beatIntervalUs() / subs;
}

// Returns the interval that should follow the CURRENT tick position
unsigned long getNextIntervalUs() {
  if (showSubdivisions[activeBeat]) {
    return subIntervalUs(activeBeat);
  }
  return beatIntervalUs();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tone synthesis
//  Generates a sine wave with a linear fade-out envelope and streams it to I2S.
//
//  FIX 2: Duration is clamped to (current sub-interval - 5 ms) so the blocking
//  write loop can never overrun the next tick deadline.
// ─────────────────────────────────────────────────────────────────────────────
void playTone(int freqHz, int durationMs) {
  if (freqHz == 0 || durationMs == 0) return;   // mode 4 — silent

  // Clamp duration so we never block longer than one sub-interval.
  // Leave a 5 ms margin for the rest of loop() work.
  unsigned long maxMs = getNextIntervalUs() / 1000UL;
  if (maxMs > 5 && durationMs > (int)(maxMs - 5)) {
    durationMs = (int)(maxMs - 5);
  }
  if (durationMs <= 0) return;

  int totalSamples = (long)SAMPLE_RATE * durationMs / 1000;
  size_t bw;

  for (int i = 0; i < totalSamples; i++) {
    float env  = 1.0f - ((float)i / totalSamples);   // linear decay
    float wave = sinf(2.0f * PI * freqHz * i / (float)SAMPLE_RATE);
    int16_t s  = (int16_t)(wave * env * 0.75f * 32000);
    int16_t lr[2] = {s, s};
    i2s_write(I2S_NUM_1, lr, sizeof(lr), &bw, portMAX_DELAY);
  }
}

// Play the tone associated with a mode integer
void playMode(int mode) {
  if (mode < 0 || mode >= NUM_MODES) mode = 0;
  playTone(TONE_TABLE[mode].freqHz, TONE_TABLE[mode].durationMs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  I2S setup  (I2S_NUM_1 — video driver owns I2S_NUM_0)
// ─────────────────────────────────────────────────────────────────────────────
void setupI2S() {
  i2s_config_t cfg = {
    .mode                = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate         = SAMPLE_RATE,
    .bits_per_sample     = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format      = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format= I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags    = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count       = 8,
    .dma_buf_len         = 64,
    .use_apll            = false,
    .tx_desc_auto_clear  = true
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Advance logic  (mirrors advanceBeat / advanceSubdivision in Dart)
// ─────────────────────────────────────────────────────────────────────────────
void advanceBeat() {
  activeBeat = (activeBeat + 1) % meterBeats;
  activeSubdivision = 0;

  // Play beat sound (first subdivision sound if subs enabled)
  if (showSubdivisions[activeBeat])
    playMode(subdivisionModes[activeBeat][0]);
  else
    playMode(beatModes[activeBeat]);
}

void advanceSubdivision() {
  bool hasSubs = showSubdivisions[activeBeat];
  if (hasSubs) {
    int subCount = beatSubdivisions[activeBeat];
    if (activeSubdivision < subCount - 1) {
      activeSubdivision++;
      playMode(subdivisionModes[activeBeat][activeSubdivision]);
    } else {
      advanceBeat();
    }
  } else {
    advanceBeat();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns the x-left of beat box [i]
int boxX(int i) {
  return BOX_START_X + i * (BOX_W + BOX_GAP);
}

// Draws beat boxes with mode-colour fill and white border when active
void drawBeatBoxes() {
  for (int i = 0; i < meterBeats; i++) {
    int x    = boxX(i);
    bool act = (i == activeBeat && activeSubdivision == 0);

    uint8_t fillCol = MODE_COLORS[beatModes[i]];

    if (act) {
      // Brighter: draw a white border then fill inside
      videoOut.fillRect(x, BOX_Y, BOX_W, BOX_H, COL_ACTIVE_BORDER);
      videoOut.fillRect(x + 2, BOX_Y + 2, BOX_W - 4, BOX_H - 4, fillCol);
    } else {
      videoOut.fillRect(x, BOX_Y, BOX_W, BOX_H, fillCol);
    }

    // Beat number centred in box
    videoOut.setTextSize(2);
    videoOut.setTextColor(act ? 0x00 : COL_TEXT);
    videoOut.setCursor(x + 18, BOX_Y + 17);
    videoOut.print(i + 1);
  }
}

// Draws subdivision dots beneath each beat box
// Active subdivision glows white-bordered; others coloured by their mode
void drawSubdivisionDots() {
  for (int b = 0; b < meterBeats; b++) {
    if (!showSubdivisions[b]) continue;

    int subCount = beatSubdivisions[b];
    int bx       = boxX(b);
    // Evenly space dots within the beat box column
    int slotW    = BOX_W / subCount;

    for (int s = 0; s < subCount; s++) {
      int cx    = bx + slotW * s + slotW / 2;
      int cy    = SUB_ROW_Y;
      bool act  = (b == activeBeat && s == activeSubdivision);

      uint8_t dotCol = MODE_COLORS[subdivisionModes[b][s]];

      if (act) {
        videoOut.fillCircle(cx, cy, SUB_DOT_R + 2, COL_ACTIVE_BORDER);
        videoOut.fillCircle(cx, cy, SUB_DOT_R,     dotCol);
      } else {
        videoOut.fillCircle(cx, cy, SUB_DOT_R, dotCol);
      }
    }
  }
}

void drawBpmDisplay() {
  videoOut.setTextColor(COL_TEXT);
  videoOut.setTextSize(2);
  videoOut.setCursor(50, 110);
  videoOut.print("Current BPM:");

  videoOut.setTextSize(4);
  videoOut.setCursor(bpm < 100 ? 95 : 75, 145);
  videoOut.print(bpm);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("Tempo Titans - Subdivisions + Tone Edition");

  videoOut.begin();
  // FIX 1: copyAfterSwap removed — caused random screen blanking.
  // Full redraw every frame makes it unnecessary.

  setupI2S();

  // Startup bell
  playMode(0);

  // Prime the absolute tick clock: first beat fires immediately
  nextTickUs = micros();
  firstBeat  = true;

  Serial.println("Ready!");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  // ── Tick / beat advancement ───────────────────────────────────────────────
  unsigned long now = micros();

  if (firstBeat) {
    // Play beat 0 immediately on start
    activeBeat        = 0;
    activeSubdivision = 0;
    playMode(showSubdivisions[0] ? subdivisionModes[0][0] : beatModes[0]);
    nextTickUs = now + getNextIntervalUs();
    firstBeat  = false;
  } else if (now >= nextTickUs) {
    // FIX 3: advanceSubdivision() is called FIRST so that activeBeat /
    // activeSubdivision reflect the new position before we sample the
    // interval.  Previously the interval was read before the advance, so a
    // beat-boundary tick used the old beat's subdivision count.
    advanceSubdivision();

    // Drift correction: advance target by the interval, not from 'now',
    // so accumulated jitter doesn't shift the whole timeline.
    unsigned long interval = getNextIntervalUs();   // sampled after advance
    nextTickUs += interval;

    // If we're running very late (e.g. audio took too long), re-anchor
    // to avoid a burst of catch-up ticks.
    if (micros() > nextTickUs + interval) {
      nextTickUs = micros() + interval;
    }

    Serial.printf("Beat %d  Sub %d\n", activeBeat, activeSubdivision);
  }

  // ── Display ───────────────────────────────────────────────────────────────
  videoOut.waitForFrame();
  videoOut.fillScreen(COL_BG);

  drawBeatBoxes();
  drawSubdivisionDots();
  drawBpmDisplay();
}
