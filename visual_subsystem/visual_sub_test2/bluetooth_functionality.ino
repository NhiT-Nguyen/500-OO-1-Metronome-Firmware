#include <ArduinoBLE.h>

/*
  You can use a generic Bluetooth® Low Energy central app, like LightBlue (iOS and Android) or
  nRF Connect (Android), to interact with the services and characteristics
  created in this sketch.

  Format adapted from example code from ArduinoBLE library (Button LED)
*/
// set pins for volume up/down buttons
// const int volumeUpPin = ; // set volumeUpPin to digital pin #:
// const int volumeDownPin = ; // set volumeDownPin to digital pin #:

/*
 create service that manages metronome glasses characteristics
 Note: UUID generated with: https://www.uuidgenerator.net/version4
*/
BLEService MGDisplayService("da1e24f0-e3c4-472f-8d4b-0e3390344fb4");

void bleIntChracteristicWritten(BLEDevice central, BLECharacteristic bleCharacteristic, int* characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  *characteristic = bleCharacteristic.value();
}

void bleWordChracteristicWritten(BLEDevice central, BLECharacteristic bleCharacteristic, string* characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  *characteristic = word(bleCharacteristic.value());
}

void bleBoolChracteristicWritten(BLEDevice central, BLECharacteristic bleCharacteristic, bool* characteristic) {
  // new value written to characteristc, update the local characteristic
  Serial.print("Characteristic event, written: ");
  *characteristic = bleCharacteristic.value();
}

{
// create bpm characteristic and allow remote device to read and write
BLEIntCharacteristic MGDisplayBPMCharacteristic("da1e24f1-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplayBPMCharacteristic.setEventHandler(BLEWritten, bleIntChracteristicWritten);


// create number of beats characteristic and allow remote device to read and write
BLEIntCharacteristic MGDisplayNumBeatsCharacteristic("da1e24f2-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplayNumBeatsCharacteristic.setEventHandler(BLEWritten, bleIntChracteristicWritten);


// create subdivisions characteristic and allow remote device to read and write
BLEWordCharacteristic MGDisplaySubdivisionsCharacteristic("da1e24f3-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplaySubdivisionsCharacteristic.setEventHandler(BLEWritten, bleWordChracteristicWritten);


// create volume characteristic and allow remote device to read and write
BLEIntCharacteristic MGDisplayVolumeCharacteristic("da1e24f4-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplayVolumeCharacteristic.setEventHandler(BLEWritten, bleIntChracteristicWritten);


// create subdivision audio characteristic and allow remote device to read and write
BLEWordCharacteristic MGDisplaySubAudioCharacteristic("da1e24f5-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplaySubAudioCharacteristic.setEventHandler(BLEWritten, bleWordChracteristicWritten);

// create currently playing boolean characteristic and allow remote device to read and write
BLEBoolCharacteristic MGDisplayCurrentlyPlayingCharacteristic("da1e24f6-e3c4-472f-8d4b-0e3390344fb4", BLERead | BLEWrite);
// assign event handler for characteristic
MGDisplayCurrentlyPlayingCharacteristic.setEventHandler(BLEWritten, bleBoolChracteristicWritten);
}

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

  // set the UUID for metronome glasses service that this peripheral advertises:
  BLE.setAdvertisedService(MGDisplayService);

  // add the characteristics to the service
  MGDisplayService.addCharacteristic(MGDisplayBPMCharacteristic);
  MGDisplayService.addCharacteristic(MGDisplayNumBeatsCharacteristic);
  MGDisplayService.addCharacteristic(MGDisplaySubdivisionsCharacteristic);
  MGDisplayService.addCharacteristic(MGDisplayVolumeCharacteristic);
  MGDisplayService.addCharacteristic(MGDisplaySubAudioCharacteristic);
  MGDisplayService.addCharacteristic(MGDisplayCurrentlyPlayingCharacteristic);

  // add the service to the Bluetooth Device (ESP32)
  BLE.addService(MGDisplayService);

  // set intial values for each Bluetooth characteristic
  MGDisplayBPMCharacteristic.writeValue(40);
  MGDisplayNumBeatsCharacteristic.writeValue(4);
  MGDisplaySubdivisionsCharacteristic.writeValue("");
  MGDisplayVolumeCharacteristic.writeValue(0);
  MGDisplaySubAudioCharacteristic.writeValue("");
  MGDisplayCurrentlyPlayingCharacteristic.writeValue(false);

  // start advertising
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
}


/*
  function to poll for Bluetooth events/notifications from the remote device.
  must be called during the main loop() of the board program.

*/
void checkForBluetoothEvents(){
  // poll for BLE events
  BLE.poll();

  // read value of volume buttons
  // char upButtonValue = digitalRead(volumeUpPin);
  // char downButtonValue = digitalRead(volumeDownPin);

}
