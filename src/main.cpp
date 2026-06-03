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
#define intervalo 10000

//==========================================
//
//      Declaração de variaveis globais
//
//==========================================

float temperatura;
float umidade;
float ruido;

float temperaturaOposto;
float umidadeOposto;
float ruidoOposto;

int comandoAr;
int alertaSom;
bool eco;
unsigned long ultimaPublicacao = 0;

unsigned long inicioRuidoA = 0;
unsigned long inicioRuidoB = 0;
bool ativoA = false;
bool ativoB = false;
const unsigned long duracaoRuido = 3000;

unsigned long inicioSilencioA = 0;
unsigned long inicioSilencioB = 0;
bool silencioA = false;
bool silencioB = false;
 
const unsigned long duracaoEco = 900000;

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

void tratarMensagemRecebida(const char* topico, const String & mensagem);

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

    if (millis() - ultimaPublicacao >= intervalo)
    {
        ruido = sensor.getPercentage(20);
        if (SensorUmidadeTemperatura())
            publicarDadosAnalise();
        else
            debugErro("Erro ao ler sensores, publicacão nao acontecerá");
        ultimaPublicacao = millis();
    }
}


bool SensorUmidadeTemperatura()
{
    umidade = dht.readHumidity();
    temperatura = dht.readTemperature();
    ruido = sensor.getPercentage(20);
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
    debugInfo("Timestamp: " + String(timestamp));
    debugInfo("Temperatura: " + String(temperatura));
    debugInfo("Umidade: " + String(umidade));
    debugInfo("Ruido: " + String(ruido));
    debugInfo("ComandoAr: " + String(comandoAr));
    debugInfo("AlertaSom: " + String(alertaSom));
    debugInfo("Eco: " + String(eco));

    analise["timestamp"] = timestamp;
    analise["temperatura"] = temperatura;
    analise["umidade"] = umidade;
    analise["ruido"] = ruido;
    analise["comandoAr"] = comandoAr;
    analise["alertaSom"] = alertaSom;
    analise["eco"] = eco;

    char buffer[256];

    serializeJson(doc, buffer, sizeof(buffer)); // (JSON, onde vai ser escrito, tamanho maximo)
    publicarMensagem(obterTopicoPublicacao(0), buffer);
}


void tratarMensagemRecebida(const char* topico, const String & mensagem)
{
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, mensagem);

  if(erro)
  {
    debugErro("Erro ao interpretar JSON do lado oposto");
    return;
  }

  temperaturaOposto = doc["analise"]["temperatura"].as<float>();
  umidadeOposto = doc["analise"]["umidade"].as<float>();
  ruidoOposto = doc["analise"]["ruido"].as<float>();
}

void diferencaTemp()
{
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
}

void alertaSomEco()
{
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

    if (!silencioA) 
    {
     silencioA = true;
     inicioSilencioA = agora;
    }
  }
  
  
    if (ruido >= limitesSom)
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

     if (!silencioB) {
      silencioB = true;
      inicioSilencioB = agora;
    }
  }
  

  bool alertaA = ativoA && (agora - inicioRuidoA >= duracaoRuido);
  bool alertaB = ativoB && (agora - inicioRuidoB >= duracaoRuido);

  if      (alertaA && alertaB)
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
    eco = 1;
  }
  else 
  {
    eco = 0;
  }
}