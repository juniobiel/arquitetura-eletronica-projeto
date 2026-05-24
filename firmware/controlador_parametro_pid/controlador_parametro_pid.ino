#include <DHT.h>

// Definições de Pinos
const int LDR_PIN = 34;       
const int LED_PWM_PIN = 27;   
const int DHT_PIN = 4;        

// Configurações do DHT11
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// Configurações do PWM 
const int freq = 5000;        
const int resolution = 8;     

// Variáveis para temporização
unsigned long tempoAnteriorDHT = 0;
const unsigned long intervaloDHT = 2000; 

// Filtro EMA
float ldrFiltrado = 0;
const float alpha = 0.1; 

// --- PARÂMETROS DO CONTROLADOR SINTONIZADO (ZIEGLER-NICHOLS) ---
double kp = 1.0;   // Reduzido para menos solavancos
double ki = 0.5;   // Reduzido drasticamente para uma transição suave
double kd = 0.0;   

double setpointLux = 200.0;

// Memória do Controlador
double erroAcumulado = 0.0;
double erroAnterior = 0.0;
unsigned long tempoAnteriorPID = 0;

// Tempo de amostragem fixo para a malha de controle (50ms)
const unsigned long intervaloPID = 50;

void setup() {
  Serial.begin(115200);
  
  dht.begin();
  ledcAttach(LED_PWM_PIN, freq, resolution);
  
  ldrFiltrado = analogRead(LDR_PIN);
  
  Serial.println("Controle Malha Fechada PI - Iniciado!");
}

void loop() {
  unsigned long tempoAtual = millis();

  // --- 1. LEITURA DO SENSOR ---
  int ldrRaw = analogRead(LDR_PIN);
  ldrFiltrado = (alpha * ldrRaw) + ((1.0 - alpha) * ldrFiltrado);
  
  // --- 2. REGRESSÃO PARA LUX ---
  float adcSeguro = constrain(ldrFiltrado, 1.0, 4094.0);
  float rLDR = 10000.0 * (4095.0 / adcSeguro - 1.0);
  float luxReal = 320000.0 * pow(rLDR, -0.90);

  // --- 3. MALHA PID COM TEMPO DISCRETO E ZONA MORTA ---
  // Só executa o cálculo do PID a cada 50ms (permite que o LDR "respire")
  if (tempoAtual - tempoAnteriorPID >= intervaloPID) {
    double dt = (tempoAtual - tempoAnteriorPID) / 1000.0; 
    
    double erro = setpointLux - luxReal;
    
    // ZONA MORTA (Deadband): Se o erro for menor que 20 Lux, ignora a correção.
    // Isso evita o efeito "pisca-pisca" por ruído do sensor.
    if (abs(erro) < 50.0) {
      erro = 0; 
    }

    // Cálculos PID
    double P = kp * erro;
    
    // Só acumula o erro se estivermos fora da zona morta
    if (erro != 0) {
      erroAcumulado += erro * dt;
    }
    // Anti-Windup com limite estreito
    erroAcumulado = constrain(erroAcumulado, -255.0 / ki, 255.0 / ki); 
    
    double I = ki * erroAcumulado;
    
    // Saída
    double saidaPID = P + I;
    
    // --- 4. ATUAÇÃO (PWM) ---
    int pwmValue = (int)saidaPID;
    pwmValue = constrain(pwmValue, 0, 255);
    ledcWrite(LED_PWM_PIN, pwmValue);
    
    // Atualiza a memória para o próximo ciclo
    tempoAnteriorPID = tempoAtual;
  }

  // --- 5. EXIBIÇÃO ---
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
      Serial.print(" | Setpoint: "); Serial.print(setpointLux, 1); Serial.println(" Lux");
      Serial.print("Lux Real: "); Serial.print(luxReal, 1);
      Serial.print(" | PWM MOSFET: "); Serial.println((int)constrain(kp*(setpointLux - luxReal) + ki*erroAcumulado, 0, 255));
      Serial.println("-------------------------");
    }
  }
}