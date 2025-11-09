#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Tab5 Network Manager - Verwaltet WiFi und MQTT
class Tab5NetworkManager {
public:
  // Initialisierung
  void init();

  // Update-Schleife
  void update();

  // WiFi-Status
  bool isWifiConnected() const;
  bool wasPreviouslyConnected() const { return was_connected; }

  // MQTT-Status
  bool isMqttConnected();
  PubSubClient& getMqttClient();

  // Verbindung herstellen
  void connectWifi();
  void connectMqtt();

  // Telemetrie
  void publishTelemetry();

private:
  WiFiClient net_client;
  PubSubClient mqtt_client;

  uint32_t wifi_retry_at = 0;
  uint32_t mqtt_retry_at = 0;
  uint32_t last_telemetry = 0;
  bool was_connected = false;

  // MQTT Topics (aus Config oder Default)
  static const char* TP_STAT_CONN;
  static const char* TP_TELE_UP;
};

// Globale Instanz
extern Tab5NetworkManager networkManager;

#endif // NETWORK_MANAGER_H
