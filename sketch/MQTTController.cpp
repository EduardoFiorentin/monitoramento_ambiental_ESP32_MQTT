#include "MQTTController.h"

MQTTController* MQTTController::_instance = nullptr;

MQTTController::MQTTController(MQTTConfig config) : _config(config) {
  _instance = this;
}

void MQTTController::begin() {
  _buildTopics();
  
  WiFi.mode(WIFI_STA);
  
  _mqttClient.setBufferSize(MAX_PUB_BUFFER_SIZE);    

  _wifiClient.setTimeout(10);
  _wifiClientSecure.setTimeout(10);
  _wifiClientSecure.setHandshakeTimeout(10); // TLS

  if (_config.useTLS) {
    if (_config.rootCA != nullptr) {
      _wifiClientSecure.setCACert(_config.rootCA);
    } else {
      _wifiClientSecure.setInsecure();
    }
    _mqttClient.setClient(_wifiClientSecure);
  } else {
    _mqttClient.setClient(_wifiClient);
  }

  _mqttClient.setServer(_config.mqttBroker, _config.mqttPort);
  _mqttClient.setCallback(MQTTController::_mqttCallbackWrapper);
  
  _connectionState = EConnectionStatus::DISCONNECTED;
  _lastReconnectAttempt = 0;
  
  Serial.println("[MQTTController] Inicializado e aguardando processamento da rede.");
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
  _topicDashStatus  = prefix + TOPIC_SUFFIX_DASH_STATUS;
}

void MQTTController::update() {
  _updatePublishWindow();
  switch (_connectionState) {
    
    case EConnectionStatus::DISCONNECTED:
      if (millis() - _lastReconnectAttempt >= _reconnectInterval || _lastReconnectAttempt == 0) {
        _lastReconnectAttempt = millis();
        
        Serial.printf("\n[Wi-Fi] Tentando conectar ao SSID: %s...\n", _config.wifiSSID);
        
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        
        if (_config.isEduroam) {
          Serial.println("[Wi-Fi] Modo Ativo: WPA2-Enterprise (Universidade)");

          esp_eap_client_set_identity((uint8_t *)_config.wifiUser, strlen(_config.wifiUser));
          esp_eap_client_set_username((uint8_t *)_config.wifiUser, strlen(_config.wifiUser));
          esp_eap_client_set_password((uint8_t *)_config.wifiPassword, strlen(_config.wifiPassword));
          esp_wifi_sta_enterprise_enable();
          
          WiFi.begin(_config.wifiSSID); 
        } else {
          Serial.println("[Wi-Fi] Modo Ativo: WPA2-Personal (Doméstico)");
          WiFi.begin(_config.wifiSSID, _config.wifiPassword);
        }
        
        _connectionState = EConnectionStatus::WIFI_CONNECTING;
      }
      break;

    case EConnectionStatus::WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[Wi-Fi] Conectado! IP: ");
        Serial.println(WiFi.localIP());
        _connectionState = EConnectionStatus::WIFI_CONNECTED;
      } 
      else if (millis() - _lastReconnectAttempt > 10000) {
        Serial.println("[Wi-Fi] Falha na conexão. Reiniciando ciclo.");
        WiFi.disconnect();
        _connectionState = EConnectionStatus::DISCONNECTED;
        _lastReconnectAttempt = millis(); 
      }
      break;

    case EConnectionStatus::WIFI_CONNECTED:
      _connectionState = EConnectionStatus::BROKER_CONNECTING;
      _lastReconnectAttempt = 0; 
      break;

    case EConnectionStatus::BROKER_CONNECTING:
      // intervalo antes de tentar novamente 
      if (millis() - _lastReconnectAttempt >= _reconnectInterval) {
        
        Serial.printf("[MQTT] Conectando ao Broker %s:%d...\n", _config.mqttBroker, _config.mqttPort);
        
        // zona bloqueante =========================================
        bool success;
        if (_config.mqttUser != nullptr && strlen(_config.mqttUser) > 0) {
          success = _mqttClient.connect(_config.clientId, _config.mqttUser, _config.mqttPassword, 
            _topicStatus.c_str(), 1, true, "offline");
        } else {
          success = _mqttClient.connect(_config.clientId, 
            _topicStatus.c_str(), 1, true, "offline");
        }
        // ===================================================== fim zona blq

        if (success) {
          Serial.println("[MQTT] Conectado com sucesso!");
          
          // Assinatura de topicos de controle
          _mqttClient.publish(_topicStatus.c_str(), "online", true);
          _mqttClient.subscribe(_topicLedsCmd.c_str());
          _mqttClient.subscribe(_topicRgbCmd.c_str());
          _mqttClient.subscribe(_topicResetCmd.c_str());
          _mqttClient.subscribe(_topicDashStatus.c_str());
          
          _connectionState = EConnectionStatus::BROKER_CONNECTED;

        } else {
          Serial.printf("[MQTT] Falha na conexão. Código de erro (rc): %d\n", _mqttClient.state());
        }
        _lastReconnectAttempt = millis(); 
      }
      break;

    case EConnectionStatus::BROKER_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Rede] Conexão Wi-Fi perdida!");
        _connectionState = EConnectionStatus::DISCONNECTED;
        if (_onClientDisconnectCallback) _onClientDisconnectCallback();
        break;
      }
      
      if (!_mqttClient.connected()) {
        Serial.println("[MQTT] Conexão com o broker perdida!");
        _connectionState = EConnectionStatus::BROKER_CONNECTING;
        if (_onClientDisconnectCallback) _onClientDisconnectCallback();
        break;
      }
      
      _mqttClient.loop();
      break;
  }
}

void MQTTController::_mqttCallbackWrapper(char* topic, byte* payload, unsigned int length) {
  if (_instance) {
    String topicStr(topic);
    String payloadStr((char*)payload, length);
    _instance->_processIncomingMessage(topicStr, payloadStr);
  }
}

void MQTTController::_processIncomingMessage(String topic, String payload) {
  Serial.printf("[Comando MQTT] Tópico: %s | Payload: %s\n", topic.c_str(), payload.c_str());

  static bool lastLed1 = false;
  static bool lastLed2 = false;

  if (topic == _topicLedsCmd) {
    int commaIndex = payload.indexOf(',');
    if (commaIndex > 0) {
      lastLed1 = (payload.substring(0, commaIndex).toInt() > 0);
      lastLed2 = (payload.substring(commaIndex + 1).toInt() > 0);
      if (_onLedsCommand) _onLedsCommand(lastLed1, lastLed2, false);
    }
  } 
  
  else if (topic == _topicResetCmd) {
    if (_onLedsCommand) {
      bool triggerReset = (payload.toInt() > 0);
      if (triggerReset) {
        _onLedsCommand(lastLed1, lastLed2, true);
      }
    }
  } 
  
  else if (topic == _topicRgbCmd) {
    int firstComma = payload.indexOf(',');
    int secondComma = payload.indexOf(',', firstComma + 1);
    
    if (firstComma > 0 && secondComma > firstComma) {
      uint8_t r = payload.substring(0, firstComma).toInt();
      uint8_t g = payload.substring(firstComma + 1, secondComma).toInt();
      uint8_t b = payload.substring(secondComma + 1).toInt();
      
      if (_onRgbCommand) _onRgbCommand(r, g, b);
    }
  }
  else if (topic == _topicDashStatus) {
    if (payload == "online") {
      Serial.println("[Sistema] Dashboard Web Conectado!");
      if (_onClientConnectCallback) _onClientConnectCallback();
    } 
    else if (payload == "offline") {
      Serial.println("[Sistema] Dashboard Web Desconectado!");
      if (_onClientDisconnectCallback) _onClientDisconnectCallback();
    }
  }
}

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
    _mqttClient.publish(_topicLockState.c_str(), lockSimpleLeds ? "1" : "0");
    _registerPublish();
  }
}

void MQTTController::sendLocalLedsState(bool led1, bool led2, bool resetMinMax) {
  if (_connectionState == EConnectionStatus::BROKER_CONNECTED) {
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

void MQTTController::_registerPublish() {
  _publishBuckets[_currentBucketIndex]++;
}

void MQTTController::_updatePublishWindow() {
  if (millis() - _lastBucketShiftTime >= 1000) {
    _lastBucketShiftTime = millis();
    _currentBucketIndex = (_currentBucketIndex + 1) % 60;
    _publishBuckets[_currentBucketIndex] = 0; 
  }
}

bool MQTTController::isWiFiConnected() {
  return _connectionState >= EConnectionStatus::WIFI_CONNECTED;
}

bool MQTTController::isMQTTConnected() {
  return _connectionState == EConnectionStatus::BROKER_CONNECTED;
}

EConnectionStatus MQTTController::getState() {
  return _connectionState;
}

// setters de callback ================================================

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