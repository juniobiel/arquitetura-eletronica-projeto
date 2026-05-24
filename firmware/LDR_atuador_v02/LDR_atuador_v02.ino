#include <DHT.h>

// Definições de Pinos
const int LDR_PIN = 34;       
const int LED_PWM_PIN = 27;   
const int DHT_PIN = 4;        

// Configurações do DHT11
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// Configurações do PWM (ESP32 v3.x)
const int freq = 5000;        
const int resolution = 8;     

// Variáveis para temporização não-bloqueante
unsigned long tempoAnteriorDHT = 0;
const unsigned long intervaloDHT = 2000; 

// Variáveis para suavização do LDR (Filtro EMA)
float ldrFiltrado = 0;
const float alpha = 0.1; 

// --- 1. PARAMETRIZAÇÃO DO CONTROLE PWM ---
const int LDR_ESCURO = 40;   
const int LDR_CLARO = 3350;  

void setup() {
  Serial.begin(115200);
  
  dht.begin();
  ledcAttach(LED_PWM_PIN, freq, resolution);
  
  // Leitura inicial para estabilizar o filtro
  ldrFiltrado = analogRead(LDR_PIN);
  
  Serial.println("Sistema de Controle Iniciado - Equacao Caracteristica Ativa!");
}

void loop() {
  unsigned long tempoAtual = millis();

  // --- LEITURA E FILTRAGEM ---
  int ldrRaw = analogRead(LDR_PIN);
  ldrFiltrado = (alpha * ldrRaw) + ((1.0 - alpha) * ldrFiltrado);
  
  // --- ATUAÇÃO (PWM) ---
  int pwmValue = map((int)ldrFiltrado, LDR_ESCURO, LDR_CLARO, 255, 0);
  pwmValue = constrain(pwmValue, 0, 255);
  ledcWrite(LED_PWM_PIN, pwmValue);

  // --- MODELAGEM MATEMÁTICA (REGRESSÃO) ---
  float luxReal = 0.0;
  
  // Trava de segurança para evitar divisão por zero ou resistências negativas nos extremos
  float adcSeguro = constrain(ldrFiltrado, 1.0, 4094.0);
  
  // A. Conversão do ADC para Resistência do LDR em Ohms (Física do Divisor de Tensão)
  float rLDR = 10000.0 * (4095.0 / adcSeguro - 1.0);
  
  // B. Aplicação da Curva de Regressão Logarítmica (Power Law) baseada nos seus dados
  luxReal = 320000.0 * pow(rLDR, -0.90);

  // --- EXIBIÇÃO NO MONITOR SERIAL ---
  if (tempoAtual - tempoAnteriorDHT >= intervaloDHT) {
    tempoAnteriorDHT = tempoAtual;
    
    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (isnan(umidade) || isnan(temperatura)) {
      Serial.println("Falha ao ler o sensor DHT11.");
    } else {
      Serial.println("--- Status do Sistema ---");
      Serial.print("Temp: "); Serial.print(temperatura); Serial.print(" °C | ");
      Serial.print("Umid: "); Serial.print(umidade); Serial.println(" %");
      Serial.print("LDR Raw: "); Serial.print(ldrRaw);
      Serial.print(" | PWM MOSFET: "); Serial.print(pwmValue);
      Serial.print(" | Resistencia: "); Serial.print(rLDR / 1000.0, 1); Serial.println(" kOhm");
      Serial.print("Iluminacao Real (Calculada): "); Serial.print(luxReal, 1); Serial.println(" Lux");
      Serial.println("-------------------------");
    }
  }
}