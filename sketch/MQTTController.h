#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Definição dos sufixos dos tópicos baseada na especificação
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

// Struct limpa para injeção de dependências
struct MQTTConfig {
  const char* wifiSSID;
  const char* wifiPassword;
  const char* mqttBroker;
  uint16_t    mqttPort;
  const char* mqttUser;
  const char* mqttPassword;
  const char* topicPrefix;     // ex: "uffs/eduardo/"
  const char* clientId;        // ex: "ESP32_Eduardo"
};

typedef void (*LedsCommandCallback)(bool led1, bool led2, bool resetMinMax);
typedef void (*RgbCommandCallback)(uint8_t r, uint8_t g, uint8_t b);
typedef void (*OnClientConnectCallback)();
typedef void (*OnClientDisconnectCallback)();

enum class EConnectionStatus {
  DISCONNECTED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  BROKER_CONNECTING, // Alterado
  BROKER_CONNECTED   // Alterado
};


class MQTTController {
private:
  MQTTConfig      _config;
  WiFiClient      _espClient;
  PubSubClient    _mqttClient;

  EConnectionStatus _connectionState = EConnectionStatus::DISCONNECTED;
  unsigned long   _lastReconnectAttempt = 0;
  const unsigned long _reconnectInterval = 5000; // 5 segundos entre tentativas (Não-bloqueante)

  // Geração dinâmica dos tópicos completos em memória
  String _topicTempC, _topicTempF, _topicHum, _topicHist, _topicRssi, _topicStatus;
  String _topicLedsState, _topicLedsCmd, _topicRgbCmd, _topicLockState, _topicResetCmd;

  // Controle de publicações por minuto (equivalente ao notify do BLE)
  uint16_t        _publishBuckets[60] = {0}; 
  uint8_t         _currentBucketIndex = 0;               
  unsigned long   _lastBucketShiftTime = 0;
  void            _registerPublish();               
  void            _updatePublishWindow();

  // Callbacks
  LedsCommandCallback           _onLedsCommand = nullptr;
  RgbCommandCallback            _onRgbCommand = nullptr;
  OnClientConnectCallback       _onClientConnectCallback = nullptr;
  OnClientDisconnectCallback    _onClientDisconnectCallback = nullptr;

  // Métodos internos de gestão da máquina de estados
  void            _buildTopics();
  void            _handleWiFiConnection();
  void            _handleMQTTConnection();
  static void     _mqttCallbackWrapper(char* topic, byte* payload, unsigned int length); // Estático para o PubSubClient
  void            _processIncomingMessage(String topic, String payload);

  // Instância global para o wrapper estático conseguir acessar os membros da classe
  static MQTTController* _instance; 

public: 
  // O Construtor agora exige as configurações de injeção
  MQTTController(MQTTConfig config);
  
  void begin();
  void update(); // Chamado no loop() - O coração não-bloqueante da classe
  
  // Substituímos o isAdvertising pelas flags reais
  bool isWiFiConnected();
  bool isMQTTConnected();
  EConnectionStatus getState();
  
  // Envio de telemetria
  void sendAmbientData(float temperature, float humidity);
  void sendHistoryData(String jsonPayload); // Método novo para o histórico
  void sendConfigData(bool lockSimpleLeds, bool measure);
  void sendLocalLedsState(bool led1, bool led2, bool resetMinMax);
  void sendRssi();

  // Callback setters
  void setLedsCallback(LedsCommandCallback cb);
  void setRgbCallback(RgbCommandCallback cb);
  void setClientConnectCallback(OnClientConnectCallback cb);
  void setClientDisconnectCallback(OnClientDisconnectCallback cb);
};

#endif // MQTT_CONTROLLER_H