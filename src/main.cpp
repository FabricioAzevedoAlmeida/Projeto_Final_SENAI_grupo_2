#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <DebugManager.h>
#include <ESP32Connectivity.h>
#include "secrets.h"
#include "KY038.h"

//==========================================
//
//      Declaração de constantes globais
//
//==========================================

#define PIN_DHT 8
#define TIPO_DHT DHT22
#define PIN_MIC 9
#define INTERVALO 3000
#define SOUND_LIMIT 70
#define RUIDO_TIME 300
#define ECO_TIME 900000

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

unsigned long ultimaPublicacao = 0;

unsigned long inicioRuidoA = 0;
unsigned long inicioRuidoB = 0;
bool ativoA = false;
bool ativoB = false;
bool alertaA = false;
bool alertaB = false;
bool alertaSomAnterior = false;
bool ecoAnterior = false;

unsigned long inicioSilencioA = 0;
unsigned long inicioSilencioB = 0;
bool silencioA = false;
bool silencioB = false;
bool mensagemRecebidaOposto = false;

//==========================================
//
//    Inicialização dos objetos da classe
//
//==========================================

DHT dht(PIN_DHT, TIPO_DHT);
SENSOR sensor(PIN_MIC);
ConfigTopicos topicos = {
    TOPICOS_PUBLICAR, TOTAL_TOPICOS_PUBLICAR,
    TOPICOS_RECEBER, TOTAL_TOPICOS_RECEBER};
//==========================================
//
//   Declaracao Callbacks de Rede
//
//==========================================

void aoConectarWiFi() { debugInfo("WiFi conectado com sucesso! IP: " + WiFi.localIP().toString()); }
void aoDesconectarWiFi() { debugAviso("Conexão WiFi perdida. Entrando em modo offline..."); }
void aoConectarMQTT() { debugInfo(">>> Conectado ao Broker/AWS com sucesso!"); }
void aoDesconectarMQTT() { debugErro(">>> Conexão com a AWS interrompida."); }

void ESPSync()
{
  JsonDocument doc;
  JsonObject analise = doc["analise"].to<JsonObject>();

  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));
  conectividade.publicar(1, buffer);

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
}

void aoReceberMensagem(const char *topico, const String &mensagem)
{
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, mensagem);
  if (erro)
  {
    debugErro("Erro ao interpretar JSON do lado oposto");
    return;
  }

  JsonObject analise = doc["analise"];

  if (analise["temperatura"].is<float>())
  {
    float tempVerificacao = analise["temperatura"].as<float>();
    if (tempVerificacao > 0.5)
    {
      temperaturaOposto = tempVerificacao;
      mensagemRecebidaOposto = true;
    }
  }

  if (analise["umidade"].is<float>())
  {
    float umidVerificacao = analise["umidade"].as<float>();
    if (umidVerificacao > 0.5)
    {
      umidadeOposto = umidVerificacao;
    }
  }

  if (analise["ruido"].is<float>())
  {
    ruidoOposto = analise["ruido"].as<float>();
  }

  if (String(topico) == conectividade.topicoRecebimento(1))
  {
    if (!syncRealizado)
    {
      syncRealizado = true;
      ESPSync();
      debugInfo("Sincronização inicial realizada entre os ESPs.");
    }
    return;
  }

  debugInfo("===== DADOS RECEBIDOS =====");
  debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
  debugInfo("Umidade lado oposto: " + String(umidadeOposto) + "%");
  debugInfo("Ruido lado oposto: " + String(ruidoOposto) + "dB");
}

void configConectivity()
{
  conectividade.configurarBufferMQTT(1024);
  conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
  conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);
  conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
  conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
  conectividade.registrarCallbackMensagem(aoReceberMensagem);

  if (USAR_AWS_IOT)
  {
    conectividade.beginAWS(
        {WIFI_SSID, WIFI_SENHA},
        {AWS_IOT_ENDPOINT, AWS_IOT_PORT, AWS_IOT_CLIENT_ID, AWS_CERT_CA, AWS_CERT_CRT, AWS_CERT_PRIVATE},
        topicos);
  }
  else
  {
    conectividade.beginTLS(
        {WIFI_SSID, WIFI_SENHA},
        {MQTT_BROKER, MQTT_PORTA, MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA},
        {MQTT_CERTIFICADO_CA},
        topicos);
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

  analise["temperatura"] = temperatura;
  analise["umidade"]     = umidade;
  analise["ruido"]       = ruido;
  analise["comandoAr"]   = comandoAr;
  analise["alertaSom"]   = alertaSom;
  analise["eco"]         = eco;
  analise["timestamp"]   = timestamp;

  debugInfo("==============================");
  debugInfo("Publicando dados de analise...");
  debugInfo("==============================");
  debugInfo("Temperatura: " + String(temperatura) + "°C");
  debugInfo("Umidade: "     + String(umidade)     + "%");
  debugInfo("Ruido: "       + String(ruido)        + "dB");
  debugInfo("ComandoAr: "   + String(comandoAr));
  debugInfo("AlertaSom: "   + String(alertaSom));
  debugInfo("Eco: "         + String(eco ? "SIM" : "NÃO"));
  debugInfo("Timestamp: "   + String(timestamp));
  debugInfo("=============================================================");

  bool alteracao = false;

  analise["temperatura"] = temperatura;
  debugInfo("Temperatura: " + String(temperatura) + "°C");

  analise["umidade"] = umidade;
  debugInfo("Umidade: " + String(umidade) + "%");

  analise["ruido"] = ruido;
  debugInfo("Ruido: " + String(ruido) + "dB");

  analise["comandoAr"] = comandoAr;
  debugInfo("ComandoAr: " + String(comandoAr));

  analise["alertaSom"] = alertaSom;
  debugInfo("AlertaSom: " + String(alertaSom));

  analise["eco"] = eco;
  debugInfo("Eco: " + String(eco));

  analise["timestamp"] = timestamp;
  debugInfo("Timestamp: " + String(timestamp));
  debugInfo("=============================================================");

  char buffer[512];
  serializeJson(doc, buffer, sizeof(buffer));
  conectividade.publicar(0, buffer);
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
    debugErro("Valor da variável 'comandoAr' é indefinida");
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

void tratarRuido()
{
  if (ruido >= SOUND_LIMIT)
    silencioA = false;

  if (!ativoA)
  {
    ativoA = true;
    inicioRuidoA = millis();
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

  if (ruidoOposto >= SOUND_LIMIT)
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
  bool alertaA = ativoA && (millis() - inicioRuidoA >= RUIDO_TIME);
  bool alertaB = ativoB && (millis() - inicioRuidoB >= RUIDO_TIME);

  bool ecoA = silencioA && (millis() - inicioSilencioA >= RUIDO_TIME);
  bool ecoB = silencioB && (millis() - inicioSilencioB >= RUIDO_TIME);

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

void setup()
{
  configurarDebug(DEBUG_NIVEL_INICIAL, PINO_HABILITA_DEBUG_COMPLETO);
  configurarSensor();
  configConectivity();
  configTime(10800, 0, "b.ntp.br");

  while (time(nullptr) < 100000 || !conectividade.mqttConectado())
  {
    conectividade.update();
  }

  SensorUmidadeTemperatura();
  ruido = sensor.getPercentage(100);
  ESPSync();
  debugInfo("Setup concluído.");
}

void loop()
{
  conectividade.update();
  if (millis() - ultimaPublicacao >= INTERVALO)
  {
    ruido = sensor.getPercentage(100);
    alertaSomEco();
    if (SensorUmidadeTemperatura())
    {
      diferencaTemp();
      publicarDadosAnalise();
    }
    else
      debugErro("Erro ao ler sensores, publicacão nao acontecerá");
    ultimaPublicacao = millis();
    if (conectividade.mensagensNaFila() > 0)
      debugAviso("Modo Offline! Mensagens na fila: " + String(conectividade.mensagensNaFila()));
  }
}