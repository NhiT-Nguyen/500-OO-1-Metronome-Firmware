#include <ArduinoBLE.h>
#include "ESP_8_BIT_GFX.h"
#include <ArduinoJson.h>

/*
  You can use a generic Bluetooth® Low Energy central app, like LightBlue (iOS and Android) or
  nRF Connect (Android), to interact with the services and characteristics
  created in this sketch.

  Format adapted from example code from ArduinoBLE library (Button LED)
*/

/*
 create service that manages metronome glasses characteristics
*/
BLEService metronome_glasses("FFFF");

// Begin code: EnhancedAdvertising.ino example
// Advertising parameters should have a global scope. Do NOT define them in 'setup' or in 'loop'
const uint8_t manufactData[4] = {0x01, 0x02, 0x03, 0x04};
const uint8_t serviceData[3] = {0x00, 0x01, 0x02};
// End code: EnhancedAdvertising.ino


// create bpm characteristic and allow remote device to read and write
BLEStringCharacteristic ble_bpm("FFF0", BLERead | BLEWrite | BLEWriteWithoutResponse);

// create number of beats characteristic and allow remote device to read and write
BLEStringCharacteristic ble_meter("FFF1", BLERead | BLEWrite | BLEWriteWithoutResponse);

// create beat audio characteristic and allow remote device to read and write
BLEStringCharacteristic ble_beatAudio("FFF2", BLERead | BLEWrite | BLEWriteWithoutResponse);

// create subdivisions enabled/disabled characteristic and allow remote device to read and write
BLEStringCharacteristic ble_showSubs("FFF3", BLERead | BLEWrite | BLEWriteWithoutResponse);

// create subdivision number beat characteristic and allow remote device to read and write
BLEStringCharacteristic ble_numSubs("FFF4", BLERead | BLEWrite | BLEWriteWithoutResponse);

/// Create subdivision audio characteristic and allow remote device to read and write.
// This characteristic is 1/2 and holds the audio data for the beats 1 & 2
BLEStringCharacteristic ble_subAudio1("FFF5", BLERead | BLEWrite | BLEWriteWithoutResponse);

/// Create subdivision audio characteristic and allow remote device to read and write.
// This characteristic is 2/2 and holds the audio data for the beats 3 & 4 if the current meter
// has beats 3 & 4
BLEStringCharacteristic ble_subAudio2("FFF6", BLERead | BLEWrite | BLEWriteWithoutResponse);

// Characteristic that holds the value of whether or not the central BLE device is currently playing
BLEStringCharacteristic ble_isPlaying("FFF7", BLERead | BLEWrite | BLEWriteWithoutResponse);

// Characteristic that holds the true/false values of whether the app
// is running metronome data from scanned music
BLEStringCharacteristic ble_fromScan("FFF8", BLERead | BLEWrite | BLEWriteWithoutResponse);

// Characteristic that holds the scanned information from the sheet music
// 1/2 characteristic, as some scanned information may be long if there
// are multiple meter changes
BLEStringCharacteristic ble_scanValues1("FFF9", BLERead | BLEWrite | BLEWriteWithoutResponse);

// Characteristic that holds the scanned information from the sheet music
// 2/2 characteristic, as some scanned information may be long if there
// are multiple meter changes
BLEStringCharacteristic ble_scanValues2("FFFA", BLERead | BLEWrite | BLEWriteWithoutResponse);


/*
  Function to setup Bluetooth service and characteristics.
  Must be called within the setup() function for the board.

*/
void setupBLE(){
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1);
  }

  // disconnect any previously connected BLE remote device
  if (BLE.connected()){
    BLE.disconnect();
  }

  // set the local name peripheral advertises
  BLE.setLocalName("Metronome_Glasses");

  // set the device name in the built in device name characteristic
  BLE.setDeviceName("Metronome_Glasses");
  
  // add the characteristics to the service
  metronome_glasses.addCharacteristic(ble_bpm);
  metronome_glasses.addCharacteristic(ble_meter);
  metronome_glasses.addCharacteristic(ble_beatAudio);
  metronome_glasses.addCharacteristic(ble_showSubs);
  metronome_glasses.addCharacteristic(ble_numSubs);
  metronome_glasses.addCharacteristic(ble_subAudio1);
  metronome_glasses.addCharacteristic(ble_subAudio2);
  metronome_glasses.addCharacteristic(ble_isPlaying);
  metronome_glasses.addCharacteristic(ble_fromScan);
  metronome_glasses.addCharacteristic(ble_scanValues1);
  metronome_glasses.addCharacteristic(ble_scanValues2);
 

  // add the service to the Bluetooth Device (ESP32)
  BLE.addService(metronome_glasses);

  //Code from EnhancedAdvertising.ino..
   // Build scan response data packet
  BLEAdvertisingData scanData;
  // Set parameters for scan response packet
  scanData.setLocalName("Scan Response Data");
  // Copy set parameters in the actual scan response packet
  BLE.setScanResponseData(scanData);

  // Build advertising data packet
  BLEAdvertisingData advData;

  // Set parameters for advertising packet
  advData.setManufacturerData(0x0001, manufactData, sizeof(manufactData));
  advData.setAdvertisedService(metronome_glasses);
  advData.setAdvertisedServiceData(0xfff0, serviceData, sizeof(serviceData));

  // Copy set parameters in the actual advertising packet
  BLE.setAdvertisingData(advData);

  //.. end code

  // start advertising
  BLE.setAdvertisedServiceUuid("FFFF");
  BLE.setConnectable;
  BLE.setPairable;
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");

 if (BLE.connected()){
  // listen for Bluetooth® Low Energy peripherals to connect:
  BLEDevice central = BLE.central();

  // if a central is connected to peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's MAC address:
    Serial.println(central.address());
  }
    BLE.stopAdvertise();
  }
}


/*
  function to poll for Bluetooth events/notifications from the remote device.
  must be called during the main loop() of the board program.

*/
void checkForBluetoothEvents(){
  // poll for BLE events
  BLE.poll();
}


