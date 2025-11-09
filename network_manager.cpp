#include "network_manager.h"
#include "config_manager.h"
#include "mqtt_handlers.h"
#include "web_admin.h"
#include "ui_manager.h"
#include "tab_settings.h"

// Globale Instanz
Tab5NetworkManager networkManager;

// MQTT Topics
const char* Tab5NetworkManager::TP_STAT_CONN = "tab5/stat/connected";
const char* Tab5NetworkManager::TP_TELE_UP = "tab5/tele/uptime";

// ========== Initialisierung ==========
void Tab5NetworkManager::init() {
  Serial.println("🌐 Initialisiere Network Manager...");

  if (!configManager.isConfigured()) {
    Serial.println("⚠️ Keine Netzwerk-Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  // WiFi-Setup
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  wifi_retry_at = 0;  // Sofortiger Verbindungsversuch

  // MQTT-Setup
  mqtt_client.setClient(net_client);
  mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
  mqtt_client.setBufferSize(4096);  // Für große History-CSV-Daten
  mqtt_client.setCallback(mqttCallback);

  Serial.println("✓ Network Manager initialisiert");
}

// ========== WiFi verbinden ==========
void Tab5NetworkManager::connectWifi() {
  wifi_retry_at = millis() + 5000UL;  // Retry in 5s

  if (!configManager.isConfigured()) {
    Serial.println("WiFi: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();
  if (cfg.wifi_ssid && cfg.wifi_ssid[0]) {
    Serial.printf("WiFi: Verbinde mit %s\n", cfg.wifi_ssid);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
  }
}

// ========== MQTT verbinden ==========
void Tab5NetworkManager::connectMqtt() {
  mqtt_retry_at = millis() + 3000UL;  // Retry in 3s

  if (WiFi.status() != WL_CONNECTED) return;

  if (!configManager.isConfigured()) {
    Serial.println("MQTT: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  char client_id[48];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(client_id, sizeof(client_id), "Tab5_LVGL-%04X", (uint16_t)(mac & 0xFFFF));

  Serial.printf("MQTT: Verbinde mit %s:%u als %s\n", cfg.mqtt_host, cfg.mqtt_port, client_id);

  bool ok = false;
  if (cfg.mqtt_user && cfg.mqtt_user[0]) {
    ok = mqtt_client.connect(client_id, cfg.mqtt_user, cfg.mqtt_pass,
                             TP_STAT_CONN, 0, true, "0");
  } else {
    ok = mqtt_client.connect(client_id, nullptr, nullptr,
                             TP_STAT_CONN, 0, true, "0");
  }

  if (!ok) {
    Serial.printf("MQTT: Verbindung fehlgeschlagen, State=%d\n", mqtt_client.state());
    return;
  }

  Serial.println("✓ MQTT verbunden");

  // Connected - Status publizieren und Topics subscriben
  mqtt_client.publish(TP_STAT_CONN, "1", true);
  mqttSubscribeTopics();
  mqttPublishDiscovery();
}

// ========== WiFi-Status ==========
bool Tab5NetworkManager::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

// ========== MQTT-Status ==========
bool Tab5NetworkManager::isMqttConnected() {
  return mqtt_client.connected();
}

PubSubClient& Tab5NetworkManager::getMqttClient() {
  return mqtt_client;
}

// ========== Telemetrie senden ==========
void Tab5NetworkManager::publishTelemetry() {
  if (!mqtt_client.connected()) return;

  uint32_t now = millis();
  if (now - last_telemetry > 30000UL) {  // 30 Sekunden
    last_telemetry = now;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(now / 1000UL));
    mqtt_client.publish(TP_TELE_UP, buf, true);
  }
}

// ========== Update-Schleife ==========
void Tab5NetworkManager::update() {
  if (!configManager.isConfigured()) {
    return;
  }

  uint32_t now_ms = millis();
  bool is_connected = (WiFi.status() == WL_CONNECTED);

  // WiFi-Verbindung verwalten
  if (!is_connected) {
    // Nicht verbunden - Retry
    if ((int32_t)(now_ms - wifi_retry_at) >= 0) {
      connectWifi();
    }

    // WebAdmin stoppen wenn Verbindung verloren
    if (was_connected && webAdminServer.isRunning()) {
      webAdminServer.stop();
    }
  } else {
    // Verbunden

    // WebAdmin starten wenn gerade verbunden
    if (!was_connected && !webAdminServer.isRunning()) {
      webAdminServer.start();
    }

    // NTP-Sync triggern bei neuer Verbindung
    if (!was_connected) {
      uiManager.scheduleNtpSync(0);
    }

    // MQTT verwalten
    if (!mqtt_client.connected()) {
      if ((int32_t)(now_ms - mqtt_retry_at) >= 0) {
        connectMqtt();
      }
    } else {
      mqtt_client.loop();
      publishTelemetry();
    }
  }

  // WiFi-Status für nächste Runde merken
  was_connected = is_connected;
}
