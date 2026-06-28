#ifndef BLE_H
#define BLE_H

#include <NimBLEDevice.h>
#include <Arduino.h>

#define BLE_NAME_ADVERTISING                "ESP32_NimBLE_Eduardo"
#define BLE_ADVERTISING_INTERVAL            400
#define BLE_MIN_CONNECT_INTERVAL            50
#define BLE_MAX_CONNECT_INTERVAL            100
#define BLE_SLAVE_LATENCI                   0
#define BLE_SUPERVISION_TIMEOUT             2000
#define BLE_MTU_LEN                         512     // tamanho máximo de pacote
          
#define BLE_PASSWORD                        666123

#define BLE_SERVICE_UID_ENV_MONITORING      "181A"
#define BLE_SERVICE_UID_ACTUATOR_CONTROLL   "d6ca719a-7ae1-485a-bf63-ac03fdf84527"
#define BLE_SERVICE_UID_CONNECT_INDICATOR   "e01c7a1d-8c40-428c-ba7b-a7f7980120b8"

// Chars from env monitoring service
#define BLE_CHAR_UID_ACTUAL_DATA            "f52de2f0-5d97-42f3-933f-6b299132861d"
#define BLE_CHAR_UID_HYST_GRAPHIC           "614e74cf-1814-45fc-8602-3263376705e3"

// Chars from actuators controll service
#define BLE_CHAR_UID_SIMPLE_LEDS            "1384d4f8-05b5-4d0e-8d4b-ecfa8a2ee4eb"
#define BLE_CHAR_UID_RGB_LED                "2214794a-21a9-4cb3-bc1d-d10aa147fad8"
#define BLE_CHAR_UID_DEVICE_CONFIG          "fc949d8b-c71e-4ee7-84a0-5c1fd772a999"

// Chars from connect indicators service
#define BLE_CHAR_UID_RSSI                   "fe4e82d1-ea3e-43af-a72d-9b7622a8113c"
#define BLE_CHAR_UID_NOTIFY_COUNT           "9ecd2413-1a5e-490a-993c-da9f6f1259f9"

typedef void (*LedsCommandCallback)(bool led1, bool led2, bool resetMinMax);
typedef void (*RgbCommandCallback)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*OnClientConnectCallback)();
typedef void (*OnClientDisconnectCallback)();

class BleController {
private:
  NimBLEServer* server = NULL;

  NimBLEService *envMonitoringService = NULL;
  NimBLEService *actuatorControllService = NULL;
  NimBLEService *connectIndicatorService = NULL;
  
  NimBLEServerCallbacks* customBLEServerCallback = NULL;
  NimBLECharacteristicCallbacks* customBLECharCallback = NULL;

  NimBLECharacteristic* dataCharacteristic = NULL;          
  NimBLECharacteristic* dataHystCharacteristic = NULL;      
  NimBLECharacteristic* ledsCharacteristic = NULL;          
  NimBLECharacteristic* rgbLedCharacteristic = NULL;        
  NimBLECharacteristic* rssiCharacteristic = NULL;          
  NimBLECharacteristic* notifyCountCharacteristic = NULL;   
  NimBLECharacteristic* deviceConfigCharacteristic = NULL;   

  
  NimBLEAdvertising *pAdvertising = NULL;

  bool deviceConnected = false;

  uint16_t notifyBuckets[60] = {0}; // vetor circular - 1 posição para cada segundo
  uint8_t currentBucketIndex = 0;   // Aponta para o "segundo" atual
  unsigned long lastBucketShiftTime = 0;

  unsigned long lastRssiNotifyTime = 0;
  
  void registerNotification();    // Método interno para somar o contador de notificações

public: 
  BleController();
  void begin();
  bool isAdvertising();
  bool hasDeviceConnected();
  void setTemperature(float temp, float humidity);
  void sendAmbientData(float temperature, float humidity);
  void sendConfigData(bool lockSimpleLeds, bool measure);
  void processIndicators(); // Método a ser chamado no loop do Arduino
  void sendLocalLedsState(bool led1, bool led2, bool resetMinMax);
  void notifyRssi();
  void updateNotificationWindow();

  // controle de callbacks usados para enviar as informações
  // do callback de char para a classe do led 
  LedsCommandCallback     onLedsCommand = nullptr;
  RgbCommandCallback      onRgbCommand = nullptr;
  OnClientConnectCallback onClientConnectCallback = nullptr;
  OnClientDisconnectCallback onClientDisconnectCallback = nullptr;

  void setLedsCallback(LedsCommandCallback cb);
  void setRgbCallback(RgbCommandCallback cb);
  void setClientConnectCallback(OnClientConnectCallback cb);
  void setClientDisconnectCallback(OnClientDisconnectCallback cb);

};

#endif  // BLE_H