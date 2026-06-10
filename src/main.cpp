/**
 * @file main.cpp
 * @brief Monitoramento de Ruído e Clima com Escuta Cruzada.
 * * Envio contínuo e completo com Logs de Debug das próprias publicações.
 */

#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <DebugManager.h>
#include <ESP32Connectivity.h>
#include "secrets.h"
#include "KY038.h"

#define CONNECTIVITY_FILA_SLOTS 15
#define CONNECTIVITY_FILA_PAYLOAD_MAX 512


// ── Mapeamento de Pinos do Hardware ──────────────────────────
#define PIN_MIC  9
#define PIN_DHT 8
#define TIPO_DHT DHT22

DHT dht(PIN_DHT, TIPO_DHT);
SENSOR sensor(PIN_MIC);

// ── Variáveis de sensor ───────────────────────────────────────
float ruido = 0.0;
float valorUmidade = 0.0;
float valorTemperatura = 0.0;

// ── Variáveis do lado oposto ──────────────────────────────────
float temperaturaOposto = 0.0; 
float umidadeOposto = 0.0; 
float ruidoOposto = 0.0;
bool  mensagemRecebidaOposto = false;

// ── Lógica de alertas ─────────────────────────────────────────
int  comandoAr = 0;
int  alertaSom = 0;
bool eco = false;

bool syncRealizado = false;

// ── Controle de tempo e alertas ───────────────────────────────
const uint32_t intervaloPublicacaoMs = 10000;
uint32_t ultimaPublicacao = 0;

int  alertaSomAnterior = -1;
bool ecoAnterior = false;

unsigned long inicioRuidoA = 0, inicioRuidoB = 0;
unsigned long inicioSilencioA = 0, inicioSilencioB = 0;
bool ativoA = false, ativoB = false;
bool silencioA = false, silencioB = false;

const unsigned long duracaoRuido = 300;
const unsigned long duracaoEco = 900000;
const int limiteSom = 70;

ConfigTopicos topicos = {
    TOPICOS_PUBLICAR, TOTAL_TOPICOS_PUBLICAR,
    TOPICOS_RECEBER,  TOTAL_TOPICOS_RECEBER
};

// ── Protótipos ────────────────────────────────────────────────
bool SensorUmidadeTemperatura();
void diferencaTemp();
void alertaSomEco();
void publicarDadosAnalise();
void aoReceberMensagem(const char* topico, const String& mensagem);
void ESPSync();

// ── Callbacks de Rede ─────────────────────────────────────────
void aoConectarWiFi()    { debugInfo("WiFi conectado com sucesso! IP: " + WiFi.localIP().toString()); }
void aoDesconectarWiFi() { debugAviso("Conexão WiFi perdida. Entrando em modo offline..."); }
void aoConectarMQTT()    { debugInfo(">>> Conectado ao Broker/AWS com sucesso!"); }
void aoDesconectarMQTT() { debugErro(">>> Conexão com a AWS interrompida."); }

// ── Setup ─────────────────────────────────────────────────────
void setup() 
{
    configurarDebug(DEBUG_NIVEL_INICIAL, PINO_HABILITA_DEBUG_COMPLETO);

    dht.begin();
    debugInfo("==========Sensor DHT22==========");
    debugInfo("Sensor inicializado");

    configTime(10800, 0, "b.ntp.br");

    conectividade.configurarBufferMQTT(1024);
    conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
    conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);
    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
    conectividade.registrarCallbackMensagem(aoReceberMensagem);

    if (USAR_AWS_IOT) {
        conectividade.beginAWS(
            { WIFI_SSID, WIFI_SENHA },
            { AWS_IOT_ENDPOINT, AWS_IOT_PORT, AWS_IOT_CLIENT_ID, AWS_CERT_CA, AWS_CERT_CRT, AWS_CERT_PRIVATE },
            topicos
        );
    } else {
        conectividade.beginTLS(
            { WIFI_SSID, WIFI_SENHA },
            { MQTT_BROKER, MQTT_PORTA, MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA },
            { MQTT_CERTIFICADO_CA },
            topicos
        );
    }

    while(time(nullptr) < 100000 || !conectividade.mqttConectado()) {
        conectividade.update();
    }

    SensorUmidadeTemperatura();
    ruido = sensor.getPercentage(100);
    ESPSync();

    debugInfo("Setup concluído.");
}

// ── Loop Principal ───────────────────────────────────────────
void loop() 
{
    conectividade.update();
    ruido = sensor.getPercentage(100);
    
    diferencaTemp();
    alertaSomEco();

    if(millis() - ultimaPublicacao >= intervaloPublicacaoMs) {
        ultimaPublicacao = millis();

        if(SensorUmidadeTemperatura()) {
            publicarDadosAnalise(); 
        }

        else{
            debugErro("Erro ao ler sensores, publicação não acontecerá.");
        }

        if(conectividade.mensagensNaFila() > 0)
            debugAviso("Modo Offline! Mensagens na fila: " + String(conectividade.mensagensNaFila()));
    }
}

// ── Callback de Mensagens MQTT ─────────────────────
void aoReceberMensagem(const char* topico, const String& mensagem) 
{
    JsonDocument doc;
    DeserializationError erro = deserializeJson(doc, mensagem);
    if(erro){ 
        debugErro("Erro ao interpretar JSON do lado oposto"); 
        return; 
    }

    JsonObject analise = doc["analise"];
    
    if(analise["temperatura"].is<float>()){
        float tempVerificacao = analise["temperatura"].as<float>();
        if(tempVerificacao > 0.5){ 
            temperaturaOposto = tempVerificacao;
            mensagemRecebidaOposto = true; 
        }
    }
    
    if(analise["umidade"].is<float>()){
        float umidVerificacao = analise["umidade"].as<float>();
        if(umidVerificacao > 0.5){
            umidadeOposto = umidVerificacao;
        }
    }
    
    if(analise["ruido"].is<float>()){
        ruidoOposto = analise["ruido"].as<float>();
    }

    if(String(topico) == conectividade.topicoRecebimento(1)){
        if(!syncRealizado){
            syncRealizado = true;
            ESPSync(); 
            debugInfo("Sincronização inicial realizada entre os ESPs.");
        }
        return;
    }

    debugInfo("===== DADOS RECEBIDOS =====");
    debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
    debugInfo("Umidade lado oposto: "     + String(umidadeOposto)     + "%");
    debugInfo("Ruido lado oposto: "       + String(ruidoOposto)       + "dB");
}

// ── Leitura do DHT ────────────────────────────────────────────
bool SensorUmidadeTemperatura() 
{
    valorUmidade     = dht.readHumidity();
    valorTemperatura = dht.readTemperature();

    if(isnan(valorUmidade) || isnan(valorTemperatura)){
        debugErro("Falha ao ler o DHT!");
        return false;
    }
    return true;
}

// ── Sincronização inicial entre ESPs ─────────────────────────
void ESPSync() 
{
    JsonDocument doc;
    JsonObject analise = doc["analise"].to<JsonObject>();

    analise["temperatura"] = valorTemperatura;
    analise["umidade"] = valorUmidade;
    analise["ruido"] = ruido;
    analise["comandoAr"] = comandoAr;
    analise["alertaSom"] = alertaSom;
    analise["eco"] = eco;

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));

    debugInfo("=================================================");
    debugInfo("Publicando dados para sincronização...");
    debugInfo("=================================================");
    debugInfo("Temperatura: " + String(valorTemperatura) + "°C");
    debugInfo("Umidade: "     + String(valorUmidade)     + "%");
    debugInfo("Ruido: "       + String(ruido)            + "dB");
    debugInfo("ComandoAr: "   + String(comandoAr));
    debugInfo("AlertaSom: "   + String(alertaSom));
    debugInfo("Eco: "         + String(eco ? "SIM" : "NÃO"));
    debugInfo("=============================================================");

    conectividade.publicar(1, buffer);
}

// ── Lógica de temperatura ─────────────────────────────────────
void diferencaTemp() 
{
    if(!mensagemRecebidaOposto){ 
        comandoAr = 0; 
        return; 
    }

    float diferencatemp = abs(valorTemperatura - temperaturaOposto);

    if(diferencatemp < 4){
        comandoAr = 0;
    } 
    
    else{
        if(valorTemperatura > temperaturaOposto){
            comandoAr = 1;
        }
        
        else{
            comandoAr = 2;
        }
    }

    static int ultimoComandoArLog = -1;
    if(comandoAr != ultimoComandoArLog){
        ultimoComandoArLog = comandoAr;
        debugInfo("===== ALERTA TEMPERATURA =====");
        debugInfo("Temperatura deste lado: "  + String(valorTemperatura) + "°C");
        debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
        debugInfo("Diferença: "               + String(diferencatemp) + "°C");
        if(comandoAr == 0) 
            debugInfo("comandoAr = 0; Sala termicamente equilibrada (diferença de até 3.9°C).");
        else if(comandoAr == 1) 
            debugInfo("comandoAr = 1; Deste lado substancialmente mais quente (diferença >= 4°C).");
        else 
            debugInfo("comandoAr = 2; Lado oposto substancialmente mais quente (diferença >= 4°C).");
        debugInfo("=============================================================");
    }
}

// ── Lógica de som e eco ───────────────────────────────────────
void alertaSomEco()
{
    if(!mensagemRecebidaOposto){ 
        alertaSom = 0; eco = false; 
        return; 
    }

    unsigned long agora = millis();

    if(ruido >= limiteSom){
        silencioA = false;
        if(!ativoA){ 
            ativoA = true; inicioRuidoA = agora; 
        }
    } 

    else{
        ativoA = false; inicioRuidoA = 0;
        if(!silencioA){ 
            silencioA = true; inicioSilencioA = agora; 
        }
    }

    if(ruidoOposto >= limiteSom){
        silencioB = false;
        if(!ativoB){ 
            ativoB = true; inicioRuidoB = agora; 
        }
    } 

    else{
        ativoB = false; inicioRuidoB = 0;
        if(!silencioB){ 
            silencioB = true; inicioSilencioB = agora; 
        }
    }

    bool alertaA = ativoA && (agora - inicioRuidoA >= duracaoRuido);
    bool alertaB = ativoB && (agora - inicioRuidoB >= duracaoRuido);

    if(alertaA && alertaB) 
        alertaSom = 3;
    
    else if(alertaA)            
        alertaSom = 1;

    else if(alertaB)            
        alertaSom = 2;

    else 
        alertaSom = 0;

    bool ecoA = silencioA && (agora - inicioSilencioA >= duracaoEco);
    bool ecoB = silencioB && (agora - inicioSilencioB >= duracaoEco);
    eco = ecoA && ecoB;

    if(alertaSom != alertaSomAnterior){
        alertaSomAnterior = alertaSom;
        debugInfo("===== ALERTA SOM / ECO =====");
        debugInfo("Ruido deste lado: "   + String(ruido)       + "dB");
        debugInfo("Ruido lado oposto: "  + String(ruidoOposto) + "dB");

        if(alertaSom == 0) 
            debugInfo("alertaSom = 0; Nível de ruído dentro dos limites de tolerância.");
        
        else if(alertaSom == 1)
            debugInfo("alertaSom = 1; Conversa alta persistente detectada neste lado.");

        else if(alertaSom == 2) 
            debugInfo("alertaSom = 2; Conversa alta persistente detectada no lado oposto.");

        else                    
            debugInfo("alertaSom = 3; Conversa alta persistente detectada em ambos os lados.");

        debugInfo("=============================================================");
    }

    if(eco != ecoAnterior){
        ecoAnterior = eco;
        debugInfo(eco ? "Sala vazia - necessário ativar modo de economia." : "Sala não está vazia.");
        debugInfo("=============================================================");
    }
}

// ── Publicação Contínua com Histórico de Debug Local ──────────
void publicarDadosAnalise() 
{
    unsigned long timestamp = time(nullptr);
    JsonDocument doc;
    JsonObject analise = doc["analise"].to<JsonObject>();

    // Todas as chaves e valores são obrigatoriamente anexados a cada execução
    analise["temperatura"] = valorTemperatura;
    analise["umidade"]     = valorUmidade;
    analise["ruido"]       = ruido;
    analise["comandoAr"]   = comandoAr;
    analise["alertaSom"]   = alertaSom;
    analise["eco"]         = eco;
    analise["timestamp"]   = timestamp;
    
    // ── DEBUG LOCAL DO PAYLOAD ENVIADO ────────────────────────
    debugInfo("====================================================");
    debugInfo(">>> DADOS PUBLICADOS POR ESTE ESP <<<");
    debugInfo("====================================================");
    debugInfo("Temperatura Local : " + String(valorTemperatura) + "°C");
    debugInfo("Umidade Local     : " + String(valorUmidade)     + "%");
    debugInfo("Ruido Local       : " + String(ruido)            + "dB");
    debugInfo("Comando Ar Cond.  : " + String(comandoAr));
    debugInfo("Alerta de Som     : " + String(alertaSom));
    debugInfo("Modo Eco Ativo    : " + String(eco ? "SIM" : "NÃO"));
    debugInfo("Timestamp         : " + String(timestamp));
    debugInfo("====================================================");

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));
    conectividade.publicar(0, buffer); 
}