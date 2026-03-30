

// #include <ESP_8_BIT_GFX.h>
// #include <ESP_8_BIT_composite.h>

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
 *   - Native ESP32 BLE libraries from Espressif: 
 * 

 * 
 * Note: AI (Claude) was used to assist with debugging
 */



/*
 * Video output configuration
 * First parameter:  true = NTSC (North America, 30fps), false = PAL (Europe, 25fps) - Analog TV standards
 * Second parameter: 8 = 8-bit colour mode (RGB332 format, 256 colours)
 */
// ESP_8_BIT_GFX ////lcd(true, 8);

/*
 * BPM and timing variables
 * bpm:          Current beats per minute
 * beatCount:    Total number of beats since program started (used to determine current beat 1-4)
 * lastBeatTime: Timestamp (in milliseconds) of when the last beat occurred
 */

#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define LGFX_AUTODETECT // 自動認識 (D-duino-32 XS, WT32-SC01, PyBadge はパネルID読取りが出来ないため自動認識の対象から外れています)

// 複数機種の定義を行うか、LGFX_AUTODETECTを定義することで、実行時にボードを自動認識します。

// ヘッダをincludeします。
#include <LovyanGFX.hpp>

#include <LGFX_AUTODETECT.hpp>  // クラス"LGFX"を準備します
// #include <lgfx_user/LGFX_ESP32_sample.hpp> // またはユーザ自身が用意したLGFXクラスを準備します

// static LGFX lcd;

int bpm = 50;
int beatCount = 0;
unsigned long lastBeatTime = 0;

// Beats configuration
struct BeatConfig {
  int numBeats;
  int beatAudio[4];
  int showSubs[4];                
  int subdivisions[4];
  int subAudio[4][4];           
  bool isRunning;                
};

// Initialize with default values
BeatConfig config = {
  4,
  {0, 0, 0, 0},  
  {0, 0, 0, 0},              
  {0, 0, 0, 0},
  {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
  },   
  false            
};

BLEServer *server = NULL;
BLECharacteristic *ble_bpm = NULL;
BLECharacteristic *ble_meter = NULL;
BLECharacteristic *ble_beatAudio = NULL;
BLECharacteristic *ble_showSubs = NULL;
BLECharacteristic *ble_numSubs = NULL;
BLECharacteristic *ble_subAudio1 = NULL;
BLECharacteristic *ble_subAudio2 = NULL;
BLECharacteristic *ble_isPlaying = NULL;
BLECharacteristic *ble_fromScan = NULL;
BLECharacteristic *ble_scanValues1 = NULL;
BLECharacteristic *ble_scanValues2 = NULL;

BLESecurity *pSecurity = new BLESecurity();

bool deviceConnected = false;
uint32_t value = 0;

#define SERVICE_UUID        "FFFF"

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

bool toBool(String src){
  if (src.equals("true")){
    return true;
  }else{
    return false;
  }
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
uint8_t activeColor = 0x1F;    // Aqua boxes (active)

uint8_t textColor = 0xFF;      // White text
uint8_t bpmColor = 0xFF;       // White for "Current BPM"

uint8_t redColor = 0xE1;        // red/pink for audio if selected
uint8_t greenColor = 0x5C;      // green for audio track if selected
uint8_t yellowColor = 0xFC;    // Yellow 

/*
* SECTION: Characteristic Callbacks
*
* onWrite (BLECharacteristic): update associated metronome logic variable
* with new value from BLECharacteristic that was just written by central
*/
class bpmCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    bpm = pCharacteristic->getValue().toInt(); // Get the new data
  }
};

class meterCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rawString = pCharacteristic->getValue();
    if ((rawString.compareTo("2/4"))==0){
      config.numBeats = 2;

    }else if ((rawString.compareTo("3/4"))==0){
      config.numBeats = 3;

    }else if ((rawString.compareTo("4/4"))==0){
    config.numBeats = 4;
    }
  }
};

class beatAudioCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rawString = pCharacteristic->getValue();
    for (int i =0; i < 4; i++){
      config.beatAudio[i] = rawString.charAt(i) - '0';
    }
  }
};

class showSubsCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String rawString = pCharacteristic->getValue();
    for (int i =3; i > -1; i--){
      int indexTrue = rawString.lastIndexOf('true');
      int indexFalse = rawString.lastIndexOf('false');
      if (indexTrue > indexFalse){
        config.showSubs[i]=true;
        rawString.remove(indexTrue);
      }else{
        config.showSubs[i] = false;
        rawString.remove(indexFalse);
      }
    }
  }
};

class numSubsCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
  String rawString = pCharacteristic->getValue();
    for (int i = 0; i < 4; i++){
      config.subdivisions[i] = rawString.charAt((3*i)+1) - '0';

    }
  }
};

class subAudio1CharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
  String rawString = pCharacteristic->getValue();
  int index = 0;
    for (int i =0; i < 2; i++){
      for (int j = 0; j <4; j++){
        config.subAudio[i][j] = rawString.charAt(index) - '0';
      }
    }
  }
};

class subAudio2CharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
  String rawString = pCharacteristic->getValue();
  int index = 0;
    for (int i =2; i < 4; i++){
      for (int j = 0; j < 4; j++){
        config.subAudio[i][j] = rawString.charAt(index) - '0'; 
      }
    }
  }
};

class isPlayingCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    config.isRunning = toBool(pCharacteristic->getValue());
  }
};

class fromScanCharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {

  }
};

class scanValues1CharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {

  }
};

class scanValues2CharCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {

  }
};

/*
  Function to setup Bluetooth service and characteristics.
  Must be called within the setup() function for the board.

*/
void setupBLE(){
  BLEDevice::init("Metronome_Glasses");
  pSecurity->setCapability(ESP_IO_CAP_NONE);  
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);
  ble_bpm = service->createCharacteristic(
      "FFF0",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE );
  ble_meter = service->createCharacteristic(
      "FFF1",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );                
  ble_beatAudio = service->createCharacteristic(
      "FFF2",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_showSubs = service->createCharacteristic(
      "FFF3",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_numSubs = service->createCharacteristic(
      "FFF4",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_subAudio1 = service->createCharacteristic(
      "FFF5",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_subAudio2 = service->createCharacteristic(
      "FFF6",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_isPlaying = service->createCharacteristic(
      "FFF7",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE 
    );
  ble_fromScan = service->createCharacteristic(
      "FFF8",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE
    );
  ble_scanValues1 = service->createCharacteristic(
      "FFF9",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE
    );
  ble_scanValues2 = service->createCharacteristic(
      "FFFA",
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_WRITE
    );

  
  ble_bpm->addDescriptor(new BLE2902());
  ble_meter->addDescriptor(new BLE2902());
  ble_beatAudio->addDescriptor(new BLE2902());
  ble_showSubs->addDescriptor(new BLE2902());
  ble_numSubs->addDescriptor(new BLE2902());
  ble_subAudio1->addDescriptor(new BLE2902());
  ble_subAudio2->addDescriptor(new BLE2902());
  ble_isPlaying->addDescriptor(new BLE2902());
  ble_fromScan->addDescriptor(new BLE2902());
  ble_scanValues1->addDescriptor(new BLE2902());
  ble_scanValues2->addDescriptor(new BLE2902());

  ble_bpm->setCallbacks(new bpmCharCallbacks());
  ble_meter->setCallbacks(new meterCharCallbacks());
  ble_beatAudio->setCallbacks(new beatAudioCharCallbacks());
  ble_showSubs->setCallbacks(new showSubsCharCallbacks());
  ble_numSubs->setCallbacks(new numSubsCharCallbacks());
  ble_subAudio1->setCallbacks(new subAudio1CharCallbacks());
  ble_subAudio2->setCallbacks(new subAudio2CharCallbacks());
  ble_isPlaying->setCallbacks(new isPlayingCharCallbacks());
  ble_fromScan->setCallbacks(new fromScanCharCallbacks());
  ble_scanValues1->setCallbacks(new scanValues1CharCallbacks());
  ble_scanValues2->setCallbacks(new scanValues2CharCallbacks());

  service->start();
  
  BLEAdvertising *bAdvertising = BLEDevice::getAdvertising();
  bAdvertising->addServiceUUID(SERVICE_UUID);

  bAdvertising->setScanResponse(true);
  bAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  bAdvertising->setMaxPreferred(0x12);  // iPhone connection issue resolution

  BLEDevice::startAdvertising();
}

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
  
  ////lcd.init();
  ////lcd.setRotation(1);
  ////lcd.setBrightness(255);
  //lcd.setColorDepth(24);

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

  // // // Wait for the next video frame (synchronizes drawing with display refresh)
  // // //lcd.waitForFrame();
  
  // // Clear the screen by filling with background colour
  // //lcd.fillScreen(bgColor);
  
  // /*
  //  * Calculate current beat (0-3)
  //  * Uses modulo (%) to cycle through 0, 1, 2, 3, 0, 1, 2, 3...
  //  * Example: beatCount=5 -> 5 % 4 = 1 (second beat)
  //  */
  // int currentBeat = (beatCount % config.numBeats);

  // /*
  //  * Beat timing logic
  //  * 
  //  * Calculate milliseconds between beats:
  //  * beatInterval = 60000ms (1 minute) / bpm
  //  * 
  //  * Example: 50 BPM -> 60000 / 50 = 1200ms between beats
  //  */
  // unsigned long beatInterval = 60000 / bpm;

  // /*
  //  * Subdivision logic
  //  * Each beat is divided into subdivisions (0-3)
  //  */

  // // Calculate current subdivision
  // int currentBeatIndex = currentBeat;
  // int currentSubdivisions = config.subdivisions[currentBeatIndex];
  // int currentSub = 0;

  // if(currentSubdivisions > 0){
  //   unsigned long subInterval = beatInterval / currentSubdivisions;
  //   unsigned long lastSubTime = millis() - lastBeatTime;
  //   currentSub = (lastSubTime / subInterval) % currentSubdivisions;
  // } 

  // /* 
  //  * Beat box dimensions and positioning
  //  * 
  //  * Screen width is 256 pixels
  //  * Total width of boxes: (4 boxes * 50) + (3 gaps * 10) = 230 pixels
  //  * Starting X position: (256 - 230) / 2 = 13 pixels (centres the boxes)
  //  */
  // int boxWidth = 50;
  // int boxHeight = 60;
  // int boxSpacing = 10;
  // int startX = 13;
  // int boxY = 30;

  // // Subdivision box dimensions
  // int subBoxWidth = 10;
  // int subBoxHeight = 10;
  // int subBoxSpacing = 2;
  // int subBoxY = boxY + boxHeight + 5;  
  
  // // void cycleBeatMode(int index, int subIndex) {
  // //   if (subIndex == -1) {
  // //     beatAudio[index] = (beatAudio[index] + 1) % 5;
  // //   } else {
  // //     subAudio[index][subIndex] = (subAudio[index][subIndex] + 1) % 5;
  // //   }
  // // }

  // /*
  //  * Draw 4 beat boxes and subdivisions
  //  * Loop through each box (i = 0, 1, 2, 3)
  //  */
  // for (int i = 0; i < config.numBeats; i++) {
  //   // Calculate x position for this box
  //   // Each box is offset by (boxWidth + boxSpacing) from the previous one
  //   int x = startX + (i * (boxWidth + boxSpacing));
    
  //   if (i == currentBeat && config.isRunning) {
  //     // Active beat - draw yellow filled rectangle
  //     //lcd.fillRect(x, boxY, boxWidth, boxHeight, activeColor);
  //     //lcd.setTextColor(0x00);  // Black text for contrast on yellow
  //   } else {
  //     // Inactive beat - draw blue filled rectangle
  //     //lcd.fillRect(x, boxY, boxWidth, boxHeight, boxColor);
  //     //lcd.setTextColor(0xFF);  // White text for contrast on blue
  //   }
    
    // Draw beat number (1, 2, 3, or 4) inside the box
    //lcd.setTextSize(2);
    // Position text roughly centred in the box
    //lcd.setCursor(x + 18, boxY + 22);
    //lcd.print(i + 1);  // i+1 converts 0-3 to 1-4

  //   // Draw subdiviosion boxes
  //   int beatSubdivisions = config.subdivisions[i];
  //   if (beatSubdivisions > 0) {
  //     for (int j = 0; j < beatSubdivisions; j++) {
  //       int totalSubWidth = (beatSubdivisions * subBoxWidth) + ((beatSubdivisions - 1) * subBoxSpacing);
  //       int subStartX = x + (boxWidth - totalSubWidth) / 2;
  //       int subX = subStartX + (j * (subBoxWidth + subBoxSpacing));
  
  //       if( i == currentBeat && j == currentSub && config.isRunning){ 
  //         //lcd.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, activeColor);
  //       } else {
  //         //lcd.fillRect(subX, subBoxY, subBoxWidth, subBoxHeight, boxColor);
  //       }
  //     }
  //   }
  // }
  
  /*
   * Draw "Current BPM:" label
   */
  //lcd.setTextColor(bpmColor);
  //lcd.setTextSize(2);
  //lcd.setCursor(50, 110);
  //lcd.print("Current BPM:");
  
  /*
   * Draw BPM number
   * Adjusts x position based on number of digits to keep it centred
   */
  //lcd.setTextColor(textColor);
  //lcd.setTextSize(4);  // Larger text for BPM value
  
  // if (bpm < 100) {
  //   // Two digits - position further right
  //   //lcd.setCursor(95, 145);
  // } else {
  //   // Three digits - position further left
  //   //lcd.setCursor(75, 145);
  // }
  //lcd.print(bpm);
  
  /*
   * Check if it is time for the next beat
   * millis() returns milliseconds since ESP32 started
   * If enough time has passed since last beat, trigger next beat
   */
  // if ((millis() - lastBeatTime >= beatInterval) && config.isRunning) {
  //   // Record the time of this beat
  //   lastBeatTime = millis();
    
  //   // Increment beat counter
  //   beatCount++;
    
  //   // Print beat number to serial monitor for debugging
  //   // Serial.print("Beat: ");
  //   // Serial.println(((beatCount-1) % config.numBeats) + 1);
  // }
}