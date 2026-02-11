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
 * JSON Format:
 * {
 *   "command": "start" or "stop",
 *   "bpm": 60,
 *   "numBeats": 4,
 *   "beats": [
 *    {"subdivisions": 0},
 *    {"subdivisions": 2},
 *    {"subdivisions": 3},
 *    {"subdivisions": 4}
 * } 
 * 
 * Note: AI (Claude) was used to assist with debugging
 */

#include "ESP_8_BIT_GFX.h"
#include <ArduinoJson.h>

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

// Beats configuration
struct BeatConfig {
  int numBeats;                  
  int subdivisions[4];           
  bool isRunning;                
};

// Initialize with default values
BeatConfig config = {
  4,              
  {0, 0, 0, 0},   
  false            
};


// Parse JSON string
// Code inspired from: https://arduinojson.org/v7/assistant/#/step1
bool parseJson(const char* input) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }

  const char* command = doc["command"]; 
  if (command == nullptr) {
    Serial.println("No command found in JSON");
    return false;
  }

  if (strcmp(command, "start") == 0) {
    config.isRunning = true;
    beatCount = 0;          
    lastBeatTime = millis();
  } else if (strcmp(command, "stop") == 0) {
    config.isRunning = false;
    beatCount = 0;
    lastBeatTime = 0;
    return true;
  }

  bpm = doc["bpm"];
  config.numBeats = doc["numBeats"]; 

  int beatIndex = 0;
  for (JsonObject beat : doc["beats"].as<JsonArray>()) {
    config.subdivisions[beatIndex] = beat["subdivisions"];
    beatIndex++;
  }

  return true;
}

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
  
  setupBLE();
  // Initialize composite video output
  videoOut.begin();
  
  // Reduces screen flicker by copying frame buffer after swap
  videoOut.copyAfterSwap = true;
}

/*
  Event handlers: Will need to be moved to the visual_sub_test_2.ino file due to order of variable declaration
  (specifically, Arduino compiles sketches alphabetically)
*/

// assign event handler for bpm characteristic
MGDisplayBPMCharacteristic.setEventHandler(BLEWritten, MGDisplayBPMCharacteristicWritten);
void MGDisplayBPMCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  bpm = characteristic.value();
}

// assign event handler for numbers of beats characteristic
MGDisplayNumBeatsCharacteristic.setEventHandler(BLEWritten, MGDisplayNumBeatsCharacteristicWritten);
void MGDisplayNumBeatsCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  config.numBeats = characteristic.value();
}


// assign event handler for subdivisions characteristic
MGDisplaySubdivisionsCharacteristic.setEventHandler(BLEWritten, MGDisplaySubdivisionsCharacteristicWritten);
void MGDisplaySubdivisionsCharacteristic(BLEDevice central, BLECharacteristic characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  for (int i = 0;i<config.subdivision.length; i++){
   config.subdivision[i] = int(characteristic.value()[i]);
  }
}

// Uncomment when volume buttons are set-up
// // assign event handler for volume characteristic
// MGDisplayVolumeCharacteristic.setEventHandler(BLEWritten, MGDisplayVolumeCharacteristicWritten);
// void MGDisplayVolumeCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
//   // new value written to characteristc, update the local characteristic
//   Serial.print("Characteristic event, written: ");
//   volume = characteristic.value();
// }


// assign event handler for subdivision audio characteristic
MGDisplaySubAudioCharacteristic.setEventHandler(BLEWritten, MGDisplaySubAudioCharacteristicWritten);
void MGDisplaySubAudioCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  subdivisionAudio = int(characteristic.value());
  
}

// assign event handler for currently playing boolean characteristic
MGDisplayCurrentlyPlayingCharacteristic.setEventHandler(BLEWritten, MGDisplayCurrentlyPlayingCharacteristicWritten);
void MGDisplayCurrentlyPlayingCharacteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  config.isRunning = characteristic.value();
}


/*
 * loop()
 * Main program loop - runs continuously
 * Draws the display and handles beat timing
 */
void loop() {
  // Only update display if running
  if (!config.isRunning) {
    return; 
  }

  // check for Bluetooth events
  checkForBluetoothEvents();

  // Wait for the next video frame (synchronizes drawing with display refresh)
  videoOut.waitForFrame();
  
  // Clear the screen by filling with background colour
  videoOut.fillScreen(bgColor);
  
  /*
   * Calculate current beat (0-3)
   * Uses modulo (%) to cycle through 0, 1, 2, 3, 0, 1, 2, 3...
   * Example: beatCount=5 -> 5 % 4 = 1 (second beat)
   */
  int currentBeat = (beatCount % config.numBeats);

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
   * Each beat is divided into subdivisions (0-3)
   */

  // Calculate current subdivision
  int currentBeatIndex = currentBeat;
  int currentSubdivisions = config.subdivisions[currentBeatIndex];
  int currentSub = 0;

  if(currentSubdivisions > 0){
    unsigned long subInterval = beatInterval / currentSubdivisions;
    unsigned long lastSubTime = millis() - lastBeatTime;
    currentSub = (lastSubTime / subInterval) % currentSubdivisions;
  } 

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
  for (int i = 0; i < config.numBeats; i++) {
    // Calculate x position for this box
    // Each box is offset by (boxWidth + boxSpacing) from the previous one
    int x = startX + (i * (boxWidth + boxSpacing));
    
    if (i == currentBeat && config.isRunning) {
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
    int beatSubdivisions = config.subdivisions[i];
    if (beatSubdivisions > 0) {
      for (int j = 0; j < beatSubdivisions; j++) {
        int totalSubWidth = (beatSubdivisions * subBoxWidth) + ((beatSubdivisions - 1) * subBoxSpacing);
        int subStartX = x + (boxWidth - totalSubWidth) / 2;
        int subX = subStartX + (j * (subBoxWidth + subBoxSpacing));
  
        if( i == currentBeat && j == currentSub && config.isRunning){ 
          videoOut.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, activeColor);
        } else {
          videoOut.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, boxColor);
        }
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
  if ((millis() - lastBeatTime >= beatInterval) && config.isRunning) {
    // Record the time of this beat
    lastBeatTime = millis();
    
    // Increment beat counter
    beatCount++;
    
    // Print beat number to serial monitor for debugging
    Serial.print("Beat: ");
    Serial.println(((beatCount-1) % config.numBeats) + 1);
  }
}