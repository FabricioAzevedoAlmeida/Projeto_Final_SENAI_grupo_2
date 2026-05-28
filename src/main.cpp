#include <Arduino.h>
#include <DHT.h>
#include "DebugManager.h"

#define PIN_DHT 5 // pino escolhido
#define TIPO_DHT DHT22

void SensorUmidadeTemperatura();
void configurarSensor();

float temperatura;
float umidade;

DHT dht(PIN_DHT, TIPO_DHT);


void setup()
{
  configurarDebug();
  configurarSensor();
}

void loop()
{
  //Serial.println("EU SOU GAY");
  SensorUmidadeTemperatura();
}


void SensorUmidadeTemperatura()
{
  delay(1000);
  umidade =  dht.readHumidity();
  temperatura = dht.readTemperature();

  if(isnan(umidade) || isnan(temperatura))
  {
    debugErro("Falha ao iniciar o sensor");
    debugInfo("Verifique a conexão e o pino definido");
    return;
  }

   debugInfo("Temperatura: " + String(temperatura) + "°C");
   debugInfo("Umidade: "+ String(umidade));

   debugInfo("------------------------------");

   return;
}


void configurarSensor()
{
 
  debugInfo("==========Sensor DHT22==========");

  dht.begin(); // inicializa o sensor DHT22
  debugInfo("Sensor inicializado");
  return;
}