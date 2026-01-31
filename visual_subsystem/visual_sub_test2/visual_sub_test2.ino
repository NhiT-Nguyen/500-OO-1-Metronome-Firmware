/*
 * Tempo Titans - Beat Box Display
 * 
 * This program displays a metronome-style BPM indicator on a micro OLED display
 * using composite video output from the ESP32's built-in DAC.
 * 
 * Hardware:
 *   - ESP32 GPIO 25 -> AV+ (Yellow wire on AV driver board)
 *   - ESP32 GND     -> GND (Black wire on AV driver board)
 *   - ESP32 5V      -> DC3.5-5V (Red wire on AV driver board)
 * 
 * Libraries:
 *   - ESP_8_BIT_GFX: Generates composite video signal using ESP32's DAC
 *  
 * 
 * Note: AI (Claude) was used to assist with debugging
 */

#include "ESP_8_BIT_GFX.h"

/*
 * Video output configuration
 * First parameter:  true = NTSC (North America, 30fps), false = PAL (Europe, 25fps) - Analog TV standards
 * Second parameter: 8 = 8-bit colour mode (RGB332 format, 256 colours)
 */
ESP_8_BIT_GFX videoOut(true, 8);

/*
 * BPM and timing variables
 * bpm:          Current beats per minute
 * beatCount:    Total number of beats since program started (used to determine current beat 1-4)
 * lastBeatTime: Timestamp (in milliseconds) of when the last beat occurred
 */
int bpm = 50;
int beatCount = 0;
unsigned long lastBeatTime = 0;

/*
 * Colour definitions (RGB332 format)
 * RGB332 uses 3 bits for red, 3 bits for green, 2 bits for blue
 * This gives 256 possible colours in a single byte
 * 
 * Colours:
 * - Blue and yellow 
 */
uint8_t bgColor = 0x02;        // Dark blue background
uint8_t boxColor = 0x03;       // Blue boxes (inactive boxes)
uint8_t activeColor = 0xFC;    // Yellow (active)
uint8_t textColor = 0xFF;      // White text
uint8_t bpmColor = 0xFF;       // White for "Current BPM"

/*
 * setup()
 * Runs once when ESP32 starts or resets
 * Initializes serial communication and video output
 */
void setup() {
  // Initialize serial communication for debugging (115200 baud rate)
  Serial.begin(115200);
  Serial.println("Tempo Titans");
  
  // Initialize composite video output
  videoOut.begin();
  
  // Reduces screen flicker by copying frame buffer after swap
  videoOut.copyAfterSwap = true;
}

/*
 * loop()
 * Main program loop - runs continuously
 * Draws the display and handles beat timing
 */
void loop() {
  // Wait for the next video frame (synchronizes drawing with display refresh)
  videoOut.waitForFrame();
  
  // Clear the screen by filling with background colour
  videoOut.fillScreen(bgColor);
  
  /*
   * Calculate current beat (0-3)
   * Uses modulo (%) to cycle through 0, 1, 2, 3, 0, 1, 2, 3...
   * Example: beatCount=5 -> 5 % 4 = 1 (second beat)
   */
  int currentBeat = (beatCount % 4);

  /*
   * Beat timing logic
   * 
   * Calculate milliseconds between beats:
   * beatInterval = 60000ms (1 minute) / bpm
   * 
   * Example: 50 BPM -> 60000 / 50 = 1200ms between beats
   */
  unsigned long beatInterval = 60000 / bpm;

  /*
   * Subdivision logic
   * Each beat is divided into 4 subdivisions (0-3)
   * Current subdivision is determined by the beatCount multiplied by 4
   */
  unsigned long subInterval = beatInterval / 4;
  unsigned long lastSubTime = millis() - lastBeatTime;
  int currentSub = (lastSubTime / subInterval) % 4;
  
  /* 
   * Beat box dimensions and positioning
   * 
   * Screen width is 256 pixels
   * Total width of boxes: (4 boxes * 50) + (3 gaps * 10) = 230 pixels
   * Starting X position: (256 - 230) / 2 = 13 pixels (centres the boxes)
   */
  int boxWidth = 50;
  int boxHeight = 60;
  int boxSpacing = 10;
  int startX = 13;
  int boxY = 30;

  // Subdivision box dimensions
  int subBoxWidth = 10;
  int subBoxHeight = 10;
  int subBoxSpacing = 2;
  int subBoxY = boxY + boxHeight + 5;  
  
  /*
   * Draw 4 beat boxes and subdivisions
   * Loop through each box (i = 0, 1, 2, 3)
   */
  for (int i = 0; i < 4; i++) {
    // Calculate x position for this box
    // Each box is offset by (boxWidth + boxSpacing) from the previous one
    int x = startX + (i * (boxWidth + boxSpacing));
    
    if (i == currentBeat) {
      // Active beat - draw yellow filled rectangle
      videoOut.fillRect(x, boxY, boxWidth, boxHeight, activeColor);
      videoOut.setTextColor(0x00);  // Black text for contrast on yellow
    } else {
      // Inactive beat - draw blue filled rectangle
      videoOut.fillRect(x, boxY, boxWidth, boxHeight, boxColor);
      videoOut.setTextColor(0xFF);  // White text for contrast on blue
    }
    
    // Draw beat number (1, 2, 3, or 4) inside the box
    videoOut.setTextSize(2);
    // Position text roughly centred in the box
    videoOut.setCursor(x + 18, boxY + 22);
    videoOut.print(i + 1);  // i+1 converts 0-3 to 1-4

    // Draw subdiviosion boxes
    for (int j = 0; j < 4; j++) {
      int totalSubWidth = (4 * subBoxWidth) + (3 * subBoxSpacing);
      int subStartX = x + (boxWidth - totalSubWidth) / 2;
      int subX = subStartX + (j * (subBoxWidth + subBoxSpacing));

      if( i == currentBeat && j == currentSub) {
        videoOut.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, activeColor);
      } else {
        videoOut.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, boxColor);
      }
    }
  }
  
  /*
   * Draw "Current BPM:" label
   */
  videoOut.setTextColor(bpmColor);
  videoOut.setTextSize(2);
  videoOut.setCursor(50, 110);
  videoOut.print("Current BPM:");
  
  /*
   * Draw BPM number
   * Adjusts x position based on number of digits to keep it centred
   */
  videoOut.setTextColor(textColor);
  videoOut.setTextSize(4);  // Larger text for BPM value
  
  if (bpm < 100) {
    // Two digits - position further right
    videoOut.setCursor(95, 145);
  } else {
    // Three digits - position further left
    videoOut.setCursor(75, 145);
  }
  videoOut.print(bpm);
  
  /*
   * Check if it is time for the next beat
   * millis() returns milliseconds since ESP32 started
   * If enough time has passed since last beat, trigger next beat
   */
  if (millis() - lastBeatTime >= beatInterval) {
    // Record the time of this beat
    lastBeatTime = millis();
    
    // Increment beat counter
    beatCount++;
    
    // Print beat number to serial monitor for debugging
    Serial.print("Beat: ");
    Serial.println((beatCount % 4) + 1);
  }
}