#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "esp_eap_client.h"

#define TOPIC_SUFFIX_TEMP_C       "temperatura/celsius"
#define TOPIC_SUFFIX_TEMP_F       "temperatura/fahrenheit"
#define TOPIC_SUFFIX_HUM          "umidade"
#define TOPIC_SUFFIX_HIST         "historico"
#define TOPIC_SUFFIX_RSSI         "rssi"
#define TOPIC_SUFFIX_STATUS       "status"
#define TOPIC_SUFFIX_LEDS_STATE   "leds/estado"
#define TOPIC_SUFFIX_LEDS_CMD     "leds/comando"
#define TOPIC_SUFFIX_RGB_CMD      "rgb/comando"
#define TOPIC_SUFFIX_LOCK_STATE   "controle/bloqueio"
#define TOPIC_SUFFIX_RESET_CMD    "controle/reset"

#define MAX_PUB_BUFFER_SIZE       1024

struct MQTTConfig {
  bool        isEduroam;
  const char* wifiSSID;
  const char* wifiUser;
  const char* wifiPassword;
  const char* mqttBroker;
  uint16_t    mqttPort;
  const char* mqttUser;
  const char* mqttPassword;
  const char* topicPrefix;     // ex: "uffs/eduardo/"
  const char* clientId;        // ex: "ESP32_Eduardo"
  bool        useTLS;
  const char* rootCA;
};

typedef void (*LedsCommandCallback)(bool led1, bool led2, bool resetMinMax);
typedef void (*RgbCommandCallback)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*OnClientConnectCallback)();
typedef void (*OnClientDisconnectCallback)();

enum class EConnectionStatus {
  DISCONNECTED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  BROKER_CONNECTING, 
  BROKER_CONNECTED   
};

class MQTTController {
private:
  MQTTConfig          _config;
  WiFiClient          _wifiClient;
  WiFiClientSecure    _wifiClientSecure;
  PubSubClient        _mqttClient;

  EConnectionStatus _connectionState = EConnectionStatus::DISCONNECTED;
  unsigned long   _lastReconnectAttempt = 0;
  const unsigned long _reconnectInterval = 5000; 

  String _topicTempC, _topicTempF, _topicHum, _topicHist, _topicRssi, _topicStatus;
  String _topicLedsState, _topicLedsCmd, _topicRgbCmd, _topicLockState, _topicResetCmd;

  uint16_t        _publishBuckets[60] = {0}; 
  uint8_t         _currentBucketIndex = 0;               
  unsigned long   _lastBucketShiftTime = 0;
  void            _registerPublish();               
  void            _updatePublishWindow();

  LedsCommandCallback           _onLedsCommand = nullptr;
  RgbCommandCallback            _onRgbCommand = nullptr;
  OnClientConnectCallback       _onClientConnectCallback = nullptr;
  OnClientDisconnectCallback    _onClientDisconnectCallback = nullptr;

  void            _buildTopics();
  void            _handleWiFiConnection();
  void            _handleMQTTConnection();
  static void     _mqttCallbackWrapper(char* topic, byte* payload, unsigned int length); 
  void            _processIncomingMessage(String topic, String payload);

  static MQTTController* _instance; 

public: 
  MQTTController(MQTTConfig config);
  
  void begin();
  void update(); 
  
  bool isWiFiConnected();
  bool isMQTTConnected();
  EConnectionStatus getState();
  
  void sendAmbientData(float temperature, float humidity);
  void sendHistoryData(String jsonPayload); 
  void sendConfigData(bool lockSimpleLeds, bool measure);
  void sendLocalLedsState(bool led1, bool led2, bool resetMinMax);
  void sendRssi();

  void setLedsCallback(LedsCommandCallback cb);
  void setRgbCallback(RgbCommandCallback cb);
  void setClientConnectCallback(OnClientConnectCallback cb);
  void setClientDisconnectCallback(OnClientDisconnectCallback cb);
};

#endif // MQTT_CONTROLLER_H