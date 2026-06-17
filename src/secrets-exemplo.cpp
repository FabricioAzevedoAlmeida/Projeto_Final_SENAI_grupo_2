#include "secrets.h"

#ifndef SECRETS_H
#define SECRETS_H

// ── WiFi ─────────────────────────────────
#define WIFI_SSID     "sua-rede"
#define WIFI_SENHA    "sua-senha"

// ── MQTT Padrão ──────────────────────────
#define MQTT_BROKER      "seu-broker"
#define MQTT_PORTA       8883
#define MQTT_CLIENT_ID   "esp32-sala-A"
#define MQTT_USUARIO     "" 
#define MQTT_SENHA       ""

// ── Tópicos (Matriz Espelhada) ───────────
#define TOTAL_TOPICOS_PUBLICAR  2
#define TOTAL_TOPICOS_RECEBER   2
const char* TOPICOS_PUBLICAR[] = { "sala/A/analise", "sala/A/sync" };
const char* TOPICOS_RECEBER[]  = { "sala/B/analise", "sala/B/sync" };

// ── Modos de Conexão ──────────────────────
#define USAR_AWS_IOT     true     // Ativa a pilha AWS IoT Core com criptografia TLS
#define MQTT_USAR_TLS    true
#define MQTT_CERTIFICADO_CA ""

// ── AWS IoT Core Certificados (Obrigatório se USAR_AWS_IOT = true) ──
#define AWS_IOT_ENDPOINT  "xxxxxxxxxxxxxx-ats.iot.us-east-1.amazonaws.com"
#define AWS_IOT_PORT      8883
#define AWS_IOT_CLIENT_ID "esp32-sala-A"

// Certificados de autenticação mútua (X.509)
const char AWS_CERT_CA[] PROGMEM = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";
const char AWS_CERT_CRT[] PROGMEM = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n";
const char AWS_CERT_PRIVATE[] PROGMEM = "-----BEGIN RSA PRIVATE KEY-----\n...\n-----END RSA PRIVATE KEY-----\n";

#endif