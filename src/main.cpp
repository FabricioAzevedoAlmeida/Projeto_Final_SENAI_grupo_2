#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "KY038.h"
#include "DebugManager.h"
#include "WiFiManager.h"
#include "MqttManager.h"

#define PIN_DHT 40
#define TIPO_DHT DHT22
#define PIN_MIC 20

float temperatura;
float umidade; 
float ruido;
int comandoAr;
int alertaSom;
bool eco;

unsigned long ultimaPublicacao = 0;
const unsigned long intervalo = 10000;

void SensorUmidadeTemperatura();
void configurarSensor();

const char  TOPICO_COMANDO[] = "sala09/analise/lado_A";

void publicarDadosAnalise();

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
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();
  SensorUmidadeTemperatura();

  if(millis() - ultimaPublicacao >= intervalo)
  {
    ultimaPublicacao = millis();
    publicarDadosAnalise();
  }
}


void SensorUmidadeTemperatura()
{
  delay(1000);



  if(isnan(umidade) || isnan(temperatura))
  {
    debugErro("Falha ao iniciar o sensor");
    debugInfo("Verifique a conexão e o pino definido");
    return;
  }
  umidade =  dht.readHumidity();
  temperatura = dht.readTemperature();
  ruido = sensor.getPercentage(50);
}

void configurarSensor()
{
 
  debugInfo("==========Sensor DHT22==========");

  dht.begin(); // inicializa o sensor DHT22
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

  serializeJson(doc, buffer, sizeof(buffer));  // (JSON, onde vai ser escrito, tamanho maximo)
  publicarMensagem(TOPICO_COMANDO, buffer);
}