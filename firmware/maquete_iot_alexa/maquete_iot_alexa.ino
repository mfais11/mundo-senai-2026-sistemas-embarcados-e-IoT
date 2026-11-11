/*
  Maquete IoT
  Firmware do ESP32, versão COM Alexa.

  Placa: DOIT ESP32 DEVKIT V1 (pacote "esp32" da Espressif, versão 2.x ou 3.x)
  Bibliotecas: Espalexa 2.7 ou mais nova (Gerenciador de Bibliotecas da Arduino IDE).

  Alexa: o ESP32 se apresenta como lâmpadas Philips Hue na rede local.
  O Echo e o ESP32 precisam estar na MESMA rede Wi-Fi 2,4 GHz, e o Echo
  precisa de internet. Depois de gravar, diga "Alexa, descobrir dispositivos".
  Dispositivos criados (ver NOMES_ALEXA): luz do quarto, luz do escritório,
  luz da cozinha, luz da garagem, ventilador e varal ("ligar" = recolher).

  A página web fica em pagina.h. Depois de alterar web/index.html, rode:
      python3 tools/gerar_pagina.py

  Rede: preencha WIFI_SSID com a rede onde está o Echo. Se não conseguir
  entrar em 15 s, o ESP32 cria a própria rede AP_SSID (página em
  http://192.168.4.1), mas aí a Alexa não funciona. Na rede local, acesse http://maquete.local
  ou o IP mostrado no monitor serial (115200).
*/
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Espalexa.h>
#include <esp_arduino_version.h>
#include "pagina.h"

// ---------------- Rede ----------------
const char* WIFI_SSID  = "Casa_IoT";
const char* WIFI_SENHA = "12345678";
const char* AP_SSID    = "Maquete-IoT";
const char* AP_SENHA   = "senai2026";     // mínimo de 8 caracteres
const char* NOME_MDNS  = "maquete";       // http://maquete.local

// ---------------- Pinos (ver HARDWARE.md) ----------------
const int PINO_L1 = 25, PINO_L2 = 12, PINO_L3 = 32;   // lâmpadas da casa
const int PINO_LG = 33;                               // luz da garagem (BC337)
const int PINO_VT = 27;                               // ventilador (BC337)
const int PINO_SR = 23;                               // sirene (NPN)
const int PINO_SERVO = 26;                            // varal (SG90)
const int PINO_TRIG = 13, PINO_ECHO = 35;             // HC-SR04 (ECHO com divisor)
const int PINO_CHUVA = 34;                            // DO do módulo de chuva
const int CHUVA_NIVEL = HIGH;                         // nível do DO com chuva (troque para LOW se inverter)
const int PINO_FUMACA = 14;                           // contato NA do relé, BAIXO = fumaça
const int PINO_SOLAR = 36;                            // ADC1, divisor 3:1

// ---------------- Ajustes ----------------
const int   LIM_LUZ  = 30;        // % abaixo do qual o ambiente está escuro
const int   LIM_TEMP = 28;        // °C acima do qual o ventilador liga
const float LIM_DIST = 5.0;       // cm abaixo do qual há carro estacionado
const unsigned long SIRENE_MS = 3000;  // a sirene silencia sozinha depois deste tempo
const float DIVISOR_SOLAR = 3.0;  // tensão do painel = tensão no pino x divisor
const int   SERVO_ANG_ESTENDIDO = 135;   // ângulo do servo com o varal estendido (fora da casa)
const int   SERVO_ANG_RECOLHIDO = 45;    // ângulo do servo com o varal recolhido (dentro da casa)
const int   SERVO_US_0   = 500;          // pulso do servo em 0°
const int   SERVO_US_180 = 2400;         // pulso do servo em 180°
const int   SERVO_CANAL = 0;             // canal LEDC (só usado no pacote 2.x)
const int   LAMPADA_CANAL[3] = { 1, 2, 3 };  // canais LEDC das lâmpadas (pacote 2.x)

// Nomes falados para a Alexa
const char* NOMES_ALEXA[] = { "Luz do quarto", "Luz do escritório", "Luz da cozinha",
                              "Luz da garagem", "Ventilador", "Varal" };

// ---------------- Estado ----------------
// Os nomes seguem o JSON da página (README, "Contrato da API").
struct Estado {
  bool L1, L2, L3, VT, VR, LG, SR;   // cargas (VR: false = estendido, true = recolhido)
  bool CH, FU;                       // sensores digitais
  float US, FV;                      // distância (cm) e tensão do painel (V)
  int luz, temp;                     // ambiente simulado pelos sliders da página
  bool aLuz, aVent, aVaral, aGar, aSir;  // automações
};
Estado S = { false, false, false, false, false, false, false,
             false, false, 99.0, 0.0,
             80, 24,
             true, true, true, true, true };

struct Condicoes { bool escuro, quente, carro, chuva, fumaca; };
Condicoes anterior;

// Filtro contra repique das entradas digitais
struct Filtro { bool estavel, bruto; unsigned long desde; };

// Brilho de cada lâmpada quando acesa (0 a 255). A Alexa pode mudar, a página só liga e desliga.
uint8_t nivel[3] = { 255, 255, 255 };

WebServer server(80);
Espalexa alexa;
bool alexaAtiva = false;
EspalexaDevice* disp[6] = { nullptr };   // mesma ordem de NOMES_ALEXA

// =========================================================
// Saídas
// =========================================================
void servoIniciar() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PINO_SERVO, 50, 16);
#else
  ledcSetup(SERVO_CANAL, 50, 16);
  ledcAttachPin(PINO_SERVO, SERVO_CANAL);
#endif
}

void servoPulso(int us) {
  uint32_t duty = (uint32_t)us * 65535UL / 20000UL;   // período de 20 ms
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PINO_SERVO, duty);
#else
  ledcWrite(SERVO_CANAL, duty);
#endif
}

const int PINOS_LAMPADA[3] = { PINO_L1, PINO_L2, PINO_L3 };

void lampadasIniciar() {
  for (int i = 0; i < 3; i++) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PINOS_LAMPADA[i], 5000, 8);
#else
    ledcSetup(LAMPADA_CANAL[i], 5000, 8);
    ledcAttachPin(PINOS_LAMPADA[i], LAMPADA_CANAL[i]);
#endif
  }
}

void lampadaEscrever(int i, uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PINOS_LAMPADA[i], duty);
#else
  ledcWrite(LAMPADA_CANAL[i], duty);
#endif
}

// Mantém o app da Alexa igual ao estado real (comandos da página e automações).
void sincronizarAlexa() {
  if (!alexaAtiva) return;
  const bool ligado[6] = { S.L1, S.L2, S.L3, S.LG, S.VT, S.VR };
  for (int i = 0; i < 6; i++) {
    uint8_t alvo = ligado[i] ? (i < 3 ? nivel[i] : 255) : 0;
    if (disp[i] && disp[i]->getValue() != alvo) disp[i]->setValue(alvo);
  }
}

void aplicarSaidas() {
  const bool lampada[3] = { S.L1, S.L2, S.L3 };
  for (int i = 0; i < 3; i++) lampadaEscrever(i, lampada[i] ? nivel[i] : 0);
  digitalWrite(PINO_LG, S.LG);
  digitalWrite(PINO_VT, S.VT);
  digitalWrite(PINO_SR, S.SR);

  static int servoAtual = -1;
  int angulo = S.VR ? SERVO_ANG_RECOLHIDO : SERVO_ANG_ESTENDIDO;
  int alvo = map(angulo, 0, 180, SERVO_US_0, SERVO_US_180);
  if (alvo != servoAtual) {
    servoPulso(alvo);
    servoAtual = alvo;
  }
  sincronizarAlexa();
}

// =========================================================
// Automações: reagem à MUDANÇA de condição, então um comando
// manual vale até a condição mudar de novo.
// =========================================================
Condicoes lerCondicoes() {
  return { S.luz < LIM_LUZ, S.temp > LIM_TEMP, S.US < LIM_DIST, S.CH, S.FU };
}

void aplicarAutomacoes() {
  Condicoes c = lerCondicoes();
  if (S.aLuz && c.escuro != anterior.escuro) S.L1 = S.L2 = S.L3 = c.escuro;
  if (S.aVent && c.quente != anterior.quente) S.VT = c.quente;
  if (S.aVaral && c.chuva != anterior.chuva) S.VR = c.chuva;
  if (S.aGar && c.carro != anterior.carro) S.LG = c.carro;
  if (S.aSir && c.fumaca != anterior.fumaca) S.SR = c.fumaca;
  anterior = c;
}

// Toda vez que a sirene liga (automação, página ou botão), ela silencia após SIRENE_MS.
void limitarSirene() {
  static bool antes = false;
  static unsigned long desde = 0;
  if (S.SR && !antes) desde = millis();
  if (S.SR && millis() - desde >= SIRENE_MS) S.SR = false;
  antes = S.SR;
}

// Ao ligar uma automação, a carga acompanha a condição atual.
void sincronizarAutomacao(const String& chave) {
  Condicoes c = lerCondicoes();
  if (chave == "aLuz") S.L1 = S.L2 = S.L3 = c.escuro;
  else if (chave == "aVent") S.VT = c.quente;
  else if (chave == "aVaral") S.VR = c.chuva;
  else if (chave == "aGar") S.LG = c.carro;
  else if (chave == "aSir") S.SR = c.fumaca;
}

// =========================================================
// Sensores
// =========================================================
// Só aceita a nova leitura depois que ela se mantém por 'ms'.
bool filtrar(Filtro& f, bool leitura, unsigned long ms) {
  if (leitura != f.bruto) { f.bruto = leitura; f.desde = millis(); }
  else if (leitura != f.estavel && millis() - f.desde >= ms) f.estavel = leitura;
  return f.estavel;
}

Filtro filtroChuva = { false, false, 0 };
Filtro filtroFumaca = { false, false, 0 };

float medirDistancia() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  unsigned long us = pulseIn(PINO_ECHO, HIGH, 6000);   // até ~1 m
  return us == 0 ? 99.0 : us / 58.0;
}

float mediana3(float a, float b, float c) {
  return max(min(a, b), min(max(a, b), c));
}

void lerSensores() {
  static unsigned long tDist = 0, tSolar = 0;
  static float d[3] = { 99, 99, 99 };
  static int i = 0;

  S.CH = filtrar(filtroChuva, digitalRead(PINO_CHUVA) == CHUVA_NIVEL, 300);
  S.FU = filtrar(filtroFumaca, digitalRead(PINO_FUMACA) == LOW, 200);

  if (millis() - tDist >= 100) {
    tDist = millis();
    d[i] = medirDistancia();
    i = (i + 1) % 3;
    S.US = mediana3(d[0], d[1], d[2]);
  }

  if (millis() - tSolar >= 500) {
    tSolar = millis();
    uint32_t mv = 0;
    for (int k = 0; k < 8; k++) mv += analogReadMilliVolts(PINO_SOLAR);
    S.FV = mv / 8 / 1000.0 * DIVISOR_SOLAR;
  }
}

// =========================================================
// Web
// =========================================================
String estadoJson() {
  char b[420];
  snprintf(b, sizeof b,
    "{\"L1\":%d,\"L2\":%d,\"L3\":%d,\"VT\":%d,\"VR\":%d,\"LG\":%d,\"SR\":%d,"
    "\"CH\":%d,\"FU\":%d,\"US\":%.1f,\"FV\":%.2f,\"luz\":%d,\"temp\":%d,"
    "\"aLuz\":%d,\"aVent\":%d,\"aVaral\":%d,\"aGar\":%d,\"aSir\":%d,"
    "\"lim\":{\"luz\":%d,\"temp\":%d,\"dist\":%.1f,\"sirene\":%lu}}",
    S.L1, S.L2, S.L3, S.VT, S.VR, S.LG, S.SR,
    S.CH, S.FU, S.US, S.FV, S.luz, S.temp,
    S.aLuz, S.aVent, S.aVaral, S.aGar, S.aSir,
    LIM_LUZ, LIM_TEMP, LIM_DIST, SIRENE_MS / 1000);
  return String(b);
}

// Aplica um comando vindo da página. Retorna false se o id não existir.
bool definir(const String& id, float v) {
  bool b = v != 0;
  if      (id == "L1") S.L1 = b;
  else if (id == "L2") S.L2 = b;
  else if (id == "L3") S.L3 = b;
  else if (id == "VT") S.VT = b;
  else if (id == "VR") S.VR = b;
  else if (id == "LG") S.LG = b;
  else if (id == "SR") S.SR = b;
  else if (id == "luz")  S.luz  = constrain((int)v, 0, 100);
  else if (id == "temp") S.temp = constrain((int)v, 15, 40);
  else if (id == "aLuz")   { S.aLuz = b;   if (b) sincronizarAutomacao(id); }
  else if (id == "aVent")  { S.aVent = b;  if (b) sincronizarAutomacao(id); }
  else if (id == "aVaral") { S.aVaral = b; if (b) sincronizarAutomacao(id); }
  else if (id == "aGar")   { S.aGar = b;   if (b) sincronizarAutomacao(id); }
  else if (id == "aSir")   { S.aSir = b;   if (b) sincronizarAutomacao(id); }
  else return false;
  aplicarAutomacoes();
  aplicarSaidas();
  return true;
}

void rotaPagina() {
  server.send_P(200, "text/html; charset=utf-8", PAGINA_HTML, sizeof(PAGINA_HTML) - 1);
}

void rotaEstado() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", estadoJson());
}

void rotaSet() {
  String id = server.arg("id");
  if (!server.hasArg("v") || !definir(id, server.arg("v").toFloat())) {
    server.send(400, "text/plain", "Comando inválido: id=" + id);
    return;
  }
  rotaEstado();
}

void configurarRotas() {
  server.on("/", HTTP_GET, rotaPagina);
  server.on("/api/estado", HTTP_GET, rotaEstado);
  server.on("/api/set", HTTP_POST, rotaSet);
  // Rotas desconhecidas podem ser comandos da Alexa (API do Hue).
  server.onNotFound([]() {
    if (!alexaAtiva || !alexa.handleAlexaApiCall(server.uri(), server.arg(0)))
      server.send(404, "text/plain", "Página não encontrada");
  });
}

// =========================================================
// Alexa
// =========================================================
void alexaComando(int i, uint8_t valor) {
  bool on = valor > 0;
  switch (i) {
    case 0: S.L1 = on; break;
    case 1: S.L2 = on; break;
    case 2: S.L3 = on; break;
    case 3: S.LG = on; break;
    case 4: S.VT = on; break;
    case 5: S.VR = on; break;
  }
  if (i < 3 && on) nivel[i] = valor;
  aplicarSaidas();
}

void configurarAlexa() {
  for (int i = 0; i < 6; i++) {
    EspalexaDeviceType tipo = i < 3 ? EspalexaDeviceType::dimmable : EspalexaDeviceType::onoff;
    alexa.addDevice(NOMES_ALEXA[i], [i](EspalexaDevice* d) { alexaComando(i, d->getValue()); }, tipo, 0);
    disp[i] = alexa.getDevice(i);
  }
  alexa.begin(&server);   // também inicia o servidor web na porta 80
  alexaAtiva = true;
}

// =========================================================
// Wi-Fi
// =========================================================
void conectarWifi() {
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
    Serial.printf("Conectando a %s", WIFI_SSID);
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setSleep(false);
      Serial.printf("Conectado. Abra http://%s ou http://%s.local\n",
                    WiFi.localIP().toString().c_str(), NOME_MDNS);
      return;
    }
    Serial.println("Não foi possível entrar na rede. Criando rede própria.");
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_SENHA);
  Serial.printf("Rede \"%s\" (senha %s). Abra http://%s\n",
                AP_SSID, AP_SENHA, WiFi.softAPIP().toString().c_str());
}

// =========================================================
void setup() {
  Serial.begin(115200);

  const int saidas[] = { PINO_LG, PINO_VT, PINO_SR, PINO_TRIG };
  for (int p : saidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(PINO_ECHO, INPUT);
  pinMode(PINO_CHUVA, INPUT);
  pinMode(PINO_FUMACA, INPUT_PULLUP);
  analogSetPinAttenuation(PINO_SOLAR, ADC_11db);
  servoIniciar();
  lampadasIniciar();

  lerSensores();
  anterior = lerCondicoes();
  aplicarSaidas();

  conectarWifi();
  if (MDNS.begin(NOME_MDNS)) MDNS.addService("http", "tcp", 80);
  configurarRotas();
  if (WiFi.getMode() == WIFI_STA) {
    configurarAlexa();
    sincronizarAlexa();
  } else {
    server.begin();
    Serial.println("Alexa desativada: ela só funciona com o ESP32 conectado à rede do Echo.");
  }
}

void loop() {
  if (alexaAtiva) alexa.loop();   // já chama server.handleClient()
  else server.handleClient();
  lerSensores();
  aplicarAutomacoes();
  limitarSirene();
  aplicarSaidas();
}
