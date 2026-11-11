# Maquete IoT

Maquete de uma **casa**, um **sobrado** e uma **garagem** que representa um sistema embarcado IoT. Um **ESP32** controla iluminação, ventilação, varal automático, alarme de incêndio e garagem. Ele hospeda uma página web, pensada para computador ou lousa digital, e aceita comandos de voz pela **Alexa**.

A nomenclatura (códigos L1, VT, CH…), os pinos e a descrição completa do hardware estão em **[HARDWARE.md](HARDWARE.md)**.

## Estrutura do repositório

```
.
├── HARDWARE.md                      # descrição completa do hardware
├── web/index.html                   # página (fonte; arquivo único, sem dependências externas)
├── tools/gerar_pagina.py            # copia a página para dentro dos firmwares (pagina.h)
└── firmware/
    ├── maquete_iot/                 # firmware SEM Alexa
    │   ├── maquete_iot.ino
    │   └── pagina.h                 # gerado, não editar
    └── maquete_iot_alexa/           # firmware COM Alexa (Espalexa)
        ├── maquete_iot_alexa.ino
        └── pagina.h                 # gerado, não editar
```

## Firmware

| | `maquete_iot` | `maquete_iot_alexa` |
|---|---|---|
| Bibliotecas extras | nenhuma | **Espalexa** 2.7+ |
| Rede | entra no Wi-Fi configurado; se falhar, cria a rede `Maquete-IoT` | precisa entrar no **mesmo Wi-Fi do Echo**; se falhar, cria a rede, mas sem Alexa |
| Lâmpadas da casa | liga e desliga | liga, desliga e ajusta o brilho (PWM) pela Alexa |

**Para gravar:**
1. Na Arduino IDE, instale o pacote de placas **esp32** (Espressif, 2.x ou 3.x). Para a versão com Alexa, instale também a biblioteca **Espalexa** pelo Gerenciador de Bibliotecas.
2. Abra a pasta do firmware escolhido e preencha `WIFI_SSID` e `WIFI_SENHA` no topo do `.ino`.
3. Selecione a placa **DOIT ESP32 DEVKIT V1** e grave.
4. No monitor serial (115200), veja o endereço da página. Na mesma rede, `http://maquete.local` também funciona. Se o ESP32 criou a própria rede, conecte-se a `Maquete-IoT` (senha `senai2026`) e abra `http://192.168.4.1`.

Ambos foram compilados com o pacote esp32 3.3.12 (cerca de 80% da flash), mas **ainda não foram testados na placa**.

**Ajustes** (constantes no topo do `.ino`):
- **Limiares:** `LIM_LUZ`, `LIM_TEMP`, `LIM_DIST`. A página lê esses valores do ESP32.
- **Servo:** `SERVO_ANG_ESTENDIDO` / `SERVO_ANG_RECOLHIDO`, em graus. Calibre o varal aqui.
- **Painel solar:** `DIVISOR_SOLAR`, a relação do divisor resistivo.

**Alterou a página?** Rode `python3 tools/gerar_pagina.py` para atualizar o `pagina.h` dos dois firmwares e grave de novo.

## Alexa

O firmware `maquete_iot_alexa` usa a biblioteca Espalexa: o ESP32 se apresenta como lâmpadas Philips Hue na rede local. Não precisa de conta nem de skill.

| Dispositivo na Alexa | Controla | Exemplos de comando |
|---|---|---|
| Luz do quarto | L1 | "Alexa, acender a luz do quarto", "Alexa, luz do quarto em 30%" |
| Luz do escritório | L2 | "Alexa, apagar a luz do escritório" |
| Luz da cozinha | L3 | "Alexa, diminuir a luz da cozinha" |
| Luz da garagem | LG | "Alexa, acender a luz da garagem" |
| Ventilador | VT | "Alexa, ligar o ventilador" |
| Varal | VR | "Alexa, ligar o varal" recolhe; "desligar" estende |

**Configuração:**
1. Grave o firmware com o Wi-Fi da rede onde está o Echo (2,4 GHz).
2. No app Alexa, peça "Alexa, descobrir dispositivos" (ou use *Dispositivos › + › Adicionar dispositivo › Outro*). Devem aparecer 6 dispositivos.
3. Para o varal soar natural, crie no app uma **rotina** com a frase "recolher o varal" que liga o dispositivo Varal, e outra com "estender o varal" que o desliga.
4. Crie um grupo "Casa" com as três luzes para dizer "Alexa, apagar a casa".

**Na apresentação:**
- Redes públicas ou corporativas costumam ter portal cativo, WPA2-Enterprise ou isolamento entre clientes, e a Alexa não funciona nelas. **Leve um roteador próprio ou use um hotspot de celular com internet** (o Echo precisa de internet para entender a voz) e faça a descoberta já nessa rede.
- A sirene **não** é exposta à Alexa, para evitar disparos acidentais.
- O ambiente é barulhento: mantenha a página como controle principal e use a voz como demonstração.

## Página web

A página é um único arquivo HTML (~46 KB), sem CDN nem fontes externas, então funciona com o ESP32 fora da internet. Visualmente, é uma prancha de desenho técnico.
- **Planta geral:** a casa, o sobrado e a garagem desenhados em planta, com um carimbo à direita mostrando conexão, hora e alarme de incêndio.
- **Detalhe:** tocar em uma construção abre o detalhe dela em tela cheia. O botão **Planta geral** (ou Esc) volta.

| Detalhe | O que mostra |
|---|---|
| A, Casa | Lâmpadas L1–L3 com botões e uma régua de luz do ambiente. Abaixo de 30%, tudo acende. |
| B, Sobrado | Corte lateral com os dois pavimentos: sensor de chuva, varal (Estender/Recolher), ventilador com régua de temperatura (liga acima de 28 °C) e painel solar. |
| C, Garagem | Distância do HC-SR04 com régua; abaixo de 5 cm, o carro é dado como estacionado e a luz acende. |

**Modo simulação:** aberta direto do computador (`file://`) ou com `?demo` na URL, a página simula o ESP32. O botão **Simular sensores** permite ensaiar a apresentação sem hardware.

### Regras de automação

As regras reagem à **mudança** de condição: depois de um comando manual (pela página ou pela Alexa), a carga fica como foi deixada até a condição mudar de novo. Ao ligar uma automação, a carga é sincronizada com a condição atual.

| Automação | Chave | Regra |
|---|---|---|
| Luz automática | `aLuz` | `luz` cruza 30% → L1–L3 acendem (escuro) ou apagam (claro) |
| Ventilação automática | `aVent` | `temp` cruza 28 °C → VT liga ou desliga |
| Varal automático | `aVaral` | começa a chover → VR recolhe; seca → VR estende |
| Luz da garagem | `aGar` | US cruza 5 cm → LG acende (carro chegou) ou apaga (saiu) |
| Sirene | `aSir` | fumaça detectada → SR toca. Em qualquer acionamento, a sirene silencia sozinha após 3 s (`SIRENE_MS`) |

### API do ESP32

| Método | Rota | Corpo | Resposta |
|---|---|---|---|
| GET | `/` | — | página |
| GET | `/api/estado` | — | JSON de estado |
| POST | `/api/set` | `id=<chave>&v=<valor>` | JSON de estado atualizado |

```json
{
  "L1": 0, "L2": 0, "L3": 0, "VT": 0, "VR": 0, "LG": 0, "SR": 0,
  "CH": 0, "FU": 0, "US": 35.0, "FV": 4.60,
  "luz": 80, "temp": 24,
  "aLuz": 1, "aVent": 1, "aVaral": 1, "aGar": 1, "aSir": 1,
  "lim": { "luz": 30, "temp": 28, "dist": 5.0, "sirene": 3 }
}
```

- `VR`: 0 = estendido (servo em 135°), 1 = recolhido (servo em 45°).
- `luz` e `temp` são valores simulados pelas réguas da página, já que não há LDR nem sensor de temperatura.

## Avisos de segurança elétrica

- Os GPIOs do ESP32 funcionam em **3,3 V** e não toleram 5 V. Veja as soluções de nível lógico em [HARDWARE.md](HARDWARE.md#6-sensores-entradas).
- Os GNDs das fontes de 5 V e 12 V **precisam** estar interligados com o GND do ESP32.
- Não alimente o ESP32 pela USB e pelo VIN de 5 V ao mesmo tempo sem proteção.
