/*
 * ============================================================
 *  FIRMWARE FINAL — Sistema Inteligente de Iluminação Adaptativa
 *  ESP32 + LDR + DHT11 + PID + WiFi + MQTT (HiveMQ Cloud)
 * ============================================================
 *
 *  CIRCUITO:
 *    3.3V ─── LDR ─── GPIO34 (ADC) ─── 10kΩ ─── GND
 *    GPIO27 (PWM) ─── [330Ω] ─── Gate MOSFET IRLZ44N
 *    VCC ─── R_LED ─── LED ─── Drain MOSFET ─── GND
 *    GPIO4 ─── DHT11 Data
 *
 *  FUNCIONALIDADES:
 *    - Controle PI (Kp=3.15, Ki=18.9) sintonizado por Ziegler-Nichols
 *    - 3 modos: Automático (PI), Manual (duty fixo), Desligado
 *    - Publicação MQTT a cada 1.5s no tópico "luminosidade/dados"
 *    - Recebe comandos do dashboard via "luminosidade/controle"
 *    - Filtro EMA no LDR + anti-windup no integrador
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ═══════════════════════════════════════════════════════════════
//  CONFIGURAÇÕES — ALTERE AQUI CONFORME NECESSÁRIO
// ═══════════════════════════════════════════════════════════════

// ── Wi-Fi ────────────────────────────────────────────────────
const char* WIFI_SSID = "Redmi Note 13";     // ← WiFi do lab
const char* WIFI_PASS = "murilo123";          // ← Senha do WiFi

// ── MQTT (HiveMQ Cloud) ─────────────────────────────────────
const char* MQTT_BROKER = "377271ae85c448099dc71d8bd61e92c6.s1.eu.hivemq.cloud";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "FabricioTheTuffest";
const char* MQTT_PASS   = "Fabricio67";
const char* TOPIC_DATA    = "luminosidade/dados";     // ESP32 → Dashboard
const char* TOPIC_CONTROL = "luminosidade/controle";  // Dashboard → ESP32
const char* CLIENT_ID     = "esp32-luxcontrol";

// ── Pinos ────────────────────────────────────────────────────
const int LDR_PIN     = 34;
const int LED_PWM_PIN = 27;
const int DHT_PIN     = 4;

// ── PWM ──────────────────────────────────────────────────────
const int PWM_FREQ       = 5000;
const int PWM_RESOLUTION = 8;     // 0-255
const int PWM_MAX        = 255;

// ── Sensor DHT ───────────────────────────────────────────────
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ── Parâmetros PI (Ziegler-Nichols) ──────────────────────────
double kp = 3.15;
double ki = 18.9;
double kd = 0.0;

// ── LDR Regressão ────────────────────────────────────────────
const float LDR_A     = 320000.0;
const float LDR_GAMMA = 0.90;
const float R_FIXO    = 10000.0;

// ── Filtro EMA ───────────────────────────────────────────────
const float EMA_ALPHA = 0.1;

// ═══════════════════════════════════════════════════════════════
//  VARIÁVEIS GLOBAIS
// ═══════════════════════════════════════════════════════════════

// Estado do sistema
String modo = "auto";           // "auto" | "manual" | "off"
double setpointLux = 200.0;     // Setpoint de luminosidade (lux)
int    manualDuty  = 50;        // Duty cycle manual (0-100 %)

// Controlador PI
double erroAcumulado = 0.0;
double erroAnterior  = 0.0;
unsigned long tempoAnteriorPID = 0;

// Sensor
float ldrFiltrado = 0;
float luxReal     = 0;
float temperatura = 0;
float umidade     = 0;
int   pwmAtual    = 0;

// Temporização
unsigned long ultimoEnvioMQTT = 0;
const unsigned long INTERVALO_MQTT = 1500;  // Publicar a cada 1.5s

unsigned long ultimaLeituraDHT = 0;
const unsigned long INTERVALO_DHT = 2000;   // Ler DHT a cada 2s

// MQTT + WiFi
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ═══════════════════════════════════════════════════════════════
//  FUNÇÕES AUXILIARES
// ═══════════════════════════════════════════════════════════════

// ── Conversão LDR → Lux ─────────────────────────────────────
float calcularLux(float adcFiltrado) {
  float adcSeguro = constrain(adcFiltrado, 1.0, 4094.0);
  float rLDR = R_FIXO * (4095.0 / adcSeguro - 1.0);
  return LDR_A * pow(rLDR, -LDR_GAMMA);
}

// ── WiFi ─────────────────────────────────────────────────────
void conectarWiFi() {
  Serial.print("[WiFi] Conectando a ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] ✓ Conectado! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] ✗ Falha na conexão. Continuando sem WiFi...");
  }
}

// ── MQTT Callback (recebe comandos do dashboard) ─────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Parsear JSON recebido
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  
  if (err) {
    Serial.println("[MQTT] Erro ao parsear JSON recebido");
    return;
  }
  
  const char* comando = doc["comando"];
  
  if (strcmp(comando, "setpoint") == 0) {
    setpointLux = doc["valor"].as<double>();
    Serial.printf("[MQTT] Setpoint → %.1f lux\n", setpointLux);
    
  } else if (strcmp(comando, "modo") == 0) {
    modo = doc["valor"].as<String>();
    // Reset do integrador ao trocar de modo
    erroAcumulado = 0.0;
    erroAnterior = 0.0;
    Serial.printf("[MQTT] Modo → %s\n", modo.c_str());
    
  } else if (strcmp(comando, "duty") == 0) {
    manualDuty = constrain(doc["valor"].as<int>(), 0, 100);
    Serial.printf("[MQTT] Duty manual → %d%%\n", manualDuty);
    
  } else {
    Serial.printf("[MQTT] Comando desconhecido: %s\n", comando);
  }
}

// ── MQTT Conexão ─────────────────────────────────────────────
void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  secureClient.setInsecure();  // Aceita certificado auto-assinado (HiveMQ Cloud)
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  
  int tentativas = 0;
  while (!mqttClient.connected() && tentativas < 3) {
    Serial.print("[MQTT] Conectando ao HiveMQ Cloud...");
    if (mqttClient.connect(CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println(" ✓ Conectado!");
      mqttClient.subscribe(TOPIC_CONTROL);
      Serial.printf("[MQTT] Inscrito em: %s\n", TOPIC_CONTROL);
    } else {
      Serial.printf(" ✗ Falha (estado: %d). Tentando novamente...\n", mqttClient.state());
      tentativas++;
      delay(2000);
    }
  }
}

// ── Publicar dados via MQTT ──────────────────────────────────
void publicarDados() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["lux"]      = round(luxReal * 10.0) / 10.0;     // 1 casa decimal
  doc["pwm"]      = round((pwmAtual / 255.0) * 100.0); // Converter para %
  doc["setpoint"] = (int)setpointLux;
  doc["temp"]     = round(temperatura * 10.0) / 10.0;
  doc["umid"]     = (int)umidade;
  doc["modo"]     = modo;
  
  char payload[256];
  serializeJson(doc, payload);
  
  if (mqttClient.publish(TOPIC_DATA, payload)) {
    Serial.printf("[MQTT] 📤 %s\n", payload);
  } else {
    Serial.println("[MQTT] ✗ Erro ao publicar");
  }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n╔══════════════════════════════════════════════════╗");
  Serial.println("║  LuxControl — Sistema de Iluminação Adaptativa  ║");
  Serial.println("║  PI + WiFi + MQTT + Dashboard                   ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
  
  // Inicializar sensores
  dht.begin();
  ledcAttach(LED_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  
  // Leitura inicial para estabilizar filtro EMA
  ldrFiltrado = analogRead(LDR_PIN);
  
  // Conectar WiFi e MQTT
  conectarWiFi();
  conectarMQTT();
  
  tempoAnteriorPID = millis();
  
  Serial.printf("[SYS] Parâmetros PI: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", kp, ki, kd);
  Serial.printf("[SYS] Setpoint: %.1f lux | Modo: %s\n", setpointLux, modo.c_str());
  Serial.println("[SYS] Sistema iniciado! ✓\n");
}

// ═══════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ═══════════════════════════════════════════════════════════════
void loop() {
  unsigned long agora = millis();
  
  // ── Manter conexão MQTT ────────────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      conectarMQTT();
    }
    mqttClient.loop();
  }
  
  // ── 1. LEITURA DO SENSOR (LDR + Filtro EMA) ───────────────
  int ldrRaw = analogRead(LDR_PIN);
  ldrFiltrado = (EMA_ALPHA * ldrRaw) + ((1.0 - EMA_ALPHA) * ldrFiltrado);
  luxReal = calcularLux(ldrFiltrado);
  
  // ── 2. CONTROLADOR PI ──────────────────────────────────────
  double dt = (agora - tempoAnteriorPID) / 1000.0;
  
  if (dt > 0.0) {
    if (modo == "auto") {
      // ── Controle PI ──
      double erro = setpointLux - luxReal;
      
      // Proporcional
      double P = kp * erro;
      
      // Integral com anti-windup
      erroAcumulado += erro * dt;
      erroAcumulado = constrain(erroAcumulado, -255.0 / ki, 255.0 / ki);
      double I = ki * erroAcumulado;
      
      // Derivativo (Kd=0 no caso atual, mas pronto para usar)
      double D = kd * ((erro - erroAnterior) / dt);
      
      // Saída
      double saidaPID = P + I + D;
      pwmAtual = constrain((int)saidaPID, 0, PWM_MAX);
      
      erroAnterior = erro;
      
    } else if (modo == "manual") {
      // ── Duty fixo ──
      pwmAtual = map(manualDuty, 0, 100, 0, PWM_MAX);
      erroAcumulado = 0.0;
      erroAnterior = 0.0;
      
    } else {
      // ── Desligado ──
      pwmAtual = 0;
      erroAcumulado = 0.0;
      erroAnterior = 0.0;
    }
    
    tempoAnteriorPID = agora;
  }
  
  // ── 3. ATUAÇÃO (PWM → MOSFET → LED) ───────────────────────
  ledcWrite(LED_PWM_PIN, pwmAtual);
  
  // ── 4. LEITURA DHT11 (a cada 2s) ──────────────────────────
  if (agora - ultimaLeituraDHT >= INTERVALO_DHT) {
    ultimaLeituraDHT = agora;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperatura = t;
    if (!isnan(h)) umidade = h;
  }
  
  // ── 5. PUBLICAR VIA MQTT (a cada 1.5s) ────────────────────
  if (agora - ultimoEnvioMQTT >= INTERVALO_MQTT) {
    ultimoEnvioMQTT = agora;
    
    publicarDados();
    
    // Debug no Serial Monitor
    Serial.printf("[%s] Lux:%.1f | SP:%.0f | PWM:%d | T:%.1f°C | H:%.0f%%\n",
                  modo.c_str(), luxReal, setpointLux, pwmAtual, temperatura, umidade);
  }
}
