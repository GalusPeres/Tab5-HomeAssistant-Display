#ifndef MQTT_HANDLERS_H
#define MQTT_HANDLERS_H

#include <Arduino.h>

// MQTT Topic Definitionen
namespace MqttTopics {
  extern const char* SENSOR_OUT;
  extern const char* SENSOR_IN;
  extern const char* SENSOR_SOC;
  extern const char* SCENE_CMND;
  extern const char* STAT_CONN;
  extern const char* TELE_UP;
  extern const char* HA_WOHN_TEMP;
  extern const char* HA_PV_GARAGE;
  extern const char* HA_PV_GARAGE_HISTORY;
}

// MQTT Callback-Funktionen
void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
void mqttSubscribeTopics();
void mqttPublishDiscovery();
void mqttPublishScene(const char* scene_name);
void mqttPublishHomeSnapshot();

#endif // MQTT_HANDLERS_H
