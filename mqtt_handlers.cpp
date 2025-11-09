#include "mqtt_handlers.h"
#include "network_manager.h"
#include "tab_home.h"
#include "tab_solar.h"
#include <PubSubClient.h>

// ========== MQTT Topics ==========
namespace MqttTopics {
  const char* SENSOR_OUT = "tab5/sensor/outside_c";
  const char* SENSOR_IN = "tab5/sensor/inside_c";
  const char* SENSOR_SOC = "tab5/sensor/soc_pct";
  const char* SCENE_CMND = "tab5/cmnd/scene";
  const char* STAT_CONN = "tab5/stat/connected";
  const char* TELE_UP = "tab5/tele/uptime";
  const char* HA_WOHN_TEMP = "ha/statestream/sensor/og_wohnbereich_sensor_temperatur/state";
  const char* HA_PV_GARAGE = "ha/statestream/sensor/pv_haus_garage/state";
  const char* HA_PV_GARAGE_HISTORY = "ha/statestream/sensor/pv_haus_garage/history";
}

// Cached values
static float g_outside_c = 21.7f;
static float g_inside_c = 22.4f;
static int g_soc_pct = 73;

// ========== MQTT Callback (Topic-Routing) ==========
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  // Check if this is the PV history topic (needs large buffer)
  if (strcmp(topic, MqttTopics::HA_PV_GARAGE_HISTORY) == 0) {
    // Allocate buffer for CSV history data
    static char history_buf[4000];
    length = (length < sizeof(history_buf)-1) ? length : (sizeof(history_buf)-1);
    memcpy(history_buf, payload, length);
    history_buf[length] = '\0';

    Serial.println("═══════════════════════════════════════");
    Serial.printf("📥 HA PV Garage History: %d bytes empfangen\n", length);
    Serial.printf("📝 Erste 200 Zeichen: %.200s\n", history_buf);
    Serial.println("═══════════════════════════════════════");

    solar_set_history_csv(history_buf);
    return;
  }

  // Make a zero-terminated copy for regular messages
  char msg[64];
  length = (length < sizeof(msg)-1) ? length : (sizeof(msg)-1);
  memcpy(msg, payload, length);
  msg[length] = '\0';

  // Route by topic
  if (strcmp(topic, MqttTopics::SENSOR_OUT) == 0) {
    g_outside_c = atof(msg);
    home_set_values(g_outside_c, g_inside_c, g_soc_pct);
  }
  else if (strcmp(topic, MqttTopics::SENSOR_IN) == 0) {
    g_inside_c = atof(msg);
    home_set_values(g_outside_c, g_inside_c, g_soc_pct);
  }
  else if (strcmp(topic, MqttTopics::SENSOR_SOC) == 0) {
    g_soc_pct = atoi(msg);
    home_set_values(g_outside_c, g_inside_c, g_soc_pct);
  }
  else if (strcmp(topic, MqttTopics::SCENE_CMND) == 0) {
    Serial.printf("Scene command: %s\n", msg);
    // TODO: Optionally trigger UI feedback or actions
  }
  else if (strcmp(topic, MqttTopics::HA_WOHN_TEMP) == 0) {
    float v = atof(msg);
    Serial.printf("HA Wohnbereich Temperatur: raw='%s' parsed=%.2f C\n", msg, v);
    home_set_wohn_temp(v);
  }
  else if (strcmp(topic, MqttTopics::HA_PV_GARAGE) == 0) {
    float v = atof(msg);
    Serial.printf("HA PV Garage: raw='%s' parsed=%.0f W\n", msg, v);
    home_set_pv_garage(v);
    solar_update_power(v);
  }
}

// ========== Subscribe zu Topics ==========
void mqttSubscribeTopics() {
  PubSubClient& mqtt = networkManager.getMqttClient();

  mqtt.subscribe(MqttTopics::SENSOR_OUT);
  mqtt.subscribe(MqttTopics::SENSOR_IN);
  mqtt.subscribe(MqttTopics::SENSOR_SOC);
  mqtt.subscribe(MqttTopics::SCENE_CMND);
  mqtt.subscribe(MqttTopics::HA_WOHN_TEMP);
  mqtt.subscribe(MqttTopics::HA_PV_GARAGE);
  mqtt.subscribe(MqttTopics::HA_PV_GARAGE_HISTORY);

  Serial.println("✓ MQTT Topics subscribed");
}

// ========== Home Snapshot publizieren ==========
void mqttPublishHomeSnapshot() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  char buf[24];
  dtostrf(g_outside_c, 0, 1, buf);
  mqtt.publish(MqttTopics::SENSOR_OUT, buf, true);

  dtostrf(g_inside_c, 0, 1, buf);
  mqtt.publish(MqttTopics::SENSOR_IN, buf, true);

  snprintf(buf, sizeof(buf), "%d", g_soc_pct);
  mqtt.publish(MqttTopics::SENSOR_SOC, buf, true);
}

// ========== Scene Command publizieren ==========
void mqttPublishScene(const char* scene_name) {
  if (!scene_name || !*scene_name) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("Scene command übersprungen (MQTT offline): %s\n", scene_name);
    return;
  }

  bool ok = mqtt.publish(MqttTopics::SCENE_CMND, scene_name, false);
  Serial.printf("Scene command -> MQTT '%s' (%s)\n", scene_name, ok ? "ok" : "fail");
}

// ========== Home Assistant MQTT Discovery ==========
void mqttPublishDiscovery() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  Serial.println("📡 Publiziere Home Assistant Discovery...");

  // Build device ID based on MAC
  char did[24];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(did, sizeof(did), "tab5_lvgl_%04X", (uint16_t)(mac & 0xFFFF));

  char tpc[96];
  char js[256];

  // Outside temperature
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_outside_c/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Outside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_out\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    MqttTopics::SENSOR_OUT, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  // Inside temperature
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_inside_c/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Inside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_in\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    MqttTopics::SENSOR_IN, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  // Battery SoC
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_soc_pct/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Battery SoC\",\"stat_t\":\"%s\",\"unit_of_meas\":\"%%\",\"dev_cla\":\"battery\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_soc\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    MqttTopics::SENSOR_SOC, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  // Uptime
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_uptime/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Uptime\",\"stat_t\":\"%s\",\"unit_of_meas\":\"s\",\"uniq_id\":\"%s_up\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    MqttTopics::TELE_UP, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  // Scene Buttons
  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_abend/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Abend\",\"cmd_t\":\"%s\",\"pl_prs\":\"Abend\",\"uniq_id\":\"%s_btn_abend\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    MqttTopics::SCENE_CMND, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_lesen/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Lesen\",\"cmd_t\":\"%s\",\"pl_prs\":\"Lesen\",\"uniq_id\":\"%s_btn_lesen\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    MqttTopics::SCENE_CMND, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_allesaus/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Alles Aus\",\"cmd_t\":\"%s\",\"pl_prs\":\"AllesAus\",\"uniq_id\":\"%s_btn_allesaus\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    MqttTopics::SCENE_CMND, did, MqttTopics::STAT_CONN, did);
  mqtt.publish(tpc, js, true);

  Serial.println("✓ Home Assistant Discovery veröffentlicht");
}
