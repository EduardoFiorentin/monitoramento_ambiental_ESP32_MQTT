#include "DHT_Async.h"
#include <LiquidCrystal_I2C.h>
#include "PulldownButton.h"
#include "RGBLed.h"
#include "SwitchPullDown.h"
#include "MQTTController.h"
#include "Timer.h"
#include "SimpleLed.h"
#include "env.h"
#include "HistoryBuffer.h"

#define   DHT_SENSOR_PIN    18
#define   DHT_SENSOR_TYPE   DHT_TYPE_22
#define   DHT_MEASURE_TIME  4000ul

#define   WIFI_DATA_TRANSMISSION_INTERVAL                 5000
#define   WIFI_RSSI_TRANSMISSION_INTERVAL                 5000

#define   HISTORY_SEND_INTERVAL                           60000

#define   PIN_BUTTON_1          26
#define   PIN_BUTTON_2          27
#define   PIN_SW_1              34
#define   PIN_SW_2              35
#define   PIN_SW_3              32
#define   PIN_SW_4              33
#define   PIN_LED_1             2
#define   PIN_LED_2             15
#define   PIN_LED_RGB_R         17
#define   PIN_LED_RGB_G         16
#define   PIN_LED_RGB_B         4

#define   MSG_TEMP_C            "Temp (C): "      // lcd screen 1 
#define   MSG_TEMP_F            "Temp (F): "      // lcd screen 1
#define   MSG_HUM               "Hum (%): "       // lcd screen 1
#define   MSG_TEMP_MIN          "Temp min: "      // lcd screen 2
#define   MSG_TEMP_MAX          "Temp max: "      // lcd screen 2
#define   MSG_HUM_MIN           "Hum min: "       // lcd screen 3
#define   MSG_HUM_MAX           "Hum max: "       // lcd screen 3
#define   MSG_NET_STTS          "Rede Status: "   // lcd screen 5
#define   MSG_NET_OFF           "Desconectado"
#define   MSG_NET_WIFI          "Wi-Fi Conectado"
#define   MSG_NET_MQTT          "MQTT Conectado"
#define   MSG_NET_NOT_STARTED   "Erro de Instância"

#define CELSIUS_TO_FAHRENHEIT(c) (((c) * 9.0) / 5.0 + 32.0)

#define DHT_READ_MOCK   // comentar para uso do sensor real nas leituras 
// #define IS_WOKWI     // comentar para habilitar uso do wi-fi real

void update_min_max();
void update_lcd_messages();
void envDataTransmissionCallback();
void rssiTransmissionCallback();
void onHistoryReadyToSendCallback();

// enums =============================================================
// Atualizado para refletir o modo de rede
enum LCDStateEnum {
  SCREEN_1_TEMP_C,      // temp graus C + Hum %
  SCREEN_2_TEMP_F,      // temp graus F + Hum %
  SCREEN_3_TEMP_HIST,   // minimo e máximo de temperatura
  SCREEN_4_HUM_HIST,    // minimo e máximo de umidade
  SCREEN_5_NET          // estado da rede (Wi-Fi / MQTT)
};

// Outputs 
DHT_Async             *dht_sensor;
MQTTController        *mqttController = nullptr; // Ponteiro para a nova classe Wi-Fi
RGBLed                rgbLed(PIN_LED_RGB_R, PIN_LED_RGB_G, PIN_LED_RGB_B);
SimpleLed             led1(PIN_LED_1), led2(PIN_LED_2);
LiquidCrystal_I2C     lcd(0x27, 16, 2);
HistoryBuffer         historyBuffer;

// Inputs
PulldownButton 
  btn1(PIN_BUTTON_1),   // alternância manual das telas do ldc 
  btn2(PIN_BUTTON_2);   // reset minimos e máximos

SwitchPullDown 
  sw1(PIN_SW_1),        // bloqueia/libera o controle dos leds remoto
  sw2(PIN_SW_2),        // liga/desliga led localmente 
  sw3(PIN_SW_3),        // liga/desliga led localmente 
  sw4(PIN_SW_4);        // define a visualização do gráfico no app entre C e F


// State variables ===================================================================
LCDStateEnum lcdState = SCREEN_1_TEMP_C;

Timer 
  envDataTransmitionTimer (WIFI_DATA_TRANSMISSION_INTERVAL, envDataTransmissionCallback), 
  rssiTransmitionTimer    (WIFI_RSSI_TRANSMISSION_INTERVAL, rssiTransmissionCallback),
  historyTimer            (HISTORY_SEND_INTERVAL, onHistoryReadyToSendCallback);


// Value variables ================================================================
float temp = 0.0, minTemp = 40.0, maxTemp = 0.0;
float hum = 0.0, minHum = 40.0, maxHum = 0.0;

// Flag variables ===============================================================
bool isLedsBlockedToApp = false;

// CALLBACKS DE ATUALIZAÇÃO DOS DADOS VINDOS DO MQTT ============================
void processAppLedsCommandCallback(bool appLed1, bool appLed2, bool appReset) {
  if (isLedsBlockedToApp) {
    Serial.println("Comando do Dashboard ignorado: Controle bloqueado fisicamente (SW1).");
    return;
  }

  if (appLed1) led1.setOn(); else led1.setOff();
  if (appLed2) led2.setOn(); else led2.setOff();

  if (appReset) {
    Serial.println("Reset dos valores min/max MQTT.");
    setup_min_max();
    update_lcd_messages();
  }
}

void processAppRgbCommandCallback(uint8_t r, uint8_t g, uint8_t b) {
  if (isLedsBlockedToApp) return;
  rgbLed.setColor(r, g, b);
}

void envDataTransmissionCallback() {
  if (mqttController != nullptr) {
    mqttController->sendAmbientData(temp, hum);
  }
}

void rssiTransmissionCallback() {
  if (mqttController != nullptr) {
    mqttController->sendRssi();
  }
}

// conexão e desconexão do broker
void onDeviceConnectedCallback() {
  if (mqttController != nullptr) {
    Serial.println("[MQTT] Callback de conexão chamada!");
    mqttController->sendConfigData(isLedsBlockedToApp, sw4.isOn());
    mqttController->sendLocalLedsState(led1.isOn(), led2.isOn(), false);
    update_lcd_messages();
  }
}

void onDeviceDisconnectedCallback() {
  Serial.println("[Rede] Callback de desconexão chamada!");
  update_lcd_messages();
}

void onHistoryReadyToSendCallback() {
  Serial.println("[MQTT] Callback mandando historico");
  if (mqttController != nullptr) {
    String payload = historyBuffer.getHistoryJSON();
    Serial.println(payload.c_str());
    mqttController->sendHistoryData(payload);
  }
}


// METODOS DE SETUP ================================================================
void setup_lcd() {
  lcd.init();
  lcd.backlight();
}

void setup_min_max() {
  maxTemp = temp;
  minTemp = temp;
  maxHum = hum;
  minHum = hum;
}

void setup_mqtt() {
  #ifndef IS_WOKWI
  MQTTConfig config = {
    WIFI_SSID,
    WIFI_PASS,
    BROKER_ADDRS,
    MQTT_PORT,
    MQTT_USER_LOGIN,
    MQTT_USER_PASS,
    MQTT_TOPIC_PREFIX,
    MQTT_ESP_CLIENT_ID
  };

  mqttController = new MQTTController(config);
  
  if (mqttController != nullptr) {
    mqttController->setLedsCallback(processAppLedsCommandCallback);
    mqttController->setRgbCallback(processAppRgbCommandCallback);
    mqttController->setClientConnectCallback(onDeviceConnectedCallback);
    mqttController->setClientDisconnectCallback(onDeviceDisconnectedCallback);
    mqttController->begin();
  }
  #else 
  Serial.println("[MQTT] Wokwi detectado - Wi-Fi e MQTT não iniciados")
  #endif 
}

void setup_dht() {
  #ifndef DHT_READ_MOCK
  dht_sensor = new DHT_Async(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
  #endif
}

void setup_io() {
  btn1.begin();
  btn2.begin();
  
  sw1.begin();
  sw2.begin();
  sw3.begin();
  sw4.begin();

  rgbLed.begin();
  led1.begin();
  led2.begin();
}

// Obriga as caracteristicas a assumirem um valor inicial valido
void setup_initial_hardware_state() {
  isLedsBlockedToApp = sw1.isOn();

  // (TODO testar) removido para iniciar refletindo estado das chaves
  // if (isLedsBlockedToApp) {
  // }
  if (sw2.isOn()) led1.setOn(); else led1.setOff();
  if (sw3.isOn()) led2.setOn(); else led2.setOff();

  if (mqttController != nullptr && mqttController->isMQTTConnected()) {
    mqttController->sendConfigData(isLedsBlockedToApp, sw4.isOn());
    mqttController->sendLocalLedsState(led1.isOn(), led2.isOn(), false);
  }
}

// CONTROLES LCD =================================================================
void write_lcd_row_1(String text) {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(text);
}

void write_lcd_row_2(String text) {
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(text);
}

void set_next_lcd_state() {
  switch (lcdState) {
    case SCREEN_1_TEMP_C:     lcdState = SCREEN_2_TEMP_F;     break;
    case SCREEN_2_TEMP_F:     lcdState = SCREEN_3_TEMP_HIST;  break;
    case SCREEN_3_TEMP_HIST:  lcdState = SCREEN_4_HUM_HIST;   break;
    case SCREEN_4_HUM_HIST:   lcdState = SCREEN_5_NET;        break;
    case SCREEN_5_NET:        lcdState = SCREEN_1_TEMP_C;     break;
    default:                  break;
  }
}

void update_lcd_messages() {
  if ( lcdState == SCREEN_1_TEMP_C) {
    String msgTempC= MSG_TEMP_C + String(temp);
    String msgHum= MSG_HUM + String(hum);
    write_lcd_row_1(msgTempC);
    write_lcd_row_2(msgHum);
  }
  else if ( lcdState == SCREEN_2_TEMP_F) {
    String msgTempF = MSG_TEMP_F + String(CELSIUS_TO_FAHRENHEIT(temp));
    String msgHum= MSG_HUM + String(hum);
    write_lcd_row_1(msgTempF);
    write_lcd_row_2(msgHum);
  }
  else if ( lcdState == SCREEN_3_TEMP_HIST) {
    String msgMinTempC= MSG_TEMP_MIN + String(minTemp) + " C";
    String msgMaxTempC= MSG_TEMP_MAX + String(maxTemp) + " %";
    write_lcd_row_1(msgMaxTempC);
    write_lcd_row_2(msgMinTempC);
  }
  else if ( lcdState == SCREEN_4_HUM_HIST) {
    String msgMaxHum= MSG_HUM_MAX + String(maxHum) + " %";
    String msgMinHum= MSG_HUM_MIN + String(minHum) + " %"; 
    write_lcd_row_1(msgMaxHum);
    write_lcd_row_2(msgMinHum);
  }
  else if ( lcdState == SCREEN_5_NET) {
    write_lcd_row_1(MSG_NET_STTS);
    if (mqttController != nullptr) {
      if (mqttController->isMQTTConnected()) {    // broker alcançado
        write_lcd_row_2(MSG_NET_MQTT);
      } 
      else if (mqttController->isWiFiConnected()) { // rede wi-fi alcançada
        write_lcd_row_2(MSG_NET_WIFI);
      } 
      else {
        write_lcd_row_2(MSG_NET_OFF);   // tudo quebrado 
      }
    } 
    else {
      write_lcd_row_2(MSG_NET_NOT_STARTED);
    }
  }
}

// DHT MEASURE CONTROLL ===========================================================
static bool measure_environment(float *temperature, float *humidity) {
  static unsigned long measurement_timestamp = millis();
  if (millis() - measurement_timestamp > DHT_MEASURE_TIME) {
    
    #ifndef DHT_READ_MOCK
    if (dht_sensor->measure(temperature, humidity)) {
      measurement_timestamp = millis();
      update_min_max();
      update_lcd_messages();
      return (true);
    }
    #else
    float noise = (random(0, 10000) / 10000.0 * 2.0) - 1.0;
    *temperature = 25.0 + noise; 
    *humidity = 50.0 - noise;

    measurement_timestamp = millis();
    update_min_max();
    update_lcd_messages();
    return (true);
    #endif
  }
  return (false);
}


// STATES CONTROLL =================================================================
void update_hardware_state() {
  if (btn1.wasPressed()) {
    set_next_lcd_state();
    update_lcd_messages();
  }

  if (btn2.wasPressed()) {
    maxTemp = temp;
    minTemp = temp;
    maxHum = hum;
    minHum = hum;
    update_lcd_messages();
  }

  if (sw1.hasChanged()) {
    if (sw1.isOn()) {
      isLedsBlockedToApp = true;
      Serial.println("Controle local ativado. MQTT bloqueado");

      if (sw2.isOn()) led1.setOn(); else led1.setOff();  
      if (sw3.isOn()) led2.setOn(); else led2.setOff();
      
      if (mqttController != nullptr){ 
        mqttController->sendConfigData(isLedsBlockedToApp, sw4.isOn());
        mqttController->sendLocalLedsState(led1.isOn(), led2.isOn(), false);
      }
    }
    else {
      isLedsBlockedToApp = false;
      Serial.println("Controle local desativado. MQTT liberado");
      if (mqttController != nullptr){ 
        mqttController->sendConfigData(isLedsBlockedToApp, sw4.isOn());
      }
    }
  }

  //  && isLedsBlockedToApp
  if (sw2.hasChanged()) {
    if (sw2.isOn() && !led1.isOn()) led1.setOn();
    if (!sw2.isOn() && led1.isOn()) led1.setOff();

    if (mqttController != nullptr){ 
      mqttController->sendLocalLedsState(led1.isOn(), led2.isOn(), false);
    }
  }

  //  && isLedsBlockedToApp
  if (sw3.hasChanged()) {
    if (sw3.isOn() && !led2.isOn()) led2.setOn();
    if (!sw3.isOn() && led2.isOn()) led2.setOff();
    
    if (mqttController != nullptr){ 
      mqttController->sendLocalLedsState(led1.isOn(), led2.isOn(), false);
    }
  }
  
  if (sw4.hasChanged()) {
    if (mqttController != nullptr){ 
      mqttController->sendConfigData(isLedsBlockedToApp, sw4.isOn());
    }
  }
}

void update_io() {
  sw1.update();
  sw2.update();
  sw3.update();
  sw4.update();
  btn1.update();
  btn2.update();
}

void update_dht_measures() {
  if (measure_environment(&temp, &hum)) {
    historyBuffer.enqueue(temp, hum);     // TODO Ver onde fica melhor esta chamada
  }
  
}

void update_min_max() {
  if (temp < minTemp)   minTemp = temp;
  if (temp > maxTemp)   maxTemp = temp;
  if (hum < minHum)     minHum = hum;
  if (hum > maxHum)     maxHum = hum;
}

void setup() {
  Serial.begin(115200);
  setup_lcd();
  setup_mqtt();
  setup_io();
  setup_dht();
  update_lcd_messages();
  envDataTransmitionTimer.start();
  rssiTransmitionTimer.start();
  historyTimer.start();

  setup_initial_hardware_state();
}

void loop() {
  // estados 
  update_io();
  update_dht_measures();
  update_hardware_state();
  
  // O coração do Wi-Fi e MQTT! Mantém a receção e envio ativos sem bloquear
  if (mqttController != nullptr) {
    mqttController->update();
  }
  
  // timers
  envDataTransmitionTimer.update();
  rssiTransmitionTimer.update();
  historyTimer.update();
}