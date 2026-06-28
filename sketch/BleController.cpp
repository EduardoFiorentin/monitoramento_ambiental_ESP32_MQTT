#include "BleController.h"

class CustomServerCallbacks : public NimBLEServerCallbacks {
private:
  bool            *pDeviceConnected;
  BleController   *parentController;

public:
  CustomServerCallbacks(bool* connectedFlag, BleController *controller) {
    this->pDeviceConnected = connectedFlag; 
    this->parentController = controller;
  }

  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    *pDeviceConnected = true;
    Serial.println(">>> Cliente BLE conectado");
    Serial.print(" Endereço: ");
    Serial.println(connInfo.getAddress().toString().c_str());

    // otimização dos parametros de conexão
    pServer->updateConnParams(
      connInfo.getConnHandle(), 
      BLE_MIN_CONNECT_INTERVAL, 
      BLE_MAX_CONNECT_INTERVAL, 
      BLE_SLAVE_LATENCI, 
      BLE_SUPERVISION_TIMEOUT
    );
    Serial.println(" -> Parmetros de conexão aplicados.");

    if (this->parentController != nullptr) {
      if (this->parentController->onClientConnectCallback != nullptr)
      this->parentController->onClientConnectCallback();
    }

  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    *pDeviceConnected = false;
    Serial.println(">>> Cliente BLE desconectado");
    pServer->startAdvertising();
    if (this->parentController != nullptr) {
      if (this->parentController->onClientDisconnectCallback != nullptr)
      this->parentController->onClientDisconnectCallback();
    }
  }
};

class CustomCharCallbacks : public NimBLECharacteristicCallbacks {
private:
  BleController* parentController;

public:
  CustomCharCallbacks(BleController* controller): parentController(controller) {}

  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    Serial.printf(" [BLE] O apk leu a característica: %s\n", pCharacteristic->getUUID().toString().c_str());
  }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string rxValue = pCharacteristic->getValue();
    
    if (rxValue.length() > 0) {
      
      //qual característica foi escrita
      NimBLEUUID writtenUUID = pCharacteristic->getUUID();

      if (writtenUUID.equals(NimBLEUUID(BLE_CHAR_UID_SIMPLE_LEDS))) {
        
        uint8_t payload = rxValue[0]; 
        
        // decodifica com bitwise
        // 1 - true / 0 - false
        bool led1Ativo        = (payload & 0x01); // Verifica o Bit 0
        bool led2Ativo        = (payload & 0x02); // Verifica o Bit 1
        bool resetMinMaxAtivo = (payload & 0x04); // Verifica o Bit 2

        Serial.println("[COMANDO BLE] -> Recebido Status dos LEDs e Reset:");
        
        Serial.print("  -> LED 1: ");
        Serial.println(led1Ativo ? "LIGADO" : "DESLIGADO");
        
        Serial.print("  -> LED 2: ");
        Serial.println(led2Ativo ? "LIGADO" : "DESLIGADO");
        
        Serial.print("  -> Reset Min/Max: ");
        Serial.println(resetMinMaxAtivo ? "ACIONADO" : "INATIVO");

        // Se uma função foi registrada no arquivo principal, nós a chamamos agora!
        if (parentController->onLedsCommand != nullptr) {
          parentController->onLedsCommand(led1Ativo, led2Ativo, resetMinMaxAtivo);
        }
      }
      
      // comandos para o led rgb
      else if (writtenUUID.equals(NimBLEUUID(BLE_CHAR_UID_RGB_LED))) {
        // Para um RGB, esperamos receber 3 bytes (Vermelho, Verde, Azul), cada um de 0 a 255.
        if (rxValue.length() >= 3) {
          uint8_t r = rxValue[0];
          uint8_t g = rxValue[1];
          uint8_t b = rxValue[2];
          
          Serial.printf("[COMANDO BLE] -> Cor recebida | R: %d | G: %d | B: %d\n", r, g, b);

          if (parentController->onRgbCommand != nullptr) {
            parentController->onRgbCommand(r, g, b);
          }
        } else {
          Serial.println("[ERRO BLE] Pacote RGB incompleto. Esperados 3 bytes.");
        }
      }
    }
  }

  void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
  }
};

BleController::BleController() {
  // Construtor vazio evita problemas de boot no escopo global
}

void BleController::begin() {
  NimBLEDevice::init(BLE_NAME_ADVERTISING);

  // expandindo o tamanho do pacote para transferências maiores (como o histórico)
  NimBLEDevice::setMTU(512);

  // segurança e pareamento por Senha (PIN)
  NimBLEDevice::setSecurityAuth(true, true, true); // Bonding + MITM + Secure Connections
  NimBLEDevice::setSecurityPasskey(BLE_PASSWORD);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // Diz ao celular que o ESP exibe a senha fixa

  this->customBLEServerCallback = new CustomServerCallbacks(&this->deviceConnected, this);
  this->customBLECharCallback = new CustomCharCallbacks(this);
  this->server = NimBLEDevice::createServer();
  this->server->setCallbacks(this->customBLEServerCallback);

  // cria os serviços
  this->envMonitoringService = this->server->createService(BLE_SERVICE_UID_ENV_MONITORING);
  this->actuatorControllService = this->server->createService(BLE_SERVICE_UID_ACTUATOR_CONTROLL);
  this->connectIndicatorService = this->server->createService(BLE_SERVICE_UID_CONNECT_INDICATOR);

  // cria as características
  this->dataCharacteristic = this->envMonitoringService->createCharacteristic(
    BLE_CHAR_UID_ACTUAL_DATA, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  this->dataHystCharacteristic = this->envMonitoringService->createCharacteristic(
    BLE_CHAR_UID_HYST_GRAPHIC, NIMBLE_PROPERTY::READ
  );

  // ledsChar 
  //  write   - traz o estado dos leds do apk
  //  notify  - envia o estado do bloqueio dos leds vindo do switch 1
  this->ledsCharacteristic = this->actuatorControllService->createCharacteristic(
    BLE_CHAR_UID_SIMPLE_LEDS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY // adicionado notify em relação à GATT original
  );
  this->rgbLedCharacteristic = this->actuatorControllService->createCharacteristic(
    BLE_CHAR_UID_RGB_LED, NIMBLE_PROPERTY::WRITE_NR
  );
  this->deviceConfigCharacteristic = this->actuatorControllService->createCharacteristic( // char nova para bloqueios e C/F
    BLE_CHAR_UID_DEVICE_CONFIG, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  
  this->rssiCharacteristic = this->connectIndicatorService->createCharacteristic(
    BLE_CHAR_UID_RSSI, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );
  this->notifyCountCharacteristic = this->connectIndicatorService->createCharacteristic(
    BLE_CHAR_UID_NOTIFY_COUNT, NIMBLE_PROPERTY::READ
  );

  // Associa o callback às características que podem receber comandos
  this->ledsCharacteristic->setCallbacks(this->customBLECharCallback);
  this->rgbLedCharacteristic->setCallbacks(this->customBLECharCallback);

  // serviços para que fiquem ativos internamente
  this->envMonitoringService->start();
  this->actuatorControllService->start();
  this->connectIndicatorService->start();

  // configura e inicia o Advertiser (Anúncio)
  this->pAdvertising = NimBLEDevice::getAdvertising();
  
  // Dados do pacote de advertising principal (descoberta)
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.addServiceUUID(BLE_SERVICE_UID_ENV_MONITORING);
  
  // Dados da Resposta de Scan
  NimBLEAdvertisementData scanRespData;
  scanRespData.setName(BLE_NAME_ADVERTISING); 
  scanRespData.addServiceUUID(BLE_SERVICE_UID_ACTUATOR_CONTROLL); // UUID longo vai aqui

  // cinfig ao hardware de transmissão
  this->pAdvertising->setAdvertisementData(advData);
  this->pAdvertising->setScanResponseData(scanRespData);
  
  // frequência com que o ESP envia os pacotes de anúncio
  this->pAdvertising->setMinInterval(BLE_ADVERTISING_INTERVAL);
  this->pAdvertising->setMaxInterval(BLE_ADVERTISING_INTERVAL);

  // Inicia a transmissão via rádio
  this->pAdvertising->start();

  Serial.println(">>> BLE Inicializado com sucesso!");
  Serial.printf("Dispositivo: %s | Senha : %06d\n", BLE_NAME_ADVERTISING, BLE_PASSWORD);
}

bool BleController::hasDeviceConnected() {
  return this->deviceConnected;
}

bool BleController::isAdvertising() {
  if (this->pAdvertising != nullptr) {
    return this->pAdvertising->isAdvertising();
  }
  return false;
}

// O __attribute__((packed)) garante que o compilador não adicione bytes vazios na memória
struct __attribute__((packed)) EnvDataPayload {
  int16_t tempCelsius;     // 2 bytes
  int16_t tempFahrenheit;  // 2 bytes 
  uint16_t humidity;       // 2 bytes 
};

void BleController::sendAmbientData(float temperature, float humidity) {
  if (this->hasDeviceConnected() && this->dataCharacteristic != NULL) {
    EnvDataPayload payload;
    
    // ex: 25.43 ºC -> 2543
    // int16_t -32768 a 32767 ->  -327.68 a 327.67 C/F
    payload.tempCelsius = (int16_t)(temperature * 100);
    
    // Fahrenheit
    payload.tempFahrenheit = (int16_t)((temperature * 1.8 + 32.0) * 100);
    
    // Empacota a humidade
    payload.humidity = (uint16_t)(humidity * 100);

    // Envia o bloco de bytes direto da memória. sizeof(payload) -> 6 bytes.
    this->dataCharacteristic->setValue( (uint8_t*) &payload, sizeof(payload));
    this->dataCharacteristic->notify();
    this->registerNotification();
    
    Serial.println("[BLE] Dados enviados (6 bytes).");
  }
}

/**
 * @brief Envia dados de configuração de bloqueios de led e unidade de medida exibidas no aplicativo
 * @details Envia 1B de dados (0x000000ba), onde 'a' = lockSimpleLeds e 'b' = measure
 * @param lockSimpleLeds Indica ao aplicativo se o controle dos leds está bloqueado para eles ('0' = controle do app / '1' = controle local)
 * @param measure Indica ao aplicativo qual a unidade de medida dos gráficos (0 = C / 1 = F) 
 * @return void 
 */
void BleController::sendConfigData(bool lockSimpleLeds, bool measure) {
  if (!this->hasDeviceConnected()) return;
  
  uint8_t payload = 0x00;

  payload |= (measure & 0x01);            
  payload |= ((lockSimpleLeds & 0x01) << 1);

  this->deviceConfigCharacteristic->setValue((uint8_t*)&payload, 1);
  this->deviceConfigCharacteristic->notify();
  this->registerNotification();
}


/**
 * @brief Notifica o aplicativo sobre uma mudança local nos LEDs (feita por botão físico)
 */
void BleController::sendLocalLedsState(bool led1, bool led2, bool resetMinMax) {
  if (!this->hasDeviceConnected() || this->ledsCharacteristic == NULL) return;
  
  uint8_t payload = 0x00;
  if (led1) payload |= 0x01;
  if (led2) payload |= 0x02;
  if (resetMinMax) payload |= 0x04;

  this->ledsCharacteristic->setValue((uint8_t*)&payload, 1);
  this->ledsCharacteristic->notify();
  this->registerNotification();

  Serial.println("[BLE] estado local dos LEDs sincronizado com o App");
}

/**
 * @brief Lê o RSSI do hardware e notifica o aplicativo.
 * @details Esta função não possui timer interno. O controle de chamadas deve ser feito externamente.
 */
void BleController::notifyRssi() {
  if (!this->hasDeviceConnected()) return;

  std::vector<uint16_t> peerIds = this->server->getPeerDevices();
  if (peerIds.size() > 0) {
    uint16_t atualConnHandle = peerIds[0];
    int8_t rssiValue = 0;
    
    // Lê do hardware e notifica
    if (ble_gap_conn_rssi(atualConnHandle, &rssiValue) == 0) {
      this->rssiCharacteristic->setValue((uint8_t*)&rssiValue, 1);
      this->rssiCharacteristic->notify();
      this->registerNotification(); 
    }
  }
}

/**
 * @brief Gira a janela deslizante de 60 segundos e atualiza o valor da característica.
 * @details ATENÇÃO: Esta função DEVE ser chamada externamente a cada 1000ms exatos para garantir a precisão.
 */
void BleController::updateNotificationWindow() {
  if (!this->hasDeviceConnected()) return;

  this->currentBucketIndex = (this->currentBucketIndex + 1) % 60;
  this->notifyBuckets[this->currentBucketIndex] = 0;

  // Soma a janela inteira para obter o total atual
  uint16_t slidingTotal = 0;
  for (int i = 0; i < 60; i++) {
    slidingTotal += this->notifyBuckets[i];
  }
  if (this->notifyCountCharacteristic != NULL) {
    this->notifyCountCharacteristic->setValue((uint8_t*)&slidingTotal, 2);
  }
}

/**
 * @brief Apenas registra +1 no momento exato em que a função é chamada
 */
void BleController::registerNotification() {
  this->notifyBuckets[this->currentBucketIndex]++;
}

void BleController::setLedsCallback(LedsCommandCallback cb) {
  this->onLedsCommand = cb;
}

void BleController::setRgbCallback(RgbCommandCallback cb) {
  this->onRgbCommand = cb;
}

void BleController::setClientConnectCallback(OnClientConnectCallback cb) {
  this->onClientConnectCallback = cb;
}

void BleController::setClientDisconnectCallback(OnClientDisconnectCallback cb) {
  this->onClientDisconnectCallback = cb;
}

