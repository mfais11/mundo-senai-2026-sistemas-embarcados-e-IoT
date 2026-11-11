# Descrição do Hardware — Maquete IoT

Maquete de duas casas e uma garagem que representa um sistema embarcado IoT. A planta é controlada por uma interface web hospedada no próprio ESP32, com sliders, botões e, se for viável, comandos de voz pela Alexa.

> Este documento substitui a descrição original do projeto. As alterações em relação a ela estão marcadas com **🔧 Corrigido** e explicadas na seção [Correções em relação ao projeto original](#correções-em-relação-ao-projeto-original).

---

## 1. Placa de desenvolvimento

| Item | Especificação |
|---|---|
| Placa | ESP32-DEVKIT-V1 (30 pinos) |
| Tensão lógica | 3,3 V (os GPIOs **não** toleram 5 V) |
| Alimentação | 5 V no pino VIN, vindos da fonte de 5 V/2 A |
| Conectividade | Wi-Fi (servidor web embarcado) |

### Restrições de GPIO consideradas

| GPIO | Restrição | Consequência no projeto |
|---|---|---|
| 34, 35, 36 (VP), 39 (VN) | Só entrada, sem pull-up/pull-down interno | Usados apenas para sensores |
| ADC2 (0, 2, 4, 12–15, 25–27) | Não funciona com o Wi-Fi ativo | Leituras analógicas só no ADC1 (32–39) |
| 12 | Strapping (tensão da flash): precisa estar em nível BAIXO no boot | Só recebe carga ligada ao GND (LED), nunca pull-up |
| 0, 2, 5, 15 | Strapping (modo de boot e log) | Evitados |
| 1, 3 | UART0 (USB/Serial) | Evitados |
| 6–11 | Flash interna | Nunca usar |

---

## 2. Alimentação

| Fonte | Tipo | Alimenta |
|---|---|---|
| 5 V / 2 A | Fonte de boa qualidade | ESP32 (VIN), servo SG90, HC-SR04, LEDs da garagem, sirene |
| 12 V / 10 A | Chaveada (colmeia) | Cooler |
| 3,3 V | Regulador do próprio ESP32 (pino 3V3) | Sensor de chuva, pull-ups |

**Terra comum:** os GNDs da fonte de 5 V, da fonte de 12 V e do ESP32 estão interligados. Isso é obrigatório para que os transistores e os sinais tenham a mesma referência.

### Orçamento de corrente da fonte de 5 V

| Carga | Corrente típica | Pico |
|---|---|---|
| ESP32 com Wi-Fi | ~120 mA | ~250 mA |
| Servo SG90 | ~100–250 mA em movimento | ~650 mA (travado) |
| HC-SR04 | ~15 mA | ~15 mA |
| Sirene em 5 V | depende do modelo (medir) | — |
| LEDs da garagem | soma dos ramos (medir) | — |

A soma cabe nos 2 A, mas os **picos do servo podem causar reset por brownout** no ESP32. Por isso é obrigatório usar o capacitor de desacoplamento descrito na seção 5.3.

---

## 3. Nomenclatura (usada na interface web e na Alexa)

| Código | Nome | Tipo | Local | GPIO | Nome na Alexa |
|---|---|---|---|---|---|
| L1 | Lâmpada do quarto | Carga | Casa | 25 | Luz do quarto |
| L2 | Lâmpada do escritório | Carga | Casa | 12 | Luz do escritório |
| L3 | Lâmpada da cozinha | Carga | Casa | 32 | Luz da cozinha |
| VT | Ventilador do quarto | Carga | Sobrado, 1º andar | 27 | Ventilador |
| VR | Varal | Carga | Sobrado, térreo | 26 | Varal ("ligar" = recolher) |
| FV | Painel solar | Sensor | Sobrado, à esquerda do sensor de chuva | 36 (VP) | — |
| CH | Sensor de chuva | Sensor | Sobrado | 34 | — |
| LG | Luz da garagem | Carga | Garagem | 33 | Luz da garagem |
| US | Sensor de distância (HC-SR04) | Sensor | Garagem | 13 (TRIG) / 35 (ECHO) | — |
| SR | Sirene | Carga | Área externa | 23 | — (não exposta por segurança) |
| FU | Detector de fumaça | Sensor | Área externa | 14 | — |

---

## 4. Mapa de pinos (GPIO)

| GPIO | Lado da placa | Direção | Função | Observação |
|---|---|---|---|---|
| 36 (VP) | Esquerdo | Entrada analógica (ADC1_CH0) | Mini painel fotovoltaico | 🔧 era 15 (ADC2) |
| 34 | Esquerdo | Entrada digital | Sensor de chuva (DO) | 🔧 era 25 |
| 35 | Esquerdo | Entrada digital | HC-SR04 — ECHO | 🔧 era 12 |
| 32 | Esquerdo | Saída | LED cômodo 3 | — |
| 33 | Esquerdo | Saída | LEDs da garagem (via BC337) | — |
| 25 | Esquerdo | Saída | LED cômodo 1 | 🔧 era 34 |
| 26 | Esquerdo | Saída PWM (LEDC) | Servo SG90 (varal) | — |
| 27 | Esquerdo | Saída | Cooler 12 V (via BC337) | — |
| 14 | Esquerdo | Entrada digital (pull-up) | Detector de fumaça | — |
| 12 | Esquerdo | Saída | LED cômodo 2 | 🔧 era 35 |
| 13 | Esquerdo | Saída | HC-SR04 — TRIGGER | — |
| 23 | Direito | Saída | Sirene (via NPN) | — |

### Disposição física na protoboard

```
              ┌──────────────┐
        EN ── │              │ ── D23  Sirene
 Painel VP36 ─│              │ ── D22  (livre / alternativa p/ LED2)
       VN39 ──│              │ ── TX0
 Chuva  D34 ──│              │ ── RX0
 Echo   D35 ──│              │ ── D21
 LED3   D32 ──│   ESP32      │ ── D19
 Garag. D33 ──│  DEVKIT V1   │ ── D18
 LED1   D25 ──│              │ ── D5
 Servo  D26 ──│              │ ── TX2
 Cooler D27 ──│              │ ── RX2
 Fumaça D14 ──│              │ ── D4
 LED2   D12 ──│              │ ── D2
 Trig.  D13 ──│              │ ── D15  (liberado)
        GND ──│              │ ── GND
        VIN ──│     USB      │ ── 3V3
              └──────────────┘
```

Os sensores ficam nos pinos "só entrada" do topo, e as saídas ficam nos pinos de baixo. A única ligação que troca de lado da placa é a do painel fotovoltaico.

---

## 5. Cargas (saídas)

### 5.1 LEDs dos cômodos (3×, foscos)

| Item | Valor |
|---|---|
| Pinos | 25 (cômodo 1), 12 (cômodo 2), 32 (cômodo 3) |
| Ligação | Ânodo → resistor → GPIO; cátodos em comum no GND |
| Acionamento | Nível ALTO acende (PWM permite dimerizar pelo slider) |
| Resistor | 120 Ω para LED vermelho/amarelo/verde (≈10 mA); **47–68 Ω** para LED branco/azul (Vf ≈ 3 V) |

> O GPIO 12 é seguro para o LED: a carga ligada ao GND funciona como pull-down e mantém o pino baixo no boot. Se preferir evitar o strapping, use o **GPIO 22**.

### 5.2 LEDs de alto brilho da garagem

| Item | Valor |
|---|---|
| Pino | 33 |
| Driver | Transistor BC337 (NPN), emissor no GND |
| Resistor de base | 1 kΩ |
| Alimentação | 5 V |
| Ligação | LEDs em paralelo, cada ramo com o seu resistor, ligados entre 5 V e o coletor |
| Acionamento | Nível ALTO acende o conjunto |
| Limite | Corrente total < 500 mA (o BC337 suporta 800 mA no máximo) |

### 5.3 Servomotor SG90 (varal automático)

| Item | Valor |
|---|---|
| Pino | 26 (PWM 50 Hz, pulso de 500–2400 µs) |
| Alimentação | 5 V da mesma fonte do ESP32, com GND comum |
| Sinal | 3,3 V do ESP32: compatível, o SG90 reconhece 3,3 V como nível alto |
| Posições | **135°** = varal estendido (fora da casa) · **45°** = varal recolhido (dentro da casa). Ajuste em `SERVO_ANG_ESTENDIDO` / `SERVO_ANG_RECOLHIDO` |
| Desacoplamento | 🔧 **Capacitor eletrolítico de 470–1000 µF / 10 V** + cerâmico de 100 nF entre 5 V e GND, junto ao conector do servo |
| Acionamento | Automático pelo sensor de chuva (fecha ao detectar chuva) ou manual pela interface |

### 5.4 Cooler 12 V (refrigeração)

| Item | Valor |
|---|---|
| Pino | 27 |
| Driver | Transistor BC337 (NPN), emissor no GND, cooler entre +12 V e o coletor |
| Resistor de base | 470 Ω (≈5,5 mA de base, saturação garantida até ~250 mA) |
| Proteção | 🔧 **Diodo 1N4007 em antiparalelo com o cooler** (cátodo no +12 V), obrigatório por ser carga indutiva |
| Acionamento | Nível ALTO liga |
| Limite | Corrente do cooler < 500 mA (verificar na etiqueta) |

### 5.5 Sirene 12 V ativa (alarme de incêndio)

| Item | Valor |
|---|---|
| Pino | 23 |
| Driver | Transistor NPN (BC337), emissor no GND |
| Resistor de base | 1 kΩ |
| Alimentação | 5 V (de propósito, para reduzir o volume) |
| Proteção | Diodo 1N4148/1N4007 em paralelo (recomendado) |
| Acionamento | Nível ALTO liga; disparo automático pelo detector de fumaça |

---

## 6. Sensores (entradas)

### Compatibilidade de nível lógico

| Sensor | Alimentação | Nível do sinal de saída | Compatível com 3,3 V? | Solução |
|---|---|---|---|---|
| HC-SR04 | 5 V | ECHO = 5 V | ❌ | Divisor resistivo no ECHO |
| Sensor de chuva (LM393) | 🔧 3,3 V | DO = 3,3 V | ✅ (com 3,3 V) | Alimentar com 3,3 V em vez de 5 V |
| Detector de fumaça | própria | Contato seco de relé (isolado) | ✅ | COM no GND + pull-up para 3,3 V |
| Mini painel fotovoltaico | — | Analógico (até Voc) | ✅ com divisor | Divisor 3:1 |

### 6.1 Sensor ultrassônico HC-SR04 (entrada/saída do carro)

| Item | Valor |
|---|---|
| Pinos | TRIGGER → 13 · ECHO → 35 🔧 |
| Alimentação | 5 V |
| TRIGGER | Ligação direta (3,3 V é suficiente para o HC-SR04) |
| ECHO | 🔧 **Divisor de tensão:** ECHO → 1 kΩ → GPIO 35 → 2 kΩ → GND (5 V × 2/3 ≈ 3,3 V) |
| Funcionamento | Mede a distância; uma variação abaixo do limiar indica que o carro entrou ou saiu da garagem e aciona os LEDs da garagem |

### 6.2 Sensor de chuva (aciona o varal)

| Item | Valor |
|---|---|
| Pino | 34 (DO) 🔧 |
| Alimentação | 🔧 **3,3 V** (o módulo LM393 funciona de 3,3 a 5 V; assim o DO nunca passa de 3,3 V) |
| Lógica | No módulo usado, DO **ALTO = molhado (chuva)** · BAIXO = seco (constante `CHUVA_NIVEL` no firmware) |
| Ajuste | Sensibilidade pelo trimpot do módulo |
| Pull-up | Já existe no módulo (o GPIO 34 não tem pull-up interno) |

### 6.3 Detector de fumaça Walmonof DFO-LX928N (aciona a sirene)

| Item | Valor |
|---|---|
| Pino | 14 |
| Saída | **Contato seco de relé**, isolado da alimentação do detector |
| Ligação | **COM → GND** do ESP32 · **NA (normalmente aberto) → GPIO 14**. Ao detectar fumaça o relé fecha e leva o pino ao GND |
| Pull-up | `INPUT_PULLUP` no firmware **e** resistor externo de 10 kΩ para 3,3 V (recomendado para fios longos) |
| Lógica | **BAIXO = fumaça detectada** · ALTO = normal |
| Retenção | Mantém a saída acionada após a detecção (o rearme é feito no próprio detector) |
| Alimentação do detector | Conforme a etiqueta do fabricante. Como o contato é isolado, a fonte do detector não interfere no sinal |
| Observação | Se for usado o terminal NF por engano, a lógica se inverte (BAIXO = normal) |

### 6.4 Mini painel fotovoltaico (geração sustentável)

| Item | Valor |
|---|---|
| Pino | 🔧 **36 (VP)**, canal ADC1_CH0 |
| Ligação | Negativo no GND comum; positivo → divisor de tensão 3:1 → GPIO 36 |
| Divisor (exemplo) | 20 kΩ (série) + 10 kΩ (para o GND) → V<sub>pino</sub> = V<sub>painel</sub> / 3 |
| Faixa | Aceita painel com Voc de até ~9 V (9 V / 3 = 3 V) |
| Proteção | Zener de 3,3 V em paralelo com a entrada (opcional; ver nota) |
| Firmware | `analogSetPinAttenuation(36, ADC_11db)` e cálculo `V = leitura × 3,3 / 4095 × 3` |

> **Nota sobre o zener:** um zener de 3,3 V começa a conduzir antes de 3,3 V e distorce a leitura perto do topo da escala. Se o Voc do painel for conhecido e o divisor o mantiver abaixo de 3,3 V, o zener pode ser dispensado. Outra opção é usar um diodo Schottky (BAT54) do pino para o 3V3.

---

## 7. Lista de componentes adicionais (correções)

| Qtd | Componente | Uso |
|---|---|---|
| 1 | Resistor 1 kΩ | Divisor do ECHO (série) |
| 1 | Resistor 2 kΩ (ou 2× 1 kΩ em série) | Divisor do ECHO (para o GND) |
| 1 | Resistor 10 kΩ | Pull-up do detector de fumaça |
| 1 | Capacitor eletrolítico 470–1000 µF / 10 V | Desacoplamento do servo |
| 1 | Capacitor cerâmico 100 nF | Desacoplamento do servo |
| 1–2 | Diodo 1N4007 | Roda-livre do cooler (e da sirene) |
| 3 | Resistor 470 Ω – 1 kΩ | Bases dos transistores (cooler, garagem, sirene) |

---

## Correções em relação ao projeto original

| # | Original | Problema | Correção |
|---|---|---|---|
| 1 | LEDs nos GPIO **34** e **35** | Pinos só de entrada, sem driver de saída: os LEDs não acenderiam | LED1 → **25**, LED2 → **12** |
| 2 | Painel no GPIO **15** | O 15 é ADC2, inutilizável com o Wi-Fi ativo (e a interface web exige Wi-Fi) | Painel → **36 (VP)**, ADC1 |
| 3 | ECHO no GPIO **12** sem divisor | ECHO em 5 V danifica o GPIO; o 12 é strapping | ECHO → **35** com divisor 1 kΩ/2 kΩ |
| 4 | Sensor de chuva no GPIO 25, em 5 V | O DO tem pull-up para o VCC e sairia 5 V | Chuva → **34**, módulo alimentado em **3,3 V** |
| 5 | Cooler sem diodo | Pico de tensão indutivo pode queimar o BC337 | Diodo 1N4007 em antiparalelo |
| 6 | Servo na fonte do ESP32 sem desacoplamento | Picos de corrente causam reset por brownout | Capacitor de 470–1000 µF junto ao servo |
| 7 | Detector de fumaça sem pull-up definido | Contato seco aberto deixa a entrada flutuante | COM no GND, NA no GPIO 14, `INPUT_PULLUP` + 10 kΩ para 3,3 V |
| 8 | Resistor de 120 Ω para LEDs foscos | LED branco/azul teria brilho muito baixo | 47–68 Ω para LED branco/azul |

As trocas de pino foram escolhidas para mexer o mínimo na protoboard: LED1↔Chuva (34↔25) e LED2↔ECHO (35↔12) são trocas entre vizinhos do mesmo lado da placa.
