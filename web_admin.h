#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "config_manager.h"

// Webinterface für MQTT-Konfiguration im normalen Netzwerk
// Läuft wenn das Gerät bereits mit WiFi verbunden ist

class WebAdminServer {
public:
  WebAdminServer();

  // Startet Webserver im normalen WiFi
  bool start();

  // Stoppt Webserver
  void stop();

  // Muss regelmäßig in loop() aufgerufen werden
  void handle();

  // Prüft ob Server läuft
  bool isRunning() const { return running; }

private:
  WebServer server;
  bool running;

  // Request Handler
  void handleRoot();
  void handleSaveMQTT();
  void handleStatus();
  void handleRestart();

  // HTML-Seiten
  String getAdminPage();
  String getSuccessPage();
  String getStatusJSON();
};

// Globale Instanz
extern WebAdminServer webAdminServer;
