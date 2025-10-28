#include <WiFi.h>
#include <WebServer.h>

// ===== CONFIGURACIÓN WIFI =====
const char* ssid = "iPhone de Paisa";        // <- CAMBIAR: Nombre de tu red WiFi
const char* password = "Password123";     // <- CAMBIAR: Contraseña de tu red WiFi

// ===== PIN DEL LED =====
#define LED_PIN 2  // LED integrado del ESP32

// ===== SERVIDOR WEB =====
WebServer server(80);  // Servidor HTTP en puerto 80

// ===== ESTADO DEL LED =====
bool ledState = false;

// ===== FUNCIÓN: Página principal HTML =====
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='es'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Control LED ESP32</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin: 50px; background: #1a1a1a; color: white; }";
  html += "h1 { color: #4CAF50; }";
  html += ".container { max-width: 500px; margin: 0 auto; padding: 20px; background: #2a2a2a; border-radius: 10px; }";
  html += ".button { display: inline-block; padding: 15px 40px; margin: 10px; font-size: 20px; ";
  html += "border: none; border-radius: 5px; cursor: pointer; text-decoration: none; color: white; }";
  html += ".btn-on { background-color: #4CAF50; }";
  html += ".btn-on:hover { background-color: #45a049; }";
  html += ".btn-off { background-color: #f44336; }";
  html += ".btn-off:hover { background-color: #da190b; }";
  html += ".status { font-size: 24px; margin: 20px; padding: 15px; border-radius: 5px; }";
  html += ".status-on { background-color: #4CAF50; }";
  html += ".status-off { background-color: #555; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>🔌 Control LED ESP32</h1>";
  html += "<p>Control remoto del LED en GPIO 5</p>";
  
  // Mostrar estado actual
  html += "<div class='status ";
  html += ledState ? "status-on" : "status-off";
  html += "'>";
  html += "LED: ";
  html += ledState ? "🟢 ENCENDIDO" : "⚫ APAGADO";
  html += "</div>";
  
  // Botones de control
  html += "<div>";
  html += "<a href='/on' class='button btn-on'>⚡ ENCENDER</a>";
  html += "<a href='/off' class='button btn-off'>⏸ APAGAR</a>";
  html += "</div>";
  
  html += "<p style='margin-top: 30px; color: #888;'>IP: " + WiFi.localIP().toString() + "</p>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// ===== FUNCIÓN: Encender LED =====
void handleLedOn() {
  Serial.println("\n========================================");
  Serial.println("📥 SOLICITUD RECIBIDA: /on");
  Serial.print("Cliente IP: ");
  Serial.println(server.client().remoteIP());
  
  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("✅ LED ENCENDIDO en GPIO 2 (LED integrado)");
  Serial.println("✅ Dato enviado: LED=ON");
  Serial.println("========================================\n");
  
  // Redirigir a la página principal
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== FUNCIÓN: Apagar LED =====
void handleLedOff() {
  Serial.println("\n========================================");
  Serial.println("📥 SOLICITUD RECIBIDA: /off");
  Serial.print("Cliente IP: ");
  Serial.println(server.client().remoteIP());
  
  ledState = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("⛔ LED APAGADO en GPIO 2 (LED integrado)");
  Serial.println("✅ Dato enviado: LED=OFF");
  Serial.println("========================================\n");
  
  // Redirigir a la página principal
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== FUNCIÓN: Página no encontrada =====
void handleNotFound() {
  String message = "Error 404 - Página no encontrada\n\n";
  message += "URI: " + server.uri();
  message += "\nMétodo: " + String((server.method() == HTTP_GET) ? "GET" : "POST");
  server.send(404, "text/plain", message);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("🚀 ESP32 - Control LED por WiFi");
  Serial.println("=================================\n");
  
  // Configurar pin del LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("✅ LED integrado configurado en GPIO 2");
  
  // Conectar a WiFi
  Serial.print("📡 Conectando a WiFi: ");
  Serial.println(ssid);
  Serial.print("🔑 Contraseña: ");
  Serial.println(password);
  
  WiFi.begin(ssid, password);
  
  // Esperar conexión con indicador visual
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Si no conecta después de 20 intentos (10 segundos)
    if (attempts > 20) {
      Serial.println("\n❌ ERROR: No se pudo conectar al WiFi");
      Serial.println("Verifica el SSID y contraseña en el código");
      Serial.println("Reiniciando en 5 segundos...");
      delay(5000);
      ESP.restart();
    }
  }
  
  Serial.println("\n✅ WiFi conectado exitosamente!");
  Serial.print("📶 Fuerza de señal (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("🌐 Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("🎭 Máscara de subred: ");
  Serial.println(WiFi.subnetMask());
  Serial.println("\n=================================");
  Serial.println("📍 DIRECCIÓN IP:");
  Serial.print("   http://");
  Serial.println(WiFi.localIP());
  Serial.println("=================================");
  Serial.println("\n💡 Abre esta dirección en tu navegador");
  Serial.println("   para controlar el LED\n");
  
  // Configurar rutas del servidor web
  server.on("/", handleRoot);           // Página principal
  server.on("/on", handleLedOn);        // Encender LED
  server.on("/off", handleLedOff);      // Apagar LED
  server.onNotFound(handleNotFound);    // Página no encontrada
  
  // Iniciar servidor
  server.begin();
  Serial.println("🌐 Servidor HTTP iniciado");
  Serial.println("=================================\n");
}

// ===== LOOP =====
void loop() {
  // Manejar peticiones HTTP
  server.handleClient();
  
  // Verificar si se perdió la conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n⚠️ Conexión WiFi perdida. Reconectando...");
    Serial.print("📡 Estado WiFi: ");
    Serial.println(WiFi.status());
    WiFi.reconnect();
    delay(5000);
  }
}
