#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "KY038.h"
#include "DebugManager.h"
#include "WiFiManager.h"
#include "MqttManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//==========================================
//
//      Declaração de constantes globais
//
//==========================================

#define PIN_DHT 8
#define TIPO_DHT DHT22
#define PIN_MIC 9
#define intervalo 3000
#define SOUND_LIMIT 70

//==========================================
//
//      Declaração de variaveis globais
//
//==========================================

float temperatura = 0.0;
float umidade = 0.0;
float ruido = 0.0;

float temperaturaOposto = 0.0;
float umidadeOposto = 0.0;
float ruidoOposto = 0.0;

int comandoAr = 0;
int alertaSom = 0;
bool eco = false;

bool syncRealizado = false;

static float PastPublishedTemperatura = 0.0;
static float PastPublishedUmidade = 0.0;
static float PastPublishedRuido = 0.0;

static int PastPublishedComandoAr = 0;
static int PastPublishedAlertaSom = 0;
static bool PastPublishedEco = false;

unsigned long ultimaPublicacao = 0;

unsigned long inicioRuidoA = 0;
unsigned long inicioRuidoB = 0;
bool ativoA = false;
bool ativoB = false;
const unsigned long duracaoRuido = 300;

unsigned long inicioSilencioA = 0;
unsigned long inicioSilencioB = 0;
bool silencioA = false;
bool silencioB = false;

const unsigned long duracaoEco = 900000;

bool mensagemRecebidaOposto = false;

//==========================================
//
//     Prototipação de funções
//
//==========================================

bool SensorUmidadeTemperatura();
void configurarSensor();
void diferencaTemp();
void alertaSomEco();
void publicarDadosAnalise();
void ESPSync();
void tratarMensagemRecebida(const char *topico, const String &mensagem);

//==========================================
//
//    Inicialização dos objetos da classe
//
//==========================================

DHT dht(PIN_DHT, TIPO_DHT);
SENSOR sensor(PIN_MIC);

void setup()
{
  configurarDebug();
  configurarSensor();
  conectarWiFi();
  configTime(10800, 0, "b.ntp.br");

  while (time(nullptr) < 100000)
    delay(100);

  configurarMQTT();
  conectarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  SensorUmidadeTemperatura();
  ruido = sensor.getPercentage(100);
  ESPSync();
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();

  if (millis() - ultimaPublicacao >= intervalo)
  {
    ruido = sensor.getPercentage(100);
    alertaSomEco();
    if (SensorUmidadeTemperatura())
    {
      diferencaTemp();
      publicarDadosAnalise();
      infoLadoOposto();
    }
    else
      debugErro("Erro ao ler sensores, publicacão nao acontecerá");
    ultimaPublicacao = millis();
  }
}
void infoLadoOposto()
{

  debugInfo("=============================================================");
  debugInfo("A Temperatura no lado oposto é: " + String(temperaturaOposto));
  debugInfo("A Umidade no lado oposto é: " + String(umidadeOposto));
  debugInfo("O Ruido no lado oposto é: " + String(ruidoOposto));
  debugInfo("=============================================================");
}
bool SensorUmidadeTemperatura()
{
  umidade = dht.readHumidity();
  temperatura = dht.readTemperature();

  if (isnan(umidade) || isnan(temperatura))
  {
    debugErro("Falha ao iniciar o sensor");
    debugInfo("Verifique a conexão e o pino definido");
    return false;
  }
  return true;
}

void configurarSensor()
{

  debugInfo("==========Sensor DHT22==========");

  dht.begin();
  debugInfo("Sensor inicializado");
  return;
}

void publicarDadosAnalise()
{
  unsigned long timestamp = time(nullptr);
  JsonDocument doc;
  JsonObject analise = doc["analise"].to<JsonObject>();

  debugInfo("==============================");
  debugInfo("Publicando dados de analise...");
  debugInfo("==============================");

  bool alteracao = false;
  if (abs(temperatura - PastPublishedTemperatura) >= 1)
  {
    analise["temperatura"] = temperatura;
    PastPublishedTemperatura = temperatura;
    debugInfo("Temperatura: " + String(temperatura) + "°C");
    alteracao = true;
  }
  else
    debugInfo("Temperatura Não foi publicado pois a variacão foi desconsideravel.");
  if (abs(umidade - PastPublishedUmidade) >= 1)
  {
    analise["umidade"] = umidade;
    PastPublishedUmidade = umidade;
    debugInfo("Umidade: " + String(umidade) + "%");
    alteracao = true;
  }
  else
    debugInfo("Umidade não foi publicado pois a variacão foi desconsideravel.");
  if (abs(ruido - PastPublishedRuido) >= 1)
  {
    analise["ruido"] = ruido;
    PastPublishedRuido = ruido;
    debugInfo("Ruido: " + String(ruido) + "dB");
    alteracao = true;
  }
  else
    debugInfo("Ruido não foi publicado pois a variacão foi desconsideravel.");
  if (comandoAr != PastPublishedComandoAr)
  {
    analise["comandoAr"] = comandoAr;
    PastPublishedComandoAr = comandoAr;
    debugInfo("ComandoAr: " + String(comandoAr));
    alteracao = true;
  }
  else
    debugInfo("ComandoAr não foi publicado pois não houve alteração");
  if (alertaSom != PastPublishedAlertaSom)
  {
    analise["alertaSom"] = alertaSom;
    PastPublishedAlertaSom = alertaSom;
    debugInfo("AlertaSom: " + String(alertaSom));
    alteracao = true;
  }
  else
    debugInfo("AlertaSom não foi publicado pois não houve alteração");
  if (eco != PastPublishedEco)
  {
    analise["eco"] = eco;
    PastPublishedEco = eco;
    debugInfo("Eco: " + String(eco));
    alteracao = true;
  }
  else
    debugInfo("Eco não foi publicado pois não houve alteração");

  if (alteracao)
  {
    analise["timestamp"] = timestamp;
    debugInfo("Timestamp: " + String(timestamp));
    debugInfo("=============================================================");
    char buffer[256];

    serializeJson(doc, buffer, sizeof(buffer)); // (JSON, onde vai ser escrito, tamanho maximo)
    publicarMensagem(obterTopicoPublicacao(0), buffer);
  }
  else
  {
    debugInfo("Nada foi publicado pois não houve nenhuma alterção");
    debugInfo("=============================================================");
  }
}

void tratarMensagemRecebida(const char *topico, const String &mensagem)
{
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, mensagem);

  if (erro)
  {
    debugErro("Erro ao interpretar JSON do lado oposto");
    return;
  }

  mensagemRecebidaOposto = true;
  if (doc["analise"].containsKey("temperatura"))
    temperaturaOposto = doc["analise"]["temperatura"].as<float>();
  if (doc["analise"].containsKey("umidade"))
    umidadeOposto = doc["analise"]["umidade"].as<float>();
  if (doc["analise"].containsKey("ruido"))
    ruidoOposto = doc["analise"]["ruido"].as<float>();

  if (strcmp(topico, obterTopicoRecebimento(1)) == 0)
  {
    if (!syncRealizado)
      ESPSync();
    syncRealizado = true;
    debugInfo("Sincronização realizada entre os ESPs.");

    diferencaTemp();
    alertaSomEco();
    return;
  }

  diferencaTemp();
  alertaSomEco();

  debugInfo("===== DADOS RECEBIDOS =====");
  debugInfo("Dados do ESP32 oposto recebidos, topico: " + String(topico));
  debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
  debugInfo("Umidade lado oposto: " + String(umidadeOposto) + "%");
  debugInfo("Ruido lado oposto: " + String(ruidoOposto) + "dB");
  debugInfo("Aviso para equiparar o ar: " + String(comandoAr));
  debugInfo("Aviso de ruido alto: " + String(alertaSom));
  debugInfo("Aviso para modo economia: " + String(eco));
}

void ESPSync()
{
  JsonDocument doc;
  JsonObject analise = doc["analise"].to<JsonObject>();

  debugInfo("=================================================");
  debugInfo("Publicando dados de analise para sincronização...");
  debugInfo("=================================================");
  debugInfo("Temperatura: " + String(temperatura) + "°C");
  debugInfo("Umidade: " + String(umidade) + "%");
  debugInfo("Ruido: " + String(ruido) + "dB");
  debugInfo("ComandoAr: " + String(comandoAr));
  debugInfo("AlertaSom: " + String(alertaSom));
  debugInfo("Eco: " + String(eco));
  debugInfo("=============================================================");

  analise["temperatura"] = temperatura;
  analise["umidade"] = umidade;
  analise["ruido"] = ruido;
  analise["comandoAr"] = comandoAr;
  analise["alertaSom"] = alertaSom;
  analise["eco"] = eco;

  char buffer[256];

  serializeJson(doc, buffer, sizeof(buffer)); // (JSON, onde vai ser escrito, tamanho maximo)
  publicarMensagem(obterTopicoPublicacao(1), buffer);
}

void diferencaTemp()
{
  if (!mensagemRecebidaOposto)
  {
    comandoAr = 0;
    return;
  }

  float diferencatemp = abs(temperatura - temperaturaOposto);

  if (diferencatemp < 4)
    comandoAr = 0;
  else
  {
    if (temperatura > temperaturaOposto)
      comandoAr = 1;
    else
      comandoAr = 2;
  }

  debugInfo("===== ALERTA TEMPERATURA =====");
  debugInfo("Temperatura capturada pelo sensor deste lado foi: " + String(temperatura));
  debugInfo("Temperatura captada pelo sensor do lado oposto foi: " + String(temperaturaOposto));
  debugInfo("A diferença é de: " + String(diferencatemp));

  switch (comandoAr)
  {
  case 0:
    debugInfo("comandoAr = 0; Sala termicamente equilibrada (diferença de até 3.9°C).");
    break;
  case 1:
    debugInfo("comandoAr = 1; Este Lado esta substancialmente mais quente que o Lado Oposto (diferença maior ou igual a 4°C)");
    break;
  case 2:
    debugInfo("comandoAr = 2; Lado Oposto esta substancialmente mais quente que o Este Lado (diferença maior ou igual a 4°C)");
    break;
  default:
    debugErro("Valor da variável 'comandoAr' é indefinida")
  }

  debugInfo("=============================================================");
  mensagemRecebidaOposto = false;
}

int compaAlertaSom()
{
  if (alertaA && alertaB)
  {
    return 3;
  }
  if (alertaB)
  {
    return 2;
  }
  if (alertaA)
  {
    return 1;
  }
  return 0;
}

void tratarRuido() if (ruido >= SOUND_LIMIT)
{
  silencioA = false;

  if (!ativoA)
  {
    ativoA = true;
    inicioRuidoA = millis();
  }
}
else
{
  ativoA = false;
  inicioRuidoA = 0;

  if (!silencioA)
  {
    silencioA = true;
    inicioSilencioA = millis();
  }
}

if (ruidoOposto >= limitesSom)
{
  silencioB = false;

  if (!ativoB)
  {
    ativoB = true;
    inicioRuidoB = millis();
  }
}
else
{
  ativoB = false;
  inicioRuidoB = 0;

  if (!silencioB)
  {
    silencioB = true;
    inicioSilencioB = millis();
  }
}
}

void alertaSomEco()
{

  if (!mensagemRecebidaOposto)
  {
    alertaSom = 0;
    eco = false;
    return;
  }
  tratarRuido();
  alertaSom = compaAlertaSom();
  bool alertaA = ativoA && (millis() - inicioRuidoA >= duracaoRuido);
  bool alertaB = ativoB && (millis() - inicioRuidoB >= duracaoRuido);

  bool ecoA = silencioA && (millis() - inicioSilencioA >= duracaoEco);
  bool ecoB = silencioB && (millis() - inicioSilencioB >= duracaoEco);

  if (ecoA && ecoB)
    eco = true;
  else
    eco = false;

  if (alertaSom != alertaSomAnterior)
  {
    alertaSomAnterior = alertaSom;

    debugInfo("===== ALERTA SOM / ECO =====");
    debugInfo("Ruido captado pelo sensor Deste Lado foi: " + String(ruido));
    debugInfo("Ruido captado pelo sensor do Lado Oposto foi: " + String(ruidoOposto));

    switch (alertaSom)
    {
    case 0:
      debugInfo("alertaSom = 0; Nível de ruído dentro dos limites de tolerância.");
      break;
    case 1:
      debugInfo("alertaSom = 1; Conversa alta persistente detectada Neste Lado da sala.");
      break;
    case 2:
      debugInfo("alertaSom = 2; Conversa alta persistente detectada no Lado Oposto da sala.");
      break;
    case 3:
      debugInfo("alertaSom = 3; Conversa alta persistente detectada em ambos Lados da sala.");
      break;
    default:
      debugErro("Variavel 'AlertSom' possui valor indefinido");
    }

    if (eco != ecoAnterior)
    {
      ecoAnterior = eco;

      if (!eco)
      {
        debugInfo("Sala não esta vazia.");
        debugInfo("=============================================================");
        return;
      }

      debugInfo("Sala está vazia, necessario ativar modo de economia.");
      debugInfo("=============================================================");
      return;
    }
    return;
  }
}
