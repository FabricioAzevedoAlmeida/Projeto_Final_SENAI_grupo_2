#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include "WiFiManager.h"
#include "secrets.h"
#include "DebugManager.h"

bool wifiConectado()
{
  return WiFi.status() == WL_CONNECTED;
}

void conectarWiFi()
{
  static unsigned long lastRunOfFunction = 0;
  static unsigned long beforeReconnectLoop = 0;
  if (millis() - lastRunOfFunction >= 5000)
  {
    if (WiFi.status() == WL_CONNECTED)
      return;
    lastRunOfFunction = millis();

    debugInfo("==========================");
    debugInfo("Iniciando conexão WiFi...");
    debugInfo("==========================");

    // Configura o ESP32 como station, ou seja
    // ele vai se conectar a um roteador existente.
    WiFi.mode(WIFI_STA);

    // Inicia a conexão com SSID e senha
    WiFi.begin(WIFI_SSID, WIFI_SENHA);

    debugInfo("conectando");

    int tentativasWiFi = 0;
    const int maxTentativasWiFi = 30;

    // Aguarda a conexão por ate 30 tentativas
    while (WiFi.status() != WL_CONNECTED && tentativasWiFi < maxTentativasWiFi && millis() - beforeReconnectLoop >= 2000)
    {
      beforeReconnectLoop = millis();
      debugInfoSemLinha(".");
      tentativasWiFi++;
    }

    debugInfo("");

    if (WiFi.status() == WL_CONNECTED)
    {
      debugInfo("WiFi conectado com sucesso!");
      debugInfo("Endereço IP: ");
      debugInfo(String(WiFi.localIP().toString()));
    }

    else
    {
      debugErro("Falha ao conectar no WiFi.");
      debugErro("Verifique SSID, senha e sinal de rede.");
    }
  }
}

void garantirWiFiConectado()
{
  static unsigned long lastRunOfFunction = 0;
  if (millis() - lastRunOfFunction >= 2000)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      debugErro("WiFi desconectado. tentando reconectar...");
      conectarWiFi();
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      debugErro("Não foi possivel reconectar ao WiFi");
    }
  }
  lastRunOfFunction = millis();
}