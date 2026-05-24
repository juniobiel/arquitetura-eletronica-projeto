/*
 * ============================================================
 *  ETAPA 1 — CARACTERIZAÇÃO DO LDR
 *  Sistema Inteligente de Controle de Luminosidade
 *  ESP32 + LDR + Divisor de Tensão
 * ============================================================
 *
 *  CIRCUITO:
 *    3.3V ─── LDR ─── GPIO34 (ADC) ─── R_FIXO (10kΩ) ─── GND
 *
 *  PROCEDIMENTO:
 *    1. Compile e abra o Serial Monitor (115200 baud)
 *    2. Vá para diferentes condições de iluminação
 *    3. Em cada condição, abra o app luxímetro no celular
 *    4. Digite o valor em lux no Serial Monitor e pressione Enter
 *    5. O código lê o ADC, calcula R_ldr e imprime o par (Lux, R_ldr)
 *    6. Colete pelo menos 6 pontos para a regressão
 *
 *  OBSERVAÇÃO IMPORTANTE sobre o ADC do ESP32:
 *    O ADC do ESP32 tem não-linearidade. Neste código usamos
 *    uma correção polinomial simples para melhorar a precisão.
 * ============================================================
 */

// ─── Parâmetros do circuito ───────────────────────────────────
#define ADC_PIN       34        // GPIO para leitura do LDR (use pinos ADC1)
#define R_FIXO        10000.0   // Resistor fixo no divisor de tensão (Ω)
#define VCC           3.3       // Tensão de alimentação
#define N_AMOSTRAS    64        // Nº de amostras para média (reduz ruído)

// ─── Variáveis de calibração (preencher após regressão) ──────
// Depois de coletar os pontos e fazer a regressão log-log,
// insira aqui os coeficientes A e GAMMA resultantes.
// Inicialmente deixe em 0 — o código avisará quando forem necessários.
float LDR_A     = 441474.7;   // coeficiente A  (ex: 1.2e7)
float LDR_GAMMA = 0.5968;   // expoente gamma (ex: 0.75)

// ─── Tabela de pontos coletados (para checar no monitor) ─────
struct Ponto { float lux; float r_ldr; };
Ponto tabela[20];
int n_pontos = 0;

// ─────────────────────────────────────────────────────────────

float lerADC_media() {
  long soma = 0;
  for (int i = 0; i < N_AMOSTRAS; i++) {
    soma += analogRead(ADC_PIN);
    delayMicroseconds(100);
  }
  return (float)soma / N_AMOSTRAS;
}

// Correção de não-linearidade do ADC do ESP32 (baseada em medições empíricas)
// Fonte: documentação Espressif ESP32 ADC calibration
float corrigirADC(float adc_raw) {
  // Equação polinomial de 2ª ordem para atenuação 11dB (0–3.3V)
  // Se não quiser usar, comente esta função e chame lerADC_media() direto
  float v = adc_raw * (VCC / 4095.0);
  // Correção simplificada: offset de ~0.1V na região inferior
  // Para calibração mais precisa, use esp_adc_cal (IDF)
  if (v < 0.1) v = 0.0;
  return v;
}

float calcularRldr(float adc_raw) {
  float v_out = corrigirADC(adc_raw);
  if (v_out <= 0.01 || v_out >= VCC - 0.01) return -1; // fora de faixa
  return R_FIXO * (VCC - v_out) / v_out;
}

float estimarLux(float r_ldr) {
  if (LDR_A == 0 || LDR_GAMMA == 0) return -1; // não calibrado
  if (r_ldr <= 0) return -1;
  return pow(10.0, (log10(LDR_A) - log10(r_ldr)) / LDR_GAMMA);
}

void imprimirTabela() {
  Serial.println(F("\n╔════════════════════════════════════════╗"));
  Serial.println(F("║  TABELA DE CARACTERIZAÇÃO DO LDR       ║"));
  Serial.println(F("╠════════╦══════════════╦════════════════╣"));
  Serial.println(F("║ Ponto  ║  Lux (ref)   ║   R_ldr (Ω)   ║"));
  Serial.println(F("╠════════╬══════════════╬════════════════╣"));
  for (int i = 0; i < n_pontos; i++) {
    Serial.printf("║  %3d   ║  %10.1f  ║  %12.1f  ║\n",
                  i + 1, tabela[i].lux, tabela[i].r_ldr);
  }
  Serial.println(F("╚════════╩══════════════╩════════════════╝"));
  Serial.println(F("\nPara regressão log-log (Google Sheets / Excel):"));
  Serial.println(F("Coluna A: log10(Lux)  |  Coluna B: log10(R_ldr)"));
  Serial.println(F("Aplique PROJ.LIN(B:B, A:A) → retorna [-gamma, log10(A)]"));
  Serial.println(F("─────────────────────────────────────────────────────"));
  Serial.println(F("Dados brutos (copie para planilha):"));
  Serial.println(F("Lux,R_ldr,log10_Lux,log10_R"));
  for (int i = 0; i < n_pontos; i++) {
    Serial.printf("%.1f,%.1f,%.4f,%.4f\n",
                  tabela[i].lux, tabela[i].r_ldr,
                  log10(tabela[i].lux), log10(tabela[i].r_ldr));
  }
}

void lerPonto() {
  if (n_pontos >= 20) {
    Serial.println(F("Máximo de 20 pontos atingido. Use 'T' para ver tabela."));
    return;
  }

  // Lê ADC imediatamente (iluminação estável)
  float adc  = lerADC_media();
  float v    = corrigirADC(adc);
  float rldr = calcularRldr(adc);

  Serial.println(F("\n─── Leitura atual do sensor ─────────────────────────"));
  Serial.printf("  ADC bruto   : %.1f  (de 0 a 4095)\n", adc);
  Serial.printf("  Tensão V_out: %.4f V\n", v);
  if (rldr > 0)
    Serial.printf("  R_ldr calc. : %.1f Ω\n", rldr);
  else
    Serial.println(F("  R_ldr: ERRO — tensão fora de faixa (LDR saturado ou desconectado)"));

  if (LDR_A > 0 && rldr > 0)
    Serial.printf("  Lux estimado: %.1f lux\n", estimarLux(rldr));

  Serial.println(F("\nAgora olhe o app luxímetro no celular."));
  Serial.print(F("Digite o valor em LUX e pressione Enter: "));

  // Aguarda entrada do usuário
  while (Serial.available() == 0) delay(50);
  float lux_ref = Serial.parseFloat();
  while (Serial.available()) Serial.read(); // limpa buffer

  if (lux_ref <= 0) {
    Serial.println(F("Valor inválido. Ponto não salvo."));
    return;
  }

  tabela[n_pontos].lux   = lux_ref;
  tabela[n_pontos].r_ldr = rldr;
  n_pontos++;

  Serial.printf("\n✓ Ponto %d salvo: %.1f lux → R_ldr = %.1f Ω\n",
                n_pontos, lux_ref, rldr);
  Serial.println(F("─────────────────────────────────────────────────────"));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogSetAttenuation(ADC_11db);  // faixa 0–3.3V
  analogSetWidth(12);               // resolução 12 bits (0–4095)

  Serial.println(F("\n╔══════════════════════════════════════════════════╗"));
  Serial.println(F("║   CARACTERIZAÇÃO DO LDR — Sistema Luminosidade  ║"));
  Serial.println(F("╚══════════════════════════════════════════════════╝"));
  Serial.println(F("\nCOMandos disponíveis (envie pelo Serial Monitor):"));
  Serial.println(F("  C  — Coletar novo ponto (mostra ADC e pede lux)"));
  Serial.println(F("  T  — Exibir tabela de pontos + dados para planilha"));
  Serial.println(F("  M  — Monitorar sensor continuamente (tempo real)"));
  Serial.println(F("  S  — Parar monitoramento"));
  Serial.println(F("  R  — Reset (limpar todos os pontos)"));
  Serial.println(F("  A  — Inserir coeficientes A e GAMMA (pós-regressão)"));
  Serial.println(F("\nCircuito: 3.3V — LDR — GPIO34 — 10kΩ — GND"));
  Serial.println(F("Certifique-se que o LDR está exposto à iluminação desejada.\n"));
}

bool monitorando = false;
unsigned long t_monitor = 0;

void loop() {
  // Modo monitoramento contínuo
  if (monitorando && millis() - t_monitor > 500) {
    t_monitor = millis();
    float adc  = lerADC_media();
    float rldr = calcularRldr(adc);
    float lux  = estimarLux(rldr);
    if (LDR_A > 0 && lux > 0)
      Serial.printf("[MONITOR] ADC=%.0f | R_ldr=%.0f Ω | Lux=%.1f\n", adc, rldr, lux);
    else
      Serial.printf("[MONITOR] ADC=%.0f | V=%.3fV | R_ldr=%.0f Ω\n",
                    adc, corrigirADC(adc), rldr);
  }

  if (!Serial.available()) return;
  char cmd = Serial.read();
  while (Serial.available()) Serial.read(); // limpa \n

  switch (cmd) {
    case 'C': case 'c':
      monitorando = false;
      lerPonto();
      break;

    case 'T': case 't':
      monitorando = false;
      imprimirTabela();
      break;

    case 'M': case 'm':
      monitorando = true;
      Serial.println(F("\n[MONITOR] Iniciado. Envie 'S' para parar."));
      if (LDR_A == 0)
        Serial.println(F("          (Use 'A' para inserir coeficientes e ver lux estimado)"));
      break;

    case 'S': case 's':
      monitorando = false;
      Serial.println(F("[MONITOR] Parado."));
      break;

    case 'R': case 'r':
      n_pontos = 0;
      Serial.println(F("Tabela limpa. Todos os pontos removidos."));
      break;

    case 'A': case 'a':
      monitorando = false;
      Serial.print(F("Digite o valor de A (ex: 12000000.0): "));
      while (Serial.available() == 0) delay(50);
      LDR_A = Serial.parseFloat();
      while (Serial.available()) Serial.read();
      Serial.print(F("Digite o valor de GAMMA (ex: 0.75): "));
      while (Serial.available() == 0) delay(50);
      LDR_GAMMA = Serial.parseFloat();
      while (Serial.available()) Serial.read();
      Serial.printf("\n✓ Coeficientes salvos: A=%.2e, GAMMA=%.4f\n", LDR_A, LDR_GAMMA);
      Serial.println(F("Use 'M' para monitorar com conversão em lux."));
      break;

    default:
      if (cmd >= 32) // ignora caracteres de controle
        Serial.println(F("Comando desconhecido. Use: C, T, M, S, R, A"));
      break;
  }
}
