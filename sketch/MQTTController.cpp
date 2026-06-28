#include "MQTTController.h"

// Inicialização do ponteiro estático necessário para o callback da PubSubClient
MQTTController* MQTTController::_instance = nullptr;

MQTTController::MQTTController(MQTTConfig config) : _config(config) {
  _instance = this;
}

void MQTTController::begin() {
  _buildTopics();
  
  WiFi.mode(WIFI_STA);
  
  // Configura o cliente MQTT
  _mqttClient.setClient(_espClient);
  _mqttClient.setServer(_config.mqttBroker, _config.mqttPort);
  _mqttClient.setCallback(MQTTController::_mqttCallbackWrapper);
  
  _connectionState = EConnectionStatus::DISCONNECTED;
  _lastReconnectAttempt = 0;
  
  Serial.println("[MQTTController] Inicializado. Aguardando processamento da rede...");
}

void MQTTController::_buildTopics() {
  String prefix = String(_config.topicPrefix);
  _topicTempC       = prefix + TOPIC_SUFFIX_TEMP_C;
  _topicTempF       = prefix + TOPIC_SUFFIX_TEMP_F;
  _topicHum         = prefix + TOPIC_SUFFIX_HUM;
  _topicHist        = prefix + TOPIC_SUFFIX_HIST;
  _topicRssi        = prefix + TOPIC_SUFFIX_RSSI;
  _topicStatus      = prefix + TOPIC_SUFFIX_STATUS;
  _topicLedsState   = prefix + TOPIC_SUFFIX_LEDS_STATE;
  _topicLedsCmd     = prefix + TOPIC_SUFFIX_LEDS_CMD;
  _topicRgbCmd      = prefix + TOPIC_SUFFIX_RGB_CMD;
  _topicLockState   = prefix + TOPIC_SUFFIX_LOCK_STATE;
  _topicResetCmd    = prefix + TOPIC_SUFFIX_RESET_CMD;
}

void MQTTController::update() {
  _updatePublishWindow();
  switch (_connectionState) {
    
    case EConnectionStatus::DISCONNECTED:
      if (millis() - _lastReconnectAttempt >= _reconnectInterval || _lastReconnectAttempt == 0) {
        _lastReconnectAttempt = millis();
        Serial.printf("[Wi-Fi] Tentando conectar ao SSID: %s...\n", _config.wifiSSID);
        WiFi.begin(_config.wifiSSID, _config.wifiPassword);
        _connectionState = EConnectionStatus::WIFI_CONNECTING;
      }
      break;

    case EConnectionStatus::WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[Wi-Fi] Conectado! IP: ");
        Serial.println(WiFi.localIP());
        _connectionState = EConnectionStatus::WIFI_CONNECTED;
      } 
      // Timeout de 10 segundos para não ficar preso na tentativa de Wi-Fi
      else if (millis() - _lastReconnectAttempt > 10000) {
        Serial.println("[Wi-Fi] Falha na conexão. Reiniciando ciclo.");
        WiFi.disconnect();
        _connectionState = EConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis(); 
      }
      break;

    case EConnectionStatus::WIFI_CONNECTED:
      _connectionState = EConnectionStatus::BROKER_CONNECTING;
      _lastReconnectAttempt = 0; // Força a tentativa imediata do MQTT
      break;

    case EConnectionStatus::BROKER_CONNECTING:
      if (WiFi.status() != WL_CONNECTED) {
        _connectionState = EConnectionStatus::DISCONNECTED;
        break;
      }
      
      if (millis() - _lastReconnectAttempt >= _reconnectInterval || _lastReconnectAttempt == 0) {
        _lastReconnectAttempt = millis();
        Serial.printf("[MQTT] Conectando ao Broker %s:%d...\n", _config.mqttBroker, _config.mqttPort);
        
        // Conexão com LWT (Last Will and Testament)
        // Se o ESP32 cair, o broker avisa o dashboard publicando "offline" retido
        bool connected = _mqttClient.connect(
          _config.clientId, 
          _config.mqttUser, 
          _config.mqttPassword, 
          _topicStatus.c_str(), 
          1,    // QoS 
          true, // Retain 
          "offline" // Payload do Testamento
        );

        if (connected) {
          Serial.println("[MQTT] Conectado com sucesso!");
          _connectionState = EConnectionStatus::BROKER_CONNECTED;
          
          // Publica status online retido
          _mqttClient.publish(_topicStatus.c_str(), "online", true);

          // Assina os tópicos de controle vindos do Dashboard
          _mqttClient.subscribe(_topicLedsCmd.c_str());
          _mqttClient.subscribe(_topicRgbCmd.c_str());
          _mqttClient.subscribe(_topicResetCmd.c_str());

          if (_onClientConnectCallback) _onClientConnectCallback();
        } else {
          Serial.print("[MQTT] Falha na conexão. Código de erro (rc): ");
          Serial.println(_mqttClient.state());
        }
      }
      break;

    case EConnectionStatus::BROKER_CONNECTED:
      // Se a internet cair por trás do roteador ou o cabo for puxado
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Rede] Conexão Wi-Fi perdida!");
        _connectionState = EConnectionStatus::DISCONNECTED;
        if (_onClientDisconnectCallback) _onClientDisconnectCallback();
        break;
      }
      
      // Se o broker cair ou desconectar o cliente
      if (!_mqttClient.connected()) {
        Serial.println("[MQTT] Conexão com o broker perdida!");
        _connectionState = EConnectionStatus::,;
        if (_onClientDisconnectCallback) _onClientDisconnectCallback();
        break;
      }
      
      // Mantém a recepção de mensagens ativa
      _mqttClient.loop();
      break;
  }
}

// Wrapper estático que repassa a chamada do C puro para a nossa instância C++
void MQTTController::_mqttCallbackWrapper(char* topic, byte* payload, unsigned int length) {
  if (_instance) {
    String topicStr(topic);
    String payloadStr;
    for (unsigned int i = 0; i < length; i++) {
      payloadStr += (char)payload[i];
    }
    _instance->_processIncomingMessage(topicStr, payloadStr);
  }
}

void MQTTController::_processIncomingMessage(String topic, String payload) {
  Serial.printf("[Comando MQTT] Tópico: %s | Payload: %s\n", topic.c_str(), payload.c_str());

  // Mantemos o estado interno para evitar que um comando zere o estado do outro na sketch.ino
  static bool lastLed1 = false;
  static bool lastLed2 = false;

  if (topic == _topicLedsCmd) {
    // Espera formato "1,0" (Led 1 On, Led 2 Off)
    int commaIndex = payload.indexOf(',');
    if (commaIndex > 0) {
      lastLed1 = (payload.substring(0, commaIndex).toInt() > 0);
      lastLed2 = (payload.substring(commaIndex + 1).toInt() > 0);
      if (_onLedsCommand) _onLedsCommand(lastLed1, lastLed2, false);
    }
  } 
  
  else if (topic == _topicResetCmd) {
    // Comando para resetar Mínimos e Máximos (mantém o estado dos LEDs)
    if (_onLedsCommand) {
      bool triggerReset = (payload.toInt() > 0);
      if (triggerReset) {
        _onLedsCommand(lastLed1, lastLed2, true);
      }
    }
  } 
  
  else if (topic == _topicRgbCmd) {
    // Espera formato "R,G,B" (Ex: 255,0,0)
    int firstComma = payload.indexOf(',');
    int secondComma = payload.indexOf(',', firstComma + 1);
    
    if (firstComma > 0 && secondComma > firstComma) {
      uint8_t r = payload.substring(0, firstComma).toInt();
      uint8_t g = payload.substring(firstComma + 1, secondComma).toInt();
      uint8_t b = payload.substring(secondComma + 1).toInt();
      
      if (_onRgbCommand) _onRgbCommand(r, g, b);
    }
  }
}

// ======== MÉTODOS DE ENVIO (PUBLICAÇÕES) ========

void MQTTController::sendAmbientData(float temperature, float humidity) {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
    _mqttClient.publish(_topicTempC.c_str(), String(temperature, 2).c_str());
    
    float fahrenheit = (temperature * 1.8) + 32.0;
    _mqttClient.publish(_topicTempF.c_str(), String(fahrenheit, 2).c_str());
    
    _mqttClient.publish(_topicHum.c_str(), String(humidity, 2).c_str());
    
    _registerPublish();
  }
}

void MQTTController::sendHistoryData(String jsonPayload) {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
    _mqttClient.publish(_topicHist.c_str(), jsonPayload.c_str());
    _registerPublish();
  }
}

void MQTTController::sendConfigData(bool lockSimpleLeds, bool measure) {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
    // 1 para bloqueado localmente, 0 para controle via web liberado
    _mqttClient.publish(_topicLockState.c_str(), lockSimpleLeds ? "1" : "0");
    _registerPublish();
  }
}

void MQTTController::sendLocalLedsState(bool led1, bool led2, bool resetMinMax) {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
    // Envia o estado atualizado para refletir graficamente no Dashboard
    String stateStr = String(led1 ? 1 : 0) + "," + String(led2 ? 1 : 0);
    _mqttClient.publish(_topicLedsState.c_str(), stateStr.c_str());
    _registerPublish();
  }
}

void MQTTController::sendRssi() {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
    long rssi = WiFi.RSSI();
    _mqttClient.publish(_topicRssi.c_str(), String(rssi).c_str());
    _registerPublish();
  }
}


// ======== CONTROLE DE JANELA DE MENSAGENS (GRAFICO/LCD) ========

void MQTTController::_registerPublish() {
  _publishBuckets[_currentBucketIndex]++;
}

void MQTTController::_updatePublishWindow() {
  // Gira o vetor a cada 1 segundo exato
  if (millis() - _lastBucketShiftTime >= 1000) {
    _lastBucketShiftTime = millis();
    _currentBucketIndex = (_currentBucketIndex + 1) % 60;
    _publishBuckets[_currentBucketIndex] = 0; 
  }
}

// ======== GETTERS E SETTERS ========

bool MQTTController::isWiFiConnected() {
  return _connectionState >= EConnectionStatus::WIFI_CONNECTED;
}

bool MQTTController::isMQTTConnected() {
  return _connectionState == EConnectionStatus::BROKER_CONNECTED;
}

EConnectionStatus MQTTController::getState() {
  return _connectionState;
}

void MQTTController::setLedsCallback(LedsCommandCallback cb) {
  _onLedsCommand = cb;
}

void MQTTController::setRgbCallback(RgbCommandCallback cb) {
  _onRgbCommand = cb;
}

void MQTTController::setClientConnectCallback(OnClientConnectCallback cb) {
  _onClientConnectCallback = cb;
}

void MQTTController::setClientDisconnectCallback(OnClientDisconnectCallback cb) {
  _onClientDisconnectCallback = cb;
}
