/*
 * ============================================================
 *  ETAPA 2 — ENSAIO DE DEGRAU (MALHA ABERTA)
 *  Ziegler-Nichols — Método da Curva de Reação
 * ============================================================
 *
 *  CIRCUITO:
 *    ESP32 GPIO18 (PWM) → [330Ω] → Gate IRLZ44N
 *    VCC (12V) → R_LED → LED → Drain IRLZ44N → GND
 *    3.3V → LDR → GPIO34 (ADC) → 10kΩ → GND
 *    LDR posicionado apontando para o LED, distância fixa!
 *
 *  O QUE ESTE CÓDIGO FAZ:
 *    1. Mantém o LED apagado por 3s (regime estacionário inicial)
 *    2. Aplica um degrau de 0% → STEP_DUTY (50%)
 *    3. Registra lux vs tempo a 50 Hz por DURACAO_MS milissegundos
 *    4. Imprime os dados para você copiar para planilha
 *
 *  COMO EXTRAIR L, T e K DA PLANILHA:
 *    Veja o guia de laboratório para o procedimento gráfico detalhado.
 * ============================================================
 */

#include "driver/ledc.h"

// ─── Pinos ──────────────────────────────────────────────────
#define PWM_PIN       18
#define ADC_PIN       34

// ─── Configuração PWM ────────────────────────────────────────
#define PWM_FREQ      5000
#define PWM_RES       LEDC_TIMER_10_BIT   // 0–1023
#define PWM_MAX       1023

// ─── Parâmetros do ensaio ────────────────────────────────────
#define STEP_DUTY     512       // degrau aplicado (0–1023). 512 = 50%
                                // ⚠ Ajuste para garantir que o LED responda
                                //   sem saturar o LDR (rldr > 0)
#define DURACAO_MS    5000      // duração de coleta após o degrau (ms)
#define FS_HZ         50        // frequência de amostragem (50 Hz = 20ms)
#define N_MAX         (DURACAO_MS * FS_HZ / 1000 + 50)

// ─── LDR — parâmetros de calibração ─────────────────────────
//  Preencha com os valores obtidos na Etapa 1 (regressão log-log)
#define R_FIXO        10000.0
#define VCC_LDR       3.3
#define LDR_A         1.2e7     // ← substitua pelo seu valor
#define LDR_GAMMA     0.75      // ← substitua pelo seu valor

// ─── Buffer de gravação ──────────────────────────────────────
float buf_t[N_MAX];
float buf_lux[N_MAX];
int   n_buf = 0;

// ─────────────────────────────────────────────────────────────

void pwm_init() {
  ledc_timer_config_t tmr = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .duty_resolution  = PWM_RES,
    .timer_num        = LEDC_TIMER_0,
    .freq_hz          = PWM_FREQ,
    .clk_cfg          = LEDC_AUTO_CLK
  };
  ledc_timer_config(&tmr);

  ledc_channel_config_t ch = {
    .gpio_num   = PWM_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .duty       = 0,
    .hpoint     = 0
  };
  ledc_channel_config(&ch);
}

void setPWM(int duty) {
  duty = constrain(duty, 0, PWM_MAX);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

float lerLux() {
  // Média de 16 amostras ADC
  long soma = 0;
  for (int i = 0; i < 16; i++) {
    soma += analogRead(ADC_PIN);
    delayMicroseconds(50);
  }
  float adc   = soma / 16.0;
  float v_out = adc * (VCC_LDR / 4095.0);
  if (v_out <= 0.01 || v_out >= VCC_LDR - 0.01) return -1;
  float r_ldr = R_FIXO * (VCC_LDR - v_out) / v_out;
  return pow(10.0, (log10(LDR_A) - log10(r_ldr)) / LDR_GAMMA);
}

void ensaioDegrau() {
  n_buf = 0;

  Serial.println(F("\n══════════════════════════════════════════════"));
  Serial.println(F("  ENSAIO DE DEGRAU — MALHA ABERTA (Z-N)"));
  Serial.println(F("══════════════════════════════════════════════"));
  Serial.printf ("  Degrau aplicado: duty %d / %d (%.1f%%)\n",
                 STEP_DUTY, PWM_MAX, 100.0 * STEP_DUTY / PWM_MAX);
  Serial.printf ("  Duração de coleta: %d ms a %d Hz\n", DURACAO_MS, FS_HZ);
  Serial.println(F("  Mantenha o LED e o LDR imóveis durante o ensaio!"));
  Serial.println(F("──────────────────────────────────────────────"));

  // Fase 1: estabilizar em 0
  Serial.print(F("  [1/3] Estabilizando em duty=0"));
  setPWM(0);
  for (int i = 0; i < 6; i++) { delay(500); Serial.print('.'); }
  Serial.println(F(" OK"));

  // Leitura de regime inicial
  float lux0 = lerLux();
  Serial.printf("  [1/3] Lux inicial (base): %.1f lux\n", lux0);

  // Fase 2: aplicar degrau e coletar
  Serial.println(F("  [2/3] Aplicando degrau e coletando..."));
  uint32_t dt_us = 1000000UL / FS_HZ;
  uint32_t t0    = micros();

  setPWM(STEP_DUTY);   // ←─ DEGRAU APLICADO AQUI

  while (micros() - t0 < (uint32_t)DURACAO_MS * 1000UL && n_buf < N_MAX) {
    uint32_t t_agora = micros() - t0;
    float lux = lerLux();
    buf_t[n_buf]   = t_agora / 1000.0;  // ms
    buf_lux[n_buf] = lux;
    n_buf++;

    // Aguarda próxima amostra
    while (micros() - t0 < (uint32_t)n_buf * dt_us) { /* busy-wait */ }
  }

  // Fase 3: desligar
  setPWM(0);
  Serial.printf("  [3/3] Coleta concluída — %d amostras\n\n", n_buf);

  // ── Impressão dos dados ─────────────────────────────────────
  float lux_final = buf_lux[n_buf - 1];
  float delta_lux = lux_final - lux0;
  float delta_duty_pct = 100.0 * STEP_DUTY / PWM_MAX;
  float K = delta_lux / delta_duty_pct;

  Serial.println(F("══════════════════════════════════════════════"));
  Serial.println(F("  ESTATÍSTICAS DO ENSAIO"));
  Serial.println(F("──────────────────────────────────────────────"));
  Serial.printf ("  Lux inicial  (y0)     : %.1f lux\n",   lux0);
  Serial.printf ("  Lux final    (y_inf)  : %.1f lux\n",   lux_final);
  Serial.printf ("  Δy (variação total)   : %.1f lux\n",   delta_lux);
  Serial.printf ("  Δu (degrau duty)      : %.1f %%\n",    delta_duty_pct);
  Serial.printf ("  K = Δy/Δu (ganho est.): %.4f lux/%%\n", K);
  Serial.println(F("──────────────────────────────────────────────"));
  Serial.println(F("  Agora trace a tangente na curva S para obter L e T."));
  Serial.println(F("  Use a calculadora Z-N no chat para os parâmetros PID."));
  Serial.println(F("══════════════════════════════════════════════\n"));

  // ── CSV para cópia ──────────────────────────────────────────
  Serial.println(F("── DADOS CSV (copie para planilha) ────────────"));
  Serial.println(F("t_ms,lux"));
  for (int i = 0; i < n_buf; i++) {
    Serial.printf("%.1f,%.2f\n", buf_t[i], buf_lux[i]);
  }
  Serial.println(F("── FIM DOS DADOS ───────────────────────────────"));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogSetAttenuation(ADC_11db);
  analogSetWidth(12);
  pwm_init();
  setPWM(0);

  Serial.println(F("\n╔══════════════════════════════════════════════════╗"));
  Serial.println(F("║  ENSAIO DE DEGRAU — Z-N MALHA ABERTA             ║"));
  Serial.println(F("╚══════════════════════════════════════════════════╝"));
  Serial.println(F("Comandos:"));
  Serial.println(F("  D  — Executar ensaio de degrau"));
  Serial.println(F("  M  — Monitorar lux (tempo real, para checar sensor)"));
  Serial.println(F("  S  — Parar monitoramento"));
  Serial.println(F("\n⚠  Antes de começar:"));
  Serial.println(F("   • Fixe o LDR apontando diretamente para o LED"));
  Serial.println(F("   • Distância fixa (~10 cm) durante TODOS os ensaios"));
  Serial.println(F("   • Ambiente com iluminação externa estável (sem janela perto)"));
  Serial.println(F("   • Verifique que LDR_A e LDR_GAMMA estão preenchidos no código\n"));
}

bool monitorando = false;
unsigned long t_mon = 0;

void loop() {
  if (monitorando && millis() - t_mon > 200) {
    t_mon = millis();
    float lux = lerLux();
    Serial.printf("[MONITOR] %.1f lux\n", lux);
  }

  if (!Serial.available()) return;
  char cmd = Serial.read();
  while (Serial.available()) Serial.read();

  switch (cmd) {
    case 'D': case 'd':
      monitorando = false;
      ensaioDegrau();
      break;
    case 'M': case 'm':
      monitorando = true;
      Serial.println(F("[MONITOR] Iniciado. Envie 'S' para parar."));
      break;
    case 'S': case 's':
      monitorando = false;
      Serial.println(F("[MONITOR] Parado."));
      break;
    default:
      if (cmd >= 32) Serial.println(F("Comando inválido. Use: D, M, S"));
      break;
  }
}
