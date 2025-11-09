#include "web_admin.h"
#include <WiFi.h>

// Globale Instanz
WebAdminServer webAdminServer;

WebAdminServer::WebAdminServer() : server(80), running(false) {
}

bool WebAdminServer::start() {
  if (running) {
    Serial.println("⚠️ WebAdminServer läuft bereits");
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WebAdminServer kann nicht starten - kein WiFi!");
    return false;
  }

  Serial.println("\n🌐 Starte Web-Admin-Interface...");

  // Webserver Routes
  server.on("/", [this]() { this->handleRoot(); });
  server.on("/mqtt", HTTP_POST, [this]() { this->handleSaveMQTT(); });
  server.on("/status", [this]() { this->handleStatus(); });
  server.on("/restart", HTTP_POST, [this]() { this->handleRestart(); });

  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.printf("✓ Web-Admin gestartet auf http://%s\n", ip.toString().c_str());
  Serial.println("  Öffne im Browser für MQTT-Konfiguration");

  running = true;
  return true;
}

void WebAdminServer::stop() {
  if (!running) return;

  server.stop();
  running = false;
  Serial.println("✓ WebAdminServer gestoppt");
}

void WebAdminServer::handle() {
  if (!running) return;
  server.handleClient();
}

void WebAdminServer::handleRoot() {
  Serial.println("📄 Admin-Seite angefordert");
  server.send(200, "text/html", getAdminPage());
}

void WebAdminServer::handleSaveMQTT() {
  Serial.println("💾 Speichere MQTT-Konfiguration...");

  // Lese bestehende Config um WiFi-Daten zu behalten
  DeviceConfig cfg;
  if (configManager.isConfigured()) {
    cfg = configManager.getConfig();
  } else {
    memset(&cfg, 0, sizeof(cfg));
    cfg.mqtt_port = 1883;
  }

  // Überschreibe nur MQTT-Daten
  if (server.hasArg("mqtt_host")) {
    String host = server.arg("mqtt_host");
    strncpy(cfg.mqtt_host, host.c_str(), CONFIG_MQTT_HOST_MAX - 1);
  }

  if (server.hasArg("mqtt_port")) {
    cfg.mqtt_port = server.arg("mqtt_port").toInt();
  }

  if (server.hasArg("mqtt_user")) {
    String user = server.arg("mqtt_user");
    strncpy(cfg.mqtt_user, user.c_str(), CONFIG_MQTT_USER_MAX - 1);
  }

  if (server.hasArg("mqtt_pass")) {
    String pass = server.arg("mqtt_pass");
    strncpy(cfg.mqtt_pass, pass.c_str(), CONFIG_MQTT_PASS_MAX - 1);
  }

  // Validierung
  if (strlen(cfg.mqtt_host) == 0) {
    server.send(400, "text/html", "<h1>Fehler: MQTT Host ist erforderlich!</h1>");
    return;
  }

  // Speichere Konfiguration (WiFi-Daten bleiben erhalten!)
  if (configManager.save(cfg)) {
    Serial.println("✓ MQTT-Konfiguration erfolgreich gespeichert");
    server.send(200, "text/html", getSuccessPage());
  } else {
    Serial.println("❌ Fehler beim Speichern der Konfiguration");
    server.send(500, "text/html", "<h1>Fehler beim Speichern!</h1>");
  }
}

void WebAdminServer::handleStatus() {
  server.send(200, "application/json", getStatusJSON());
}

void WebAdminServer::handleRestart() {
  server.send(200, "text/html", "<h1>Gerät wird neu gestartet...</h1>");
  delay(1000);
  ESP.restart();
}

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();

  String html = R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Tab5 Admin</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 800px;
      margin: 0 auto;
      padding: 40px;
    }
    h1 {
      color: #2d3748;
      font-size: 32px;
      margin-bottom: 8px;
    }
    .subtitle {
      color: #718096;
      margin-bottom: 32px;
      font-size: 16px;
    }
    .status-card {
      background: #f7fafc;
      border-left: 4px solid #51cf66;
      padding: 16px;
      border-radius: 8px;
      margin-bottom: 24px;
    }
    .status-item {
      display: flex;
      justify-content: space-between;
      padding: 8px 0;
      border-bottom: 1px solid #e2e8f0;
    }
    .status-item:last-child {
      border-bottom: none;
    }
    .status-label {
      color: #4a5568;
      font-weight: 500;
    }
    .status-value {
      color: #2d3748;
      font-family: monospace;
    }
    .section {
      margin-bottom: 32px;
    }
    .section-title {
      color: #667eea;
      font-size: 14px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-bottom: 16px;
      padding-bottom: 8px;
      border-bottom: 2px solid #e2e8f0;
    }
    .form-group {
      margin-bottom: 16px;
    }
    label {
      display: block;
      color: #4a5568;
      font-size: 14px;
      font-weight: 500;
      margin-bottom: 6px;
    }
    input {
      width: 100%;
      padding: 12px 16px;
      border: 2px solid #e2e8f0;
      border-radius: 8px;
      font-size: 15px;
      transition: all 0.2s;
      font-family: inherit;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
    }
    .hint {
      color: #a0aec0;
      font-size: 12px;
      margin-top: 4px;
    }
    .btn {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s, box-shadow 0.2s;
      margin-top: 8px;
    }
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary {
      background: #718096;
      margin-top: 16px;
    }
    .icon {
      font-size: 48px;
      text-align: center;
      margin-bottom: 16px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="icon">🔧</div>
    <h1>Tab5 Admin-Panel</h1>
    <p class="subtitle">MQTT-Konfiguration & System-Status</p>

    <div class="status-card">
      <div class="status-item">
        <span class="status-label">WiFi Status:</span>
        <span class="status-value">✓ Verbunden</span>
      </div>
      <div class="status-item">
        <span class="status-label">SSID:</span>
        <span class="status-value">)html" + String(cfg.wifi_ssid) + R"html(</span>
      </div>
      <div class="status-item">
        <span class="status-label">IP-Adresse:</span>
        <span class="status-value">)html" + WiFi.localIP().toString() + R"html(</span>
      </div>
      <div class="status-item">
        <span class="status-label">Freier Heap:</span>
        <span class="status-value">)html" + String(ESP.getFreeHeap() / 1024) + R"html( KB</span>
      </div>
    </div>

    <form action="/mqtt" method="POST">
      <div class="section">
        <div class="section-title">🔌 MQTT-Broker Einstellungen</div>
        <div class="form-group">
          <label for="mqtt_host">Host / IP-Adresse</label>
          <input type="text" id="mqtt_host" name="mqtt_host" placeholder="192.168.1.100" value=")html" + String(cfg.mqtt_host) + R"html(" required>
        </div>
        <div class="form-group">
          <label for="mqtt_port">Port</label>
          <input type="number" id="mqtt_port" name="mqtt_port" placeholder="1883" value=")html" + String(cfg.mqtt_port > 0 ? cfg.mqtt_port : 1883) + R"html(">
        </div>
        <div class="form-group">
          <label for="mqtt_user">Benutzername</label>
          <input type="text" id="mqtt_user" name="mqtt_user" placeholder="mqtt_user" value=")html" + String(cfg.mqtt_user) + R"html(">
          <div class="hint">Optional - leer lassen für anonyme Verbindung</div>
        </div>
        <div class="form-group">
          <label for="mqtt_pass">Passwort</label>
          <input type="password" id="mqtt_pass" name="mqtt_pass" placeholder="mqtt_password" value=")html" + String(cfg.mqtt_pass) + R"html(">
          <div class="hint">Optional</div>
        </div>
      </div>

      <button type="submit" class="btn">💾 MQTT-Einstellungen speichern</button>
    </form>

    <form action="/restart" method="POST" onsubmit="return confirm('Gerät wirklich neu starten?');">
      <button type="submit" class="btn btn-secondary">🔄 Gerät neu starten</button>
    </form>
  </div>
</body>
</html>
)html";

  return html;
}

String WebAdminServer::getSuccessPage() {
  String html = R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Gespeichert</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 500px;
      width: 100%;
      padding: 40px;
      text-align: center;
    }
    .icon {
      font-size: 72px;
      margin-bottom: 24px;
      animation: bounce 0.6s;
    }
    @keyframes bounce {
      0%, 100% { transform: translateY(0); }
      50% { transform: translateY(-20px); }
    }
    h1 {
      color: #2d3748;
      font-size: 28px;
      margin-bottom: 16px;
    }
    p {
      color: #718096;
      font-size: 16px;
      line-height: 1.6;
      margin-bottom: 24px;
    }
    a {
      display: inline-block;
      padding: 12px 24px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      text-decoration: none;
      border-radius: 8px;
      font-weight: 600;
    }
  </style>
  <script>
    setTimeout(function() {
      window.location.href = '/';
    }, 5000);
  </script>
</head>
<body>
  <div class="container">
    <div class="icon">✅</div>
    <h1>Erfolgreich gespeichert!</h1>
    <p>Die MQTT-Konfiguration wurde erfolgreich gespeichert.<br>Das Gerät wird sich mit dem MQTT-Broker verbinden.</p>
    <a href="/">Zurück zur Übersicht</a>
  </div>
</body>
</html>
)html";

  return html;
}

String WebAdminServer::getStatusJSON() {
  const DeviceConfig& cfg = configManager.getConfig();

  String json = "{";
  json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"wifi_ssid\":\"" + String(cfg.wifi_ssid) + "\",";
  json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mqtt_host\":\"" + String(cfg.mqtt_host) + "\",";
  json += "\"mqtt_port\":" + String(cfg.mqtt_port) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";

  return json;
}
