# LuxControl — Sistema Inteligente de Iluminação Adaptativa

> **Projeto Final Integrado**
> **Disciplina:** Eletrônica Digital / Arquitetura de Computadores & IoT (4º Ano — 7º Semestre)
> **Instituição:** Universidade São Francisco (USF)

---

## 🔗 Links do Portfólio

*   **Website Técnico (Landing Page):** [https://arquitetura-eletronica-projeto.vercel.app/](https://arquitetura-eletronica-projeto.vercel.app/)
*   **Dashboard Web IoT (Tempo Real):** [https://arquitetura-eletronica-projeto.vercel.app/dashboard/index.html](https://arquitetura-eletronica-projeto.vercel.app/dashboard/index.html)

---

## 👥 Equipe de Desenvolvimento

*   **Fabricio Evangelista**
*   **Gabriel Junio**
*   **Lucas Ida**
*   **Murilo Minutti**
*   **Pedro Henrique**

---

## 📝 Visão Geral do Projeto

O **LuxControl** é um sistema embarcado de iluminação inteligente e adaptativa baseado em **ESP32**. Ele monitora continuamente a luminosidade ambiental utilizando um sensor LDR calibrado e atua dinamicamente sobre uma fita de LED de 12V via modulação de largura de pulso (**PWM**) e um driver **MOSFET IRLZ44N**.

O núcleo do controle é composto por um algoritmo **PI (Proporcional-Integral) discreto com Anti-Windup** sintonizado empiricamente através do método de **Ziegler-Nichols**. O sistema realiza a telemetria em tempo real das variáveis de controle (Luminosidade real, Setpoint, Duty Cycle do PWM) e de ambiente (Temperatura e Umidade coletadas via **DHT11**) e transmite esses dados no formato **JSON** usando o protocolo **MQTT com TLS (criptografia de porta 8883)** para o broker em nuvem **HiveMQ Cloud**.

Uma interface web interativa (Dashboard) conecta-se de forma assíncrona ao broker via **WebSockets** (usando `MQTT.js` e `Chart.js`) permitindo o monitoramento dinâmico e o controle remoto de setpoint e modos de operação do sistema.

---

## 🛠️ Arquitetura do Sistema e Fluxo de Dados

A arquitetura de IoT do **LuxControl** é dividida em três camadas físicas principais:

```
[ Camada de Aquisição & Atuação ] <───(GPIO/ADC/PWM)───> [ Microcontrolador ESP32 ]
         (LDR, DHT11, LED 12V)                                      │
                                                               (WiFi / MQTT TLS)
                                                                    ▼
[ Interface de Usuário / Web ] <─────(WebSockets)──────> [ HiveMQ Cloud Broker ]
     (Dashboard com Chart.js)
```

### Diagrama de Blocos de Hardware e Comunicação

```
             ┌────────────────────────────────────────────────────────┐
             │                      DISPOSITIVO                       │
             │                                                        │
             │   ┌────────┐  Tensão Divisor   ┌─────────┐             │
             │   │  LDR   ├──────────────────>│ GPIO 34 │             │
             │   └────────┘                   │  (ADC)  │             │
             │                                └────┬────┘             │
             │   ┌────────┐     Dados GPIO    ┌────┴────┐             │
             │   │ DHT11  ├──────────────────>│ GPIO 04 │             │
             │   └────────┘                   │ (Digital)             │
             │                                └────┬────┘             │
             │                                ┌────┴────┐             │
             │                                │  ESP32  │             │
             │                                └────┬────┘             │
             │                                ┌────┴────┐             │
             │   ┌────────┐     Porta PWM     │ GPIO 27 │             │
             │   │ Fita   │<──────────────────┤  (PWM)  │             │
             │   │LED 12V │                   └─────────┘             │
             │   └───┬────┘                                           │
             └───────┼────────────────────────────────────────────────┘
                     │ V_out (Canal de Potência)
                     ▼
             ┌───────────────┐
             │    MOSFET     │
             │   IRLZ44N     │
             └───────────────┘
```

---

## 🔌 Esquema Elétrico e Pinagem do Hardware

Para o correto funcionamento do circuito e mitigação de ruídos na comutação de cargas indutivas/potência, o projeto utiliza um **MOSFET IRLZ44N N-Channel de nível lógico** (acionado diretamente com os 3.3V do ESP32) e um **GND comum** entre a fonte externa de 12V da fita LED e a alimentação de 5V do microcontrolador.

### Tabela de Mapeamento de Pinos

| Componente | Pino do Componente | Pino do ESP32 | Tipo de Sinal | Descrição / Observação |
| :--- | :--- | :--- | :--- | :--- |
| **LDR (Sensor)** | Terminal 1 | `3.3V` | Alimentação | Entrada de tensão do divisor |
| **LDR (Sensor)** | Terminal 2 | `GPIO 34 (ADC1_CH6)`| Analógico Entrada | Ponto de leitura de tensão |
| **Resistor 10kΩ**| Terminal 1 | `GPIO 34` | Analógico Entrada | Resistor fixo do divisor de tensão |
| **Resistor 10kΩ**| Terminal 2 | `GND` | Ground | Resistor de pull-down |
| **DHT11 (Temp/Umi)**| VCC | `3.3V` | Alimentação | Tensão de operação do sensor |
| **DHT11 (Temp/Umi)**| DATA | `GPIO 4` | Digital I/O | Protocolo proprietário (com pull-up 10k) |
| **DHT11 (Temp/Umi)**| GND | `GND` | Ground | Terra do sensor |
| **MOSFET IRLZ44N**| Gate (G) | `GPIO 27` | Digital (PWM) | Saída PWM de controle de brilho via resistor 220Ω |
| **MOSFET IRLZ44N**| Drain (D) | Fita LED (-) | Potência (Saída)| Comutação da corrente de retorno da fita LED |
| **MOSFET IRLZ44N**| Source (S) | `GND` Comum | Ground | Conexão ao GND comum do circuito |
| **Fita LED 12V**  | Terminal (+) | Fonte +12V | Alimentação | Tensão de potência de 12V DC |
| **Fita LED 12V**  | Terminal (-) | MOSFET Drain | Potência (Entrada)| Retorno chaveado pelo MOSFET |
| **Fonte 12V**     | GND | `GND` ESP32 | Ground | **Crucial:** Interligação de terras para referência |

---

## 🗂️ Organização Arquitetural do Repositório

A estrutura física do repositório foi projetada seguindo padrões rígidos de Arquitetura de Software, isolando o código embarcado (**firmware**), a aplicação web (**dashboard**), a mídia de portfólio (**images**) e a documentação acadêmica (**docs**).

```
.
├── docs/                                 # Documentação oficial do projeto
│   └── MINUTA DE PROJETO INTEGRADO.pdf   # Relatório acadêmico detalhado
├── firmware/                             # Códigos fontes embarcados em C++ (Arduino/ESP32)
│   ├── caracterizacao_ldr_luminosidade/  # firmware para coleta de dados de resistência vs lux
│   │   ├── caracterizacao_ldr_luminosidade.ino
│   │   └── CARACTERIZACAO.md             # Tabela de calibração obtida com luxímetro de referência
│   ├── controlador_parametro_pid/        # Protótipo básico de teste do laço PI
│   │   └── controlador_parametro_pid.ino
│   ├── LDR_atuador_v02/                  # Primeira malha fechada direta de controle sem controle PI
│   │   └── LDR_atuador_v02.ino
│   ├── LDR_sistema_completo/             # [FIRMWARE PRINCIPAL] Código final integrado de produção
│   │   └── LDR_sistema_completo.ino      # (Filtro EMA, PI Anti-Windup, WiFi, MQTT TLS e JSON)
│   ├── sintonizacao_parametros_pid_malha_aberta_degrau/ # Firmware de ensaio degrau para modelagem
│   │   └── sintonizacao_parametros_pid_malha_aberta_degrau.ino
│   └── sintonizacao_parametros_pid_malha_fechada/       # firmware para ensaio em oscilação sustentada
│       ├── sintonizacao_parametros_pid_malha_fechada.ino
│       └── ZIEGLER_NICHOLS_MALHA_FECHADA.md # Guia e dedução teórica da sintonia por oscilação
├── dashboard/                            # Aplicação do Dashboard Web IoT
│   ├── app.js                            # Cliente MQTT.js via WebSockets e gráficos em Chart.js
│   ├── index.html                        # Interface gráfica do usuário (UI dark-tech)
│   └── style.css                         # Folha de estilo de alta performance
├── images/                               # Ativos visuais do README e do Portfólio
│   ├── dashboard.png                     # Captura de tela do Dashboard em tempo real
│   └── imagem-inicial.jpg                # Foto real do protótipo físico montado
├── index.html                            # Landing Page técnica principal (Portfólio)
├── script.js                             # Lógica de interatividade e animações do site
├── style.css                             # CSS principal da Landing Page
└── README.md                             # Este manual técnico de arquitetura
```

---

## 📊 Teoria de Controle, Filtragem e Calibração

### 1. Calibração do Sensor LDR
O sensor LDR monitora a tensão $V_{\text{out}}$ resultante de um divisor de tensão pull-down alimentado por $V_{\text{cc}} = 3.3\text{V}$ com resistor de referência $R_{\text{FIXO}} = 10\text{k}\Omega$.
A resistência instantânea do LDR ($R_{\text{LDR}}$) é determinada por:

$$R_{\text{LDR}} = R_{\text{FIXO}} \cdot \left(\frac{4095}{\text{ADC}} - 1\right)$$

Para converter a resistência do LDR em luminosidade real em **Lux**, foi executada uma regressão logarítmica (log-log) a partir de 8 pontos de medição comparados com um luxímetro digital de bancada. A curva característica obedece à equação de potência exponencial:

$$\text{Lux} = A \cdot (R_{\text{LDR}})^{-\gamma}$$

No firmware de produção (`LDR_sistema_completo.ino`), os parâmetros calibrados adotados são:
*   **$A$:** $320000.0$
*   **$\gamma$ (Gamma):** $0.90$

---

### 2. Filtro Digital EMA (Média Móvel Exponencial)
O conversor analógico-digital (ADC) do ESP32 é suscetível a ruído de alta frequência induzido pela comutação PWM do MOSFET e ruído térmico. Para estabilizar o sinal sem introduzir atraso (delay de fase) prejudicial à malha fechada, foi implementado um **filtro recursivo EMA (Exponential Moving Average)** de primeira ordem:

$$y_{\text{fil}}(k) = \alpha \cdot x(k) + (1 - \alpha) \cdot y_{\text{fil}}(k-1)$$

No firmware final, adotamos um fator de suavização **$\alpha = 0.1$** (ou `EMA_ALPHA = 0.1`), que atua como um filtro passa-baixas digital de resposta impulsiva infinita (IIR), suavizando a variável analógica antes da entrada no algoritmo de controle PI.

---

### 3. Algoritmo do Controlador PI Discreto com Anti-Windup
O erro instantâneo é calculado comparando a luminosidade de referência definida pelo usuário (Setpoint) com a luminosidade real estimada pelo LDR filtrado:

$$e(k) = \text{Setpoint}(k) - \text{Lux}_{\text{Real}}(k)$$

A saída de controle é composta pela soma das ações **Proporcional ($P$)** e **Integral ($I$)**:

$$u(k) = P(k) + I(k)$$

$$P(k) = K_p \cdot e(k)$$

$$I(k) = I(k-1) + K_i \cdot e(k) \cdot \Delta t$$

#### Solução ao Efeito Windup (Saturação Integral)
Caso o atuador sature fisicamente (quando o PWM atinge o limite máximo de $255$ ou mínimo de $0$), o termo integral continuaria acumulando erro se não houvesse proteção, causando oscilações violentas e tempo de recuperação demorado ao retornar à faixa linear. Para evitar isso, implementou-se o recurso de **Anti-Windup**, limitando a faixa de acúmulo da variável integral com a função `constrain`:

$$\text{Acúmulo}_{\text{erro}} = \text{constrain}\left(\text{Acúmulo}_{\text{erro}}, -\frac{255.0}{K_i}, \frac{255.0}{K_i}\right)$$

A saída física final de comutação PWM atua na resolução de 8 bits ($0$ a $255$):

$$\text{Saída PWM} = \text{constrain}(P(k) + I(k), 0, 255)$$

---

### 4. Sintonia de Parâmetros (Ziegler-Nichols)
Utilizando o ensaio de **Ziegler-Nichols em Malha Fechada**, elevou-se o ganho puramente proporcional até que o sistema atingisse um regime de oscilação contínua e sustentada de luminosidade, identificando as variáveis críticas de controle:
*   **Ganho Crítico ($K_{\text{cr}}$):** $7.0$
*   **Período Crítico ($P_{\text{cr}}$):** $0.37$ segundos

Aplicando a regra clássica de sintonização de Ziegler-Nichols para controladores do tipo PI:

| Parâmetro PI | Fórmula | Valor Calculado e Aplicado |
| :--- | :--- | :--- |
| **Ganho Proporcional ($K_p$)** | $0.45 \cdot K_{\text{cr}}$ | **$3.15$** |
| **Ganho Integral ($K_i$)** | $1.2 \cdot \frac{K_p}{P_{\text{cr}}}$ | **$18.9$** |
| **Tempo Integral ($T_i$)** | $\frac{P_{\text{cr}}}{1.2}$ | **$0.308$ s** |
| **Ganho Derivativo ($K_d$)** | — | **$0.00$** |

Este ajuste forneceu uma resposta rápida, estável e com **erro em regime permanente inferior a 10 Lux**, com um tempo de acomodação de $3$ a $5$ segundos.

---

## 📡 Comunicação IoT, Segurança e Telemetria (MQTT)

A camada de conectividade transmite e recebe pacotes assíncronos via **MQTT** (Message Queuing Telemetry Transport) criptografado com **TLS/SSL (porta 8883)**.

*   **Host do Broker (HiveMQ Cloud):** `377271ae85c448099dc71d8bd61e92c6.s1.eu.hivemq.cloud`
*   **Tópico de Envio (ESP32 ──> Dashboard):** `luminosidade/dados`
*   **Tópico de Recepção (Dashboard ──> ESP32):** `luminosidade/controle`

### 1. Payload de Telemetria (JSON)
Publicado pelo microcontrolador a cada **$1.5$ segundos**:

```json
{
  "lux": 312.5,
  "pwm": 67,
  "setpoint": 300,
  "temp": 24.3,
  "umid": 58,
  "modo": "auto"
}
```

*   `lux`: Valor real filtrado de luminosidade ambiente (Lux).
*   `pwm`: Percentual instantâneo de brilho da fita de LED ($0$ a $100\%$).
*   `setpoint`: Alvo de luminosidade configurado na malha fechada (Lux).
*   `temp`: Leitura instantânea de temperatura (°C).
*   `umid`: Leitura instantânea de umidade relativa do ar (%).
*   `modo`: Modo de operação ativa do sistema (`"auto"` | `"manual"` | `"off"`).

### 2. Comandos de Controle (JSON)
Enviados pela interface web do dashboard para comandar o microcontrolador:

*   **Alterar Setpoint (Modo Auto):**
    ```json
    {"comando": "setpoint", "valor": 450}
    ```
*   **Alterar Modo de Operação:**
    ```json
    {"comando": "modo", "valor": "auto"} // opções: "auto", "manual", "off"
    ```
*   **Alterar Duty Cycle Fixo (Modo Manual):**
    ```json
    {"comando": "duty", "valor": 75}
    ```

---

## 🖥️ Dashboard Web e Simulador Local

O Dashboard é uma aplicação web autônoma (Single Page Application) responsiva e otimizada que apresenta um layout *dark-tech* moderno. Ele implementa:

1.  **Gráficos dinâmicos de alta performance:** Utiliza `Chart.js` via WebSockets para exibir curvas históricas de Lux, Setpoint e PWM em tempo real.
2.  **Duplo Modo (Simulação e Produção):** Se o ESP32 não estiver ativo, o dashboard entra automaticamente em **Modo Simulação**, gerando sinais matemáticos simulados baseados na perturbação ambiental e resposta dinâmica de malha fechada. Isso permite demonstrar o projeto em portfolios mesmo sem o hardware conectado!
3.  **Controle Direto:** Controles deslizantes deslizantes de setpoint, entradas numéricas de duty cycle, seletores de modo com respostas instantâneas, logs de evento em tempo real e relógio com sincronização por satélite de telemetria.

---

## 🚀 Como Compilar e Executar o Projeto

### Requisitos de Desenvolvimento
1.  **Arduino IDE** (Versão 2.x recomendada) ou **VS Code com PlatformIO**.
2.  Placa de Desenvolvimento instalada no gerenciador de placas da IDE: **ESP32 Dev Module** (Espressif).
3.  Bibliotecas necessárias no Arduino Library Manager:
    *   `PubSubClient` (por Nick O'Leary)
    *   `ArduinoJson` (por Benoit Blanchon)
    *   `DHT sensor library` (por Adafruit)
    *   `Adafruit Unified Sensor` (por Adafruit)

### Configuração e Gravação do Firmware
1.  Navegue até a pasta `firmware/LDR_sistema_completo/` e abra o arquivo `LDR_sistema_completo.ino`.
2.  Altere as credenciais de WiFi nas linhas **33** e **34** com o SSID e senha de sua rede local:
    ```cpp
    const char* WIFI_SSID = "SEU_WIFI_NOME";
    const char* WIFI_PASS = "SUA_WIFI_SENHA";
    ```
3.  *(Opcional)* Modifique as credenciais do HiveMQ Cloud caso queira usar sua própria instância de teste. Por padrão, o código já vem configurado com as chaves TLS seguras do laboratório.
4.  Conecte o ESP32 no computador via cabo Micro-USB.
5.  Selecione a placa correspondente ("ESP32 Dev Module") e a porta COM.
6.  Clique em **Upload** (Seta para a direita) e aguarde o upload concluir.
7.  Abra o **Serial Monitor** configurado em **115200 baud** para acompanhar o estado do sistema, a conexão à rede WiFi e o recebimento das credenciais MQTT.

### Executando o Dashboard IoT
*   **Execução Local:**
    Basta abrir o arquivo `dashboard/index.html` em qualquer navegador web moderno. Caso queira rodar localmente com conexão real ao hardware, garanta que o navegador tem permissão para conexão WebSocket externa segura (WSS). Caso contrário, ele rodará em **Modo Simulação** interativa demonstrando todo o funcionamento do PID na tela!

---

## 📹 Apresentação Prática de Bancada

Para assistir aos testes reais na bancada de eletrônica digital e ver o funcionamento físico do sistema inteligente atuando sobre a iluminação do protótipo, acesse os vídeos abaixo:

*   **Vídeo 1: Explicação Teórica do Projeto** — [Assistir no Google Photos](https://photos.google.com/share/AF1QipNq76PqlL1KxNYcgA2VzBX4FR_Gyg_JeIIpQaHWe2z7vMnEcqucGoFxC-ZA0QT5og/photo/AF1QipPaDcCgNHeIVancyyA7GQp3oDrQxeNnFo4n5Tuw?key=NURtSTFRZGh1SWhsSGpyYkpUcW1tY3hvazBGcndR)
*   **Vídeo 2: Demonstração e Dashboard em Ação** — [Assistir no Google Photos](https://photos.google.com/share/AF1QipNq76PqlL1KxNYcgA2VzBX4FR_Gyg_JeIIpQaHWe2z7vMnEcqucGoFxC-ZA0QT5og/photo/AF1QipMIsOPd6AKDjxm4bGgSA8zamBBYLXGdLbtwP7fk?key=NURtSTFRZGh1SWhsSGpyYkpUcW1tY3hvazBGcndR)
*   **Vídeo 3: Visão de Bancada do Circuito Físico** — [Assistir no Google Photos](https://photos.google.com/share/AF1QipNq76PqlL1KxNYcgA2VzBX4FR_Gyg_JeIIpQaHWe2z7vMnEcqucGoFxC-ZA0QT5og/photo/AF1QipOknPB85WNH1_Cq6CD_ldc_NgcSHj3q2v4BatOY?key=NURtSTFRZGh1SWhsSGpyYkpUcW1tY3hvazBGcndR)

---

## 📜 Licença

Este projeto está licenciado sob a licença **MIT** — sinta-se livre para clonar, estudar, otimizar e utilizar o projeto em trabalhos acadêmicos ou comerciais.
