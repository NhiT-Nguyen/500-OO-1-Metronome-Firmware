
// // #include <BLEDevice.h>
// // #include <BLESecurity.h>
// // #include <BLEServer.h>
// // #include <BLEUtils.h>
// // #include <BLE2902.h>

// // #define LGFX_AUTODETECT // 自動認識 (D-duino-32 XS, WT32-SC01, PyBadge はパネルID読取りが出来ないため自動認識の対象から外れています)

// // // 複数機種の定義を行うか、LGFX_AUTODETECTを定義することで、実行時にボードを自動認識します。

// // // ヘッダをincludeします。
// // #include <LovyanGFX.hpp>

// // #include <LGFX_AUTODETECT.hpp>  // クラス"LGFX"を準備します
// // // #include <lgfx_user/LGFX_ESP32_sample.hpp> // またはユーザ自身が用意したLGFXクラスを準備します

// // static LGFX lcd;  

// // BLEServer *server = NULL;
// // BLECharacteristic *ble_bpm = NULL;
// // BLECharacteristic *ble_meter = NULL;
// // BLECharacteristic *ble_beatAudio = NULL;
// // BLECharacteristic *ble_showSubs = NULL;
// // BLECharacteristic *ble_numSubs = NULL;
// // BLECharacteristic *ble_subAudio1 = NULL;
// // BLECharacteristic *ble_subAudio2 = NULL;
// // BLECharacteristic *ble_isPlaying = NULL;
// // BLECharacteristic *ble_fromScan = NULL;
// // BLECharacteristic *ble_scanValues1 = NULL;
// // BLECharacteristic *ble_scanValues2 = NULL;

// // BLESecurity *pSecurity = new BLESecurity();

// // bool deviceConnected = false;
// // uint32_t value = 0;

// // #define SERVICE_UUID        "FFFF"

// // class ServerCallbacks: public BLEServerCallbacks {
// //     void onConnect(BLEServer* pServer) {
// //       deviceConnected = true;
// //     };
// //     void onDisconnect(BLEServer* pServer) {
// //       deviceConnected = false;
// //     }
// // };

// // bool toBool(String *src){
// //   if (*src.equals("true")){
// //     return true;
// //   else
// //     return false;
// //   }
// // }

// /*
// * SECTION: Characteristic Callbacks
// *
// * onWrite (BLECharacteristic): update associated metronome logic variable
// * with new value from BLECharacteristic that was just written by central
// */
// class bpmCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     bpm = pCharacteristic->getValue().c_str().toInt(); // Get the new data
//   }
// };

// class meterCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     String rawString = pCharacteristic->getValue().c_str();
//     if ((rawString.compareTo("2/4"))==0){
//       config->numBeats = 2;

//     }else if ((rawString.compareTo("3/4"))==0){
//       config->numBeats = 3;

//     }else if ((rawString.compareTo("4/4"))==0){
//     config->numBeats = 4;
//     }
//   }
// };

// class beatAudioCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     rawString = pCharacteristic->getValue().c_str();
//     for (int i =0; i < 4; i++){
//       config->beatAudio[i] = rawString.charAt(i).toInt();
//     }
//   }
// };

// class showSubsCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     rawString = pCharacteristic->getValue().c_str();
//     for (int i =3; i > -1; i--){
//       indexTrue = rawString.lastIndexOf('true');
//       indexFalse = rawString.lastIndexOf('false');
//       if (indexTrue > indexFalse){
//         config->showSubs[i]=true;
//         rawString.remove(indexTrue);
//       }else{
//         config->showSubs[i] = false;
//         rawString.remove(indexFalse);
//       }
//     }
//   }
// };

// class numSubsCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//   rawString = pCharacteristic->getValue().c_str();
//     for (int i = 0; i < 4; i++){
//       config->subdivisions[i] = rawString.charAt((3*i)+1).toInt();

//     }
//   }
// };

// class subAudio1CharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//   rawString = pCharacteristic->getValue().c_str();
//   int index = 0;
//     for (int i =0; i < 2; i++){
//       for (int j = 0; j <4; j++){
//         config->subAudio[i][j] = rawString.charAt(index).toInt();
//       }
//     }
//   }
// };

// class subAudio2CharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//   rawString = pCharacteristic->getValue().c_str();
//   int index = 0;
//     for (int i =2; i < 4; i++){
//       for (int j = 0; j < 4; j++){
//         config->subAudio[i][j] = rawString.charAt(index).toInt();
//       }
//     }
//   }
// };

// class isPlayingCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {
//     config->isRunning = toBool(pCharacteristic->getValue().c_str());
//   }
// };

// class fromScanCharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {

//   }
// };

// class scanValues1CharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {

//   }
// };

// class scanValues1CharCallbacks: public BLECharacteristicCallbacks {
//   void onWrite(BLECharacteristic *pCharacteristic) {

//   }
// };

// /*
//   Function to setup Bluetooth service and characteristics.
//   Must be called within the setup() function for the board.

// */
// void setupBLE(){
//   BLEDevice::init("Metronome_Glasses");
//   pSecurity->setCapability(ESP_IO_CAP_NONE);  
//   pSecurity->setAuthenticationMode(ESP_LE_AUTH_NO_BOND);
//   server = BLEDevice::createServer();
//   server->setCallbacks(new ServerCallbacks());

//   BLEService *service = server->createService(SERVICE_UUID);
//   ble_bpm = service->createCharacteristic(
//       "FFF0",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE );
//   ble_meter = service->createCharacteristic(
//       "FFF1",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );                
//   ble_beatAudio = service->createCharacteristic(
//       "FFF2",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_showSubs = service->createCharacteristic(
//       "FFF3",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_numSubs = service->createCharacteristic(
//       "FFF4",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_subAudio1 = service->createCharacteristic(
//       "FFF5",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_subAudio2 = service->createCharacteristic(
//       "FFF6",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_isPlaying = service->createCharacteristic(
//       "FFF7",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE 
//     );
//   ble_fromScan = service->createCharacteristic(
//       "FFF8",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE
//     );
//   ble_scanValues1 = service->createCharacteristic(
//       "FFF9",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE
//     );
//   ble_scanValues2 = service->createCharacteristic(
//       "FFFA",
//       BLECharacteristic::PROPERTY_READ | 
//       BLECharacteristic::PROPERTY_WRITE
//     );

  
//   ble_bpm->addDescriptor(new BLE2902());
//   ble_meter->addDescriptor(new BLE2902());
//   ble_beatAudio->addDescriptor(new BLE2902());
//   ble_showSubs->addDescriptor(new BLE2902());
//   ble_numSubs->addDescriptor(new BLE2902());
//   ble_subAudio1->addDescriptor(new BLE2902());
//   ble_subAudio2->addDescriptor(new BLE2902());
//   ble_isPlaying->addDescriptor(new BLE2902());
//   ble_fromScan->addDescriptor(new BLE2902());
//   ble_scanValues1->addDescriptor(new BLE2902());
//   ble_scanValues2->addDescriptor(new BLE2902());

//   ble_bpm->setCallbacks(new bpmCharCallbacks());
//   ble_meter->setCallbacks(new meterCharCallbacks());
//   ble_beatAudio->setCallbacks(new beatAudioCharCallbacks());
//   ble_showSubs->setCallbacks(new showSubsCharCallbacks());
//   ble_numSubs->setCallbacks(new numSubsCharCallbacks());
//   ble_subAudio1->setCallbacks(new subAudio1CharCallbacks());
//   ble_subAudio2->setCallbacks(new subAudio2CharCallbacks());
//   ble_isPlaying->setCallbacks(new isPlayingCharCallbacks());
//   ble_fromScan->setCallbacks(new fromScanCharCallbacks());
//   ble_scanValues1->setCallbacks(new scanValues1CharCallbacks());
//   ble_scanValues2->setCallbacks(new scanValues2CharCallbacks());

//   service->start();
  
//   BLEAdvertising *bAdvertising = BLEDevice::getAdvertising();
//   bAdvertising->addServiceUUID(SERVICE_UUID);

//   bAdvertising->setScanResponse(true);
//   bAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
//   bAdvertising->setMaxPreferred(0x12);  // iPhone connection issue resolution

//   BLEDevice::startAdvertising();
// }



