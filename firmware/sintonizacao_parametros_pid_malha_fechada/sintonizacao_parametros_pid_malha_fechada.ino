/*
 * ============================================================
 *  ETAPA 3 — MÉTODO DO GANHO ÚLTIMO (MALHA FECHADA)
 *  Ziegler-Nichols — Ultimate Gain Method
 * ============================================================
 *
 *  PRINCÍPIO:
 *    Liga um controlador P puro (sem I, sem D).
 *    Aumenta Kp gradualmente até o sistema oscilar de forma
 *    sustentada (nem amortece, nem diverge).
 *    Anota Ku (ganho último) e Pu (período de oscilação).
 *    Calcula Kp, Ti, Td pelas tabelas de Ziegler-Nichols.
 *
 *  PROCEDIMENTO:
 *    1. Defina o setpoint com 'P' (ex: 200 lux)
 *    2. Comece com KP_INICIAL baixo (ex: 0.001)
 *    3. Use '+' para aumentar Kp em passos de KP_PASSO
 *    4. Observe a resposta pelo monitoramento contínuo
 *    5. Quando a oscilação for sustentada, pressione 'O'
 *       (o código mede Pu automaticamente)
 *    6. O código calcula e exibe Kp, Ti, Td
 *
 *  COMO RECONHECER OSCILAÇÃO SUSTENTADA:
 *    O sinal de lux fica oscilando com amplitude constante
 *    (não amortece, não cresce). No Serial Plotter fica uma
 *    senoide regular. No Serial Monitor os valores ficam
 *    subindo e descendo em torno do setpoint.
 * ============================================================
 */

#include "driver/ledc.h"

// ─── Pinos ──────────────────────────────────────────────────
#define PWM_PIN       18
#define ADC_PIN       34

// ─── PWM ─────────────────────────────────────────────────────
#define PWM_FREQ      5000
#define PWM_RES       LEDC_TIMER_10_BIT
#define PWM_MAX       1023

// ─── LDR calibração ──────────────────────────────────────────
#define R_FIXO        10000.0
#define VCC_LDR       3.3
#define LDR_A         441474.7     // ← do seu resultado da Etapa 1
#define LDR_GAMMA     0.5968      // ← do seu resultado da Etapa 1

// ─── Parâmetros do controlador ───────────────────────────────
float setpoint  = 200.0;   // lux desejado — ajuste com 'P'
float Kp        = 0.001;   // ganho proporcional — aumentar com '+'
float KP_PASSO  = 0.0005;  // incremento por pressionamento de '+'
                            // Ajuste conforme a escala do seu sistema

// ─── Taxa de amostragem ──────────────────────────────────────
#define TS_MS         20       // período de amostragem (ms) = 50 Hz

// ─── Detecção automática de oscilação ───────────────────────
#define BUF_OSC       200      // tamanho do buffer de medição (200 * 20ms = 4s)
float  buf_lux[BUF_OSC];
int    buf_idx  = 0;
bool   medindo_pu = false;

// ─────────────────────────────────────────────────────────────

void pwm_init() {
  ledc_timer_config_t tmr = {
    .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = PWM_RES,
    .timer_num  = LEDC_TIMER_0, .freq_hz = PWM_FREQ, .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&tmr);
  ledc_channel_config_t ch = {
    .gpio_num   = PWM_PIN, .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0, .timer_sel  = LEDC_TIMER_0,
    .duty       = 0, .hpoint = 0
  };
  ledc_channel_config(&ch);
}

void setPWM(int duty) {
  duty = constrain(duty, 0, PWM_MAX);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

float lerLux() {
  long soma = 0;
  for (int i = 0; i < 8; i++) { soma += analogRead(ADC_PIN); delayMicroseconds(50); }
  float v = (soma / 8.0) * (VCC_LDR / 4095.0);
  if (v <= 0.01 || v >= VCC_LDR - 0.01) return -1;
  float r = R_FIXO * (VCC_LDR - v) / v;
  return pow(10.0, (log10(LDR_A) - log10(r)) / LDR_GAMMA);
}

// ── Mede o período de oscilação Pu a partir do buffer ─────────
float calcularPu() {
  // Encontra cruzamentos de zero em relação à média (setpoint)
  float media = 0;
  for (int i = 0; i < BUF_OSC; i++) media += buf_lux[i];
  media /= BUF_OSC;

  // Conta cruzamentos positivos (subindo)
  int n_cruzamentos = 0;
  int primeiro = -1, ultimo = -1;
  for (int i = 1; i < BUF_OSC; i++) {
    if (buf_lux[i - 1] < media && buf_lux[i] >= media) {
      if (primeiro == -1) primeiro = i;
      ultimo = i;
      n_cruzamentos++;
    }
  }

  if (n_cruzamentos < 2) return -1; // sinal insuficiente

  // Período = tempo entre o 1º e o último cruzamento / (n-1)
  float Pu_amostras = (float)(ultimo - primeiro) / (n_cruzamentos - 1);
  return Pu_amostras * TS_MS; // converte amostras → ms
}

void calcularZN(float Ku, float Pu_ms) {
  Serial.println(F("\n══════════════════════════════════════════════════"));
  Serial.println(F("  PARÂMETROS Z-N — MÉTODO DO GANHO ÚLTIMO"));
  Serial.println(F("══════════════════════════════════════════════════"));
  Serial.printf ("  Ku (ganho último)   = %.6f\n", Ku);
  Serial.printf ("  Pu (período último) = %.1f ms  (%.3f s)\n", Pu_ms, Pu_ms / 1000.0);
  Serial.println(F("\n  ┌─────────────────┬─────────┬─────────┬─────────┐"));
  Serial.println(F("  │ Tipo            │    Kp   │   Ti    │   Td    │"));
  Serial.println(F("  ├─────────────────┼─────────┼─────────┼─────────┤"));
  Serial.printf ("  │ P  only         │ %7.5f │    —    │    —    │\n", 0.5 * Ku);
  Serial.printf ("  │ PI              │ %7.5f │ %5.0f ms│    —    │\n",
                 0.45 * Ku, 0.833 * Pu_ms);
  Serial.printf ("  │ PID (clássico)  │ %7.5f │ %5.0f ms│ %5.0f ms│\n",
                 0.6 * Ku, 0.5 * Pu_ms, 0.125 * Pu_ms);
  Serial.printf ("  │ PID (Tyreus-L.) │ %7.5f │ %5.0f ms│ %5.0f ms│\n",
                 0.4545 * Ku, 2.2 * Pu_ms, 0.1591 * Pu_ms);
  Serial.println(F("  └─────────────────┴─────────┴─────────┴─────────┘"));

  float Kp_pid = 0.6  * Ku;
  float Ti_pid = 0.5  * Pu_ms;
  float Td_pid = 0.125 * Pu_ms;
  float Ki_pid = Kp_pid / Ti_pid;
  float Kd_pid = Kp_pid * Td_pid;

  Serial.println(F("\n  Para uso no código PID discreto:"));
  Serial.printf ("    Kp = %.6f\n", Kp_pid);
  Serial.printf ("    Ki = Kp/Ti = %.8f  (por ms)\n", Ki_pid);
  Serial.printf ("    Kd = Kp*Td = %.5f  (ms)\n",     Kd_pid);
  Serial.printf ("    Ti = %.1f ms  |  Td = %.1f ms\n", Ti_pid, Td_pid);
  Serial.println(F("\n  ⚠ Dica: Z-N tende a ser agressivo."));
  Serial.println(F("    Se houver overshoot excessivo ou oscilação:"));
  Serial.println(F("    → Reduza Kp em 30-40%"));
  Serial.println(F("    → Aumente Ti em 20%"));
  Serial.println(F("    (detuning pós-Z-N é normal)"));
  Serial.println(F("══════════════════════════════════════════════════\n"));
}

void processarOscilacao() {
  float Pu = calcularPu();
  if (Pu < 0) {
    Serial.println(F("Não foi possível medir Pu — buffer insuficiente."));
    Serial.println(F("Aguarde mais um ciclo completo e tente novamente."));
    return;
  }
  Serial.printf("\n✓ Pu medido automaticamente: %.1f ms\n", Pu);
  calcularZN(Kp, Pu);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogSetAttenuation(ADC_11db);
  analogSetWidth(12);
  pwm_init();
  setPWM(0);

  Serial.println(F("\n╔══════════════════════════════════════════════════╗"));
  Serial.println(F("║  Z-N GANHO ÚLTIMO — MALHA FECHADA                ║"));
  Serial.println(F("╚══════════════════════════════════════════════════╝"));
  Serial.println(F("Comandos:"));
  Serial.println(F("  G  — Ligar controle proporcional (malha fechada)"));
  Serial.println(F("  X  — Desligar controle"));
  Serial.println(F("  +  — Aumentar Kp em um passo"));
  Serial.println(F("  -  — Diminuir Kp em um passo"));
  Serial.println(F("  O  — Oscilação detectada! Medir Pu e calcular Z-N"));
  Serial.println(F("  P  — Definir novo setpoint"));
  Serial.println(F("  I  — Mostrar parâmetros atuais"));
  Serial.println(F("\n  Use o Serial Plotter (Tools > Serial Plotter)"));
  Serial.println(F("  para visualizar a resposta graficamente!"));
  Serial.printf ("  Setpoint inicial: %.1f lux | Kp inicial: %.6f\n\n",
                 setpoint, Kp);
}

bool controle_ativo = false;
unsigned long t_ant = 0;
int saida_pwm = 0;

void loop() {
  // ── Loop de controle (50 Hz) ─────────────────────────────
  if (controle_ativo && millis() - t_ant >= TS_MS) {
    t_ant = millis();

    float lux = lerLux();
    float erro = setpoint - lux;

    // Controle P puro
    saida_pwm = (int)(Kp * erro * PWM_MAX);
    saida_pwm = constrain(saida_pwm, 0, PWM_MAX);
    setPWM(saida_pwm);

    // Salva no buffer circular para detecção de Pu
    buf_lux[buf_idx % BUF_OSC] = lux;
    buf_idx++;

    // Saída para Serial Plotter (formato: chave:valor)
    // Para visualizar: Tools > Serial Plotter
    Serial.printf("Setpoint:%.1f,Lux:%.1f,Duty:%d,Kp:%.6f\n",
                  setpoint, lux, saida_pwm, Kp);
  }

  // ── Processamento de comandos ────────────────────────────
  if (!Serial.available()) return;
  char cmd = Serial.read();
  while (Serial.available()) Serial.read();

  switch (cmd) {
    case 'G': case 'g':
      controle_ativo = true;
      buf_idx = 0;
      Serial.printf("\n[ON] Controle P ativo. Kp=%.6f, SP=%.1f lux\n",
                    Kp, setpoint);
      Serial.println(F("Abra o Serial Plotter para visualizar!"));
      break;

    case 'X': case 'x':
      controle_ativo = false;
      setPWM(0);
      Serial.println(F("[OFF] Controle desligado. LED apagado."));
      break;

    case '+':
      Kp += KP_PASSO;
      Serial.printf("[Kp] Aumentado → Kp = %.6f\n", Kp);
      buf_idx = 0; // zera buffer para nova medição de Pu
      break;

    case '-':
      Kp = max(0.0f, Kp - KP_PASSO);
      Serial.printf("[Kp] Diminuído  → Kp = %.6f\n", Kp);
      buf_idx = 0;
      break;

    case 'O': case 'o':
      Serial.println(F("\n[Pu] Analisando buffer..."));
      processarOscilacao();
      break;

    case 'P': case 'p':
      controle_ativo = false;
      setPWM(0);
      Serial.print(F("Digite o novo setpoint em lux: "));
      while (Serial.available() == 0) delay(50);
      setpoint = Serial.parseFloat();
      while (Serial.available()) Serial.read();
      Serial.printf("\n✓ Setpoint → %.1f lux\n", setpoint);
      break;

    case 'I': case 'i':
      Serial.println(F("\n── Parâmetros atuais ─────────────────────────────"));
      Serial.printf("  Setpoint : %.1f lux\n",  setpoint);
      Serial.printf("  Kp       : %.6f\n",       Kp);
      Serial.printf("  KP_PASSO : %.6f\n",       KP_PASSO);
      Serial.printf("  Ts       : %d ms (%.0f Hz)\n", TS_MS, 1000.0/TS_MS);
      Serial.printf("  Controle : %s\n",          controle_ativo ? "ATIVO" : "inativo");
      Serial.println(F("──────────────────────────────────────────────────"));
      break;

    default:
      if (cmd >= 32)
        Serial.println(F("Comando inválido. Use: G, X, +, -, O, P, I"));
      break;
  }
}
