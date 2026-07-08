#include <Arduino.h>
#include <LoRaWan_APP.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "HT_SSD1306Wire.h"

#include "config.h"

/*
  OpenTensio LoRa Gateway v0.1.0-draft

  Receives OpenTensio LoRa payloads and publishes them to MQTT.

  v0.1.0 default behavior is drop-in compatibility with the original field
  deployment:
  - LoRa payload: MC;cb1;cb2;t_box;t_soil;v1;v2;r1;r2;vbat;t_air;h_air;p_air
  - MQTT output: same individual Home Assistant state topics as the prototype.

  A future OT1 payload parser is present but not required for v0.1.0.
*/

#ifndef WIFI_SSID
#error "Missing config.h. Copy config.example.h to config.h and configure this gateway."
#endif

SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
static RadioEvents_t RadioEvents;

static unsigned long lastMessageMillis = 0;
static bool oledAwake = false;

struct OpenTensioPacket {
  String protocol;
  String nodeId;
  float cb1;
  float cb2;
  float boxTemperatureC;
  float soilTemperatureC;
  float sensor1Voltage;
  float sensor2Voltage;
  float sensor1ResistanceOhms;
  float sensor2ResistanceOhms;
  float batteryVoltage;
  float airTemperatureC;
  float airHumidityPct;
  float airPressureHPa;
};

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Wi-Fi connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" failed");
  }
}

void connectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("MQTT connecting");
  while (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" OK");
      return;
    }

    Serial.print(".");
    delay(3000);
  }

  Serial.println();
}

bool splitPayload(const String &payload, String fields[], int maxFields, int &fieldCount) {
  fieldCount = 0;
  int start = 0;

  while (start <= payload.length() && fieldCount < maxFields) {
    int sep = payload.indexOf(';', start);
    if (sep == -1) sep = payload.length();

    fields[fieldCount++] = payload.substring(start, sep);
    start = sep + 1;

    if (sep == payload.length()) break;
  }

  return fieldCount > 0;
}

bool parseOpenTensioPayload(const String &payload, OpenTensioPacket &packet) {
  String fields[16];
  int count = 0;
  if (!splitPayload(payload, fields, 16, count)) return false;

  if (fields[0] == "OT1") {
    if (count < 14) return false;

    packet.protocol = fields[0];
    packet.nodeId = fields[1];
    packet.cb1 = fields[2].toFloat();
    packet.cb2 = fields[3].toFloat();
    packet.boxTemperatureC = fields[4].toFloat();
    packet.soilTemperatureC = fields[5].toFloat();
    packet.sensor1Voltage = fields[6].toFloat();
    packet.sensor2Voltage = fields[7].toFloat();
    packet.sensor1ResistanceOhms = fields[8].toFloat();
    packet.sensor2ResistanceOhms = fields[9].toFloat();
    packet.batteryVoltage = fields[10].toFloat();
    packet.airTemperatureC = fields[11].toFloat();
    packet.airHumidityPct = fields[12].toFloat();
    packet.airPressureHPa = fields[13].toFloat();
    return true;
  }

  if (fields[0] == "MC") {
    if (count < 13) return false;

    packet.protocol = fields[0];
    packet.nodeId = "legacy_mc_node";
    packet.cb1 = fields[1].toFloat();
    packet.cb2 = fields[2].toFloat();
    packet.boxTemperatureC = fields[3].toFloat();
    packet.soilTemperatureC = fields[4].toFloat();
    packet.sensor1Voltage = fields[5].toFloat();
    packet.sensor2Voltage = fields[6].toFloat();
    packet.sensor1ResistanceOhms = fields[7].toFloat();
    packet.sensor2ResistanceOhms = fields[8].toFloat();
    packet.batteryVoltage = fields[9].toFloat();
    packet.airTemperatureC = fields[10].toFloat();
    packet.airHumidityPct = fields[11].toFloat();
    packet.airPressureHPa = fields[12].toFloat();
    return true;
  }

  return false;
}

String jsonForPacket(const OpenTensioPacket &packet, int16_t rssi, int8_t snr) {
  String json = "{";
  json += "\"protocol\":\"" + packet.protocol + "\",";
  json += "\"node_id\":\"" + packet.nodeId + "\",";
  json += "\"cb1\":" + String(packet.cb1, 1) + ",";
  json += "\"cb2\":" + String(packet.cb2, 1) + ",";
  json += "\"t_box\":" + String(packet.boxTemperatureC, 1) + ",";
  json += "\"t_soil\":" + String(packet.soilTemperatureC, 1) + ",";
  json += "\"s1_v\":" + String(packet.sensor1Voltage, 3) + ",";
  json += "\"s2_v\":" + String(packet.sensor2Voltage, 3) + ",";
  json += "\"s1_r\":" + String(packet.sensor1ResistanceOhms, 0) + ",";
  json += "\"s2_r\":" + String(packet.sensor2ResistanceOhms, 0) + ",";
  json += "\"v_bat\":" + String(packet.batteryVoltage, 2) + ",";
  json += "\"t_air\":" + String(packet.airTemperatureC, 1) + ",";
  json += "\"h_air\":" + String(packet.airHumidityPct, 0) + ",";
  json += "\"p_air\":" + String(packet.airPressureHPa, 0) + ",";
  json += "\"lora_rssi\":" + String(rssi) + ",";
  json += "\"lora_snr\":" + String(snr);
  json += "}";
  return json;
}

void publishFloat(const String &topic, float value, uint8_t decimals) {
  mqttClient.publish(topic.c_str(), String(value, decimals).c_str(), true);
}

void publishInt(const String &topic, int value) {
  mqttClient.publish(topic.c_str(), String(value).c_str(), true);
}

void publishPacketMQTT(const OpenTensioPacket &packet, int16_t rssi, int8_t snr) {
  if (!mqttClient.connected()) return;

#if OPENTENSIO_PUBLISH_JSON_STATE
  String stateTopic = String(MQTT_BASE_TOPIC) + "/" + packet.nodeId + "/state";
  String json = jsonForPacket(packet, rssi, snr);
  mqttClient.publish(stateTopic.c_str(), json.c_str(), true);
  publishInt(String(MQTT_BASE_TOPIC) + "/" + MQTT_GATEWAY_ID + "/last_rssi", rssi);
  publishInt(String(MQTT_BASE_TOPIC) + "/" + MQTT_GATEWAY_ID + "/last_snr", snr);
#endif

#if OPENTENSIO_PUBLISH_LEGACY_HOMEASSISTANT_TOPICS
  // v0.1.0 default: publish exactly the same Home Assistant MQTT state topics
  // as the original gateway, to avoid breaking existing dashboards/entities.
  String prefix = String(MQTT_LEGACY_PREFIX);
  publishFloat(prefix + "/houblon1/state", packet.cb1, 1);
  publishFloat(prefix + "/houblon2/state", packet.cb2, 1);
  publishFloat(prefix + "/temp_boitier/state", packet.boxTemperatureC, 1);
  publishFloat(prefix + "/temp_sol/state", packet.soilTemperatureC, 1);
  publishFloat(prefix + "/v1/state", packet.sensor1Voltage, 3);
  publishFloat(prefix + "/v2/state", packet.sensor2Voltage, 3);
  publishFloat(prefix + "/r1/state", packet.sensor1ResistanceOhms, 0);
  publishFloat(prefix + "/r2/state", packet.sensor2ResistanceOhms, 0);
  publishFloat(prefix + "/vbat/state", packet.batteryVoltage, 2);
  publishFloat(prefix + "/t_air/state", packet.airTemperatureC, 1);
  publishFloat(prefix + "/h_air/state", packet.airHumidityPct, 0);
  publishFloat(prefix + "/p_air/state", packet.airPressureHPa, 0);
  publishInt(prefix + "/lora_rssi/state", rssi);
  publishInt(prefix + "/lora_snr/state", snr);
#endif
}

void updateDisplay(const OpenTensioPacket &packet, int16_t rssi) {
  lastMessageMillis = millis();
  oledAwake = true;

  display.wakeup();
  display.clear();
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, packet.nodeId);
  display.drawString(0, 11, "AIR: " + String(packet.airTemperatureC, 1) + "C | " + String(packet.airHumidityPct, 0) + "%");
  display.drawLine(0, 23, 128, 23);

  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 25, "S1: " + String(packet.cb1, 1) + " cb");
  display.drawString(0, 41, "S2: " + String(packet.cb2, 1) + " cb");

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 54, "RSSI: " + String(rssi) + " | BAT: " + String(packet.batteryVoltage, 2) + "V");
  display.display();
}

void onReceive(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  char message[size + 1];
  memcpy(message, payload, size);
  message[size] = '\0';

  String raw = String(message);
  OpenTensioPacket packet;

  Serial.println();
  Serial.println(">>> LoRa packet received");
  Serial.printf("Raw : %s\n", raw.c_str());
  Serial.printf("RSSI: %d dBm | SNR: %d\n", rssi, snr);

  if (parseOpenTensioPayload(raw, packet)) {
    Serial.printf("Node: %s | Protocol: %s\n", packet.nodeId.c_str(), packet.protocol.c_str());
    Serial.printf("Soil: S1 %.1f cb | S2 %.1f cb | Tsoil %.1f C\n", packet.cb1, packet.cb2, packet.soilTemperatureC);
    Serial.printf("Air : %.1f C | %.0f %% | %.0f hPa\n", packet.airTemperatureC, packet.airHumidityPct, packet.airPressureHPa);

    publishPacketMQTT(packet, rssi, snr);
    updateDisplay(packet, rssi);
  } else {
    Serial.println("Unsupported or invalid payload. Ignored.");
  }

  Radio.Rx(0);
}

void setup() {
  Serial.begin(115200);

  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);

  Mcu.begin();

  display.init();
#ifdef OPENTENSIO_FLIP_OLED
  display.flipScreenVertically();
#endif
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "OpenTensio Gateway");
  display.drawString(0, 14, "Starting...");
  display.display();

  connectWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);
  connectMQTT();

  RadioEvents.RxDone = onReceive;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODING_RATE,
    0,
    LORA_PREAMBLE_LENGTH,
    0,
    false,
    0,
    true,
    0,
    0,
    false,
    true
  );
  Radio.Rx(0);

  Serial.println("OpenTensio LoRa Gateway ready and listening.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMQTT();

  mqttClient.loop();
  Radio.IrqProcess();

  if (oledAwake && millis() - lastMessageMillis > OLED_AUTO_SLEEP_MS) {
    display.clear();
    display.display();
    display.sleep();
    oledAwake = false;
    Serial.println("OLED sleep.");
  }
}
