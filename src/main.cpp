#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "KY038.h"
#include "DebugManager.h"
#include "WiFiManager.h"
#include "MqttManager.h"

//==========================================
//
//      Declaração de constantes globais
//
//==========================================

#define PIN_DHT 8
#define TIPO_DHT DHT22
#define PIN_MIC 9
#define intervalo 3000

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

//
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

int alertaSomAnterior = -1;
bool ecoAnterior = false;

const unsigned long duracaoEco = 900000;

bool mensagemRecebidaOposto = false;

//==========================================
//
//     Prototipação de funções
//
//==========================================

bool SensorUmidadeTemperatura();
void configurarSensor();
void publicarDadosAnalise();
void diferencaTemp();
void alertaSomEco();

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
  configurarMQTT();
  conectarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();
  ruido = sensor.getPercentage(100);
  alertaSomEco();

  if (millis() - ultimaPublicacao >= intervalo)
  {
    if (SensorUmidadeTemperatura())
    {
      diferencaTemp();
      publicarDadosAnalise();
    }
    else
      debugErro("Erro ao ler sensores, publicacão nao acontecerá");
    ultimaPublicacao = millis();
  }
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
    debugInfo("Não foi publicado temperatura pois a variacão foi desconsideravel.");
  if (abs(umidade - PastPublishedUmidade) >= 1)
  {
    analise["umidade"] = umidade;
    PastPublishedUmidade = umidade;
    debugInfo("Umidade: " + String(umidade) + "%");
    alteracao = true;
  }
  else
    debugInfo("Não foi publicado humidade pois a variacão foi desconsideravel.");
  if (abs(ruido - PastPublishedRuido) >= 1)
  {
    analise["ruido"] = ruido;
    PastPublishedRuido = ruido;
    debugInfo("Ruido: " + String(ruido) + "dB");
    alteracao = true;
  }
  else
    debugInfo("Não foi publicado ruido pois a variacão foi desconsideravel.");
  if (comandoAr != PastPublishedComandoAr)
  {
    analise["comandoAr"] = comandoAr;
    PastPublishedComandoAr = comandoAr;
    debugInfo("ComandoAr: " + String(comandoAr));
    alteracao = true;
  }
  else
    debugInfo("Não foi publicado comandoAr pois não houve alteração");
  if (alertaSom != PastPublishedAlertaSom)
  {
    analise["alertaSom"] = alertaSom;
    PastPublishedAlertaSom = alertaSom;
    debugInfo("AlertaSom: " + String(alertaSom));
    alteracao = true;
  }
  else
    debugInfo("Não foi publicado AlertaSom pois não houve alteração");
  if (eco != PastPublishedEco)
  {
    analise["eco"] = eco;
    PastPublishedEco = eco;
    debugInfo("Eco: " + String(eco));
    alteracao = true;
  }
  else
    debugInfo("Não foi publicado Eco pois não houve alteração");
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
    debugInfo("Nada foi publicado pois não houve nenhuma alterção");
  debugInfo("=============================================================");
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

void diferencaTemp()
{
  if (!mensagemRecebidaOposto)
  {
    comandoAr = 0;
    return;
  }

  float diferencatemp = abs(temperatura - temperaturaOposto);

  if (diferencatemp < 4)
  {
    comandoAr = 0;
  }
  else
  {
    if (temperatura > temperaturaOposto)
    {
      comandoAr = 1;
    }
    else
    {
      comandoAr = 2;
    }
  }

  debugInfo("===== ALERTA TEMPERATURA =====");
  debugInfo("Temperatura capturada pelo sensor deste lado foi: " + String(temperatura));
  debugInfo("Temperatura captada pelo sensor do lado oposto foi: " + String(temperaturaOposto));
  debugInfo("A diferença é de: " + String(diferencatemp));
  if (comandoAr == 0)
    debugInfo("comandoAr = 0; Sala termicamente equilibrada (diferença de até 3.9°C).");
  else if (comandoAr == 1)
    debugInfo("comandoAr = 1; Este Lado esta substancialmente mais quente que o Lado Oposto (diferença maior ou igual a 4°C)");
  else
    debugInfo("comandoAr = 2; Lado Oposto esta substancialmente mais quente que o Este Lado (diferença maior ou igual a 4°C)");
  debugInfo("=============================================================");
}

void alertaSomEco()
{
  if (!mensagemRecebidaOposto)
  {
    alertaSom = 0;
    eco = false;
    return;
  }

  unsigned long agora = millis();
  int limitesSom = 70;

  if (ruido >= limitesSom)
  {
    silencioA = false;

    if (!ativoA)
    {
      ativoA = true;
      inicioRuidoA = agora;
    }
  }
  else
  {
    ativoA = false;
    inicioRuidoA = 0;

    if (!silencioA)
    {
      silencioA = true;
      inicioSilencioA = agora;
    }
  }

  if (ruidoOposto >= limitesSom)
  {
    silencioB = false;

    if (!ativoB)
    {
      ativoB = true;
      inicioRuidoB = agora;
    }
  }
  else
  {
    ativoB = false;
    inicioRuidoB = 0;

    if (!silencioB)
    {
      silencioB = true;
      inicioSilencioB = agora;
    }
  }

  bool alertaA = ativoA && (agora - inicioRuidoA >= duracaoRuido);
  bool alertaB = ativoB && (agora - inicioRuidoB >= duracaoRuido);

  if (alertaA && alertaB)
  {
    alertaSom = 3;
  }
  else if (alertaA)
  {
    alertaSom = 1;
  }
  else if (alertaB)
  {
    alertaSom = 2;
  }
  else
  {
    alertaSom = 0;
  }

  bool ecoA = silencioA && (agora - inicioSilencioA >= duracaoEco);
  bool ecoB = silencioB && (agora - inicioSilencioB >= duracaoEco);

  if (ecoA && ecoB)
  {
    eco = true;
  }
  else
  {
    eco = false;
  }

  if (alertaSom != alertaSomAnterior)
  {
    alertaSomAnterior = alertaSom;

    debugInfo("===== ALERTA SOM / ECO =====");
    debugInfo("Ruido captado pelo sensor Deste Lado foi: " + String(ruido));
    debugInfo("Ruido captado pelo sensor do Lado Oposto foi: " + String(ruidoOposto));

    if (alertaSom == 0)
      debugInfo("alertaSom = 0; Nível de ruído dentro dos limites de tolerância.");
    else if (alertaSom == 1)
      debugInfo("alertaSom = 1; Conversa alta persistente detectada Neste Lado da sala.");
    else if (alertaSom == 2)
      debugInfo("alertaSom = 2; Conversa alta persistente detectada no Lado Oposto da sala.");
    else
      debugInfo("alertaSom = 3; Conversa alta persistente detectada em ambos Lados da sala.");
  }

  if (eco != ecoAnterior)
  {
    ecoAnterior = eco;

    if (eco == false)
    {
      debugInfo("Sala não esta vazia.");
      debugInfo("=============================================================");
    }
    else
    {
      debugInfo("Sala está vazia, necessario ativar modo de economia.");
      debugInfo("=============================================================");
    }
  }
}