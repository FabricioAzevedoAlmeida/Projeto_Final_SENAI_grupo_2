/**
 * @file main.cpp
 * @brief Monitoramento de Ruído e Clima com Escuta Cruzada.
 * * Envio contínuo e completo com Logs de Debug das próprias publicações.
 */

#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "KY038.h"
#include "DebugManager.h"

#define CONNECTIVITY_FILA_SLOTS 15
#define CONNECTIVITY_FILA_PAYLOAD_MAX 512

#include <ESP32Connectivity.h>
#include "secrets.h"

// ── Mapeamento de Pinos do Hardware ──────────────────────────
const int pinoKy038Digital = 9;
const int pinoDht = 8;
#define TIPO_DHT DHT22

DHT dht(pinoDht, TIPO_DHT);
SENSOR sensor(pinoKy038Digital);

// ── Variáveis de sensor ───────────────────────────────────────
float ruido = 0.0;
float valorUmidade = 0.0;
float valorTemperatura = 0.0;

// ── Variáveis do lado oposto ──────────────────────────────────
float temperaturaOposto = 0.0; 
float umidadeOposto = 0.0; 
float ruidoOposto = 0.0;
int alertaSomOposto = 0;
bool ecoOposto = false;
bool  mensagemRecebidaOposto = false;

// ── Valores anteriores publicados ─────────
static float pastPublishedTemperatura = 0.0;
static float pastPublishedUmidade = 0.0;
static float pastPublishedRuido = 0.0;
static int   pastPublishedComandoAr = 0;
static int   pastPublishedAlertaSom = 0;
static bool  pastPublishedEco = false;

// ── Lógica de alertas ─────────────────────────────────────────
int  comandoAr = 0;
int  alertaSom = 0;
bool eco = false;

// ── Variáveis de controle de Sync ─────────────────────────────
bool syncRealizado = false;
bool syncRespondido = false;

// ── Controle de tempo e alertas ───────────────────────────────
const uint32_t intervaloPublicacaoMs = 10000;
uint32_t ultimaPublicacao = 0;

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
void aoConectarWiFi()    {debugInfo("WiFi conectado com sucesso! IP: " + WiFi.localIP().toString());}
void aoDesconectarWiFi() {debugAviso("Conexão WiFi perdida. Entrando em modo offline...");}
void aoConectarMQTT()    {debugInfo(">>> Conectado ao Broker/AWS com sucesso!");}
void aoDesconectarMQTT() {debugErro(">>> Conexão com a AWS interrompida.");}

// ── Setup ─────────────────────────────────────────────────────
void setup() 
{
    configurarDebug(DEBUG_NIVEL_INICIAL, PINO_HABILITA_DEBUG_COMPLETO);

    dht.begin();
    debugInfo("========== SENSOR DHT22 E KY038 ==========");
    debugInfo("Sensores inicializados");

    configTime(-10800, 0, "b.ntp.br");

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

    if(analise["alertaSom"].is<int>()){
        alertaSomOposto = analise["alertaSom"].as<int>();
    }

    if(analise["eco"].is<bool>()){
        ecoOposto = analise["eco"].as<bool>();
    }

    if(String(topico) == conectividade.topicoRecebimento(1)){
        debugInfo("===== DADOS SYNC RECEBIDOS =====");
        debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
        debugInfo("Umidade lado oposto: "     + String(umidadeOposto)     + "%");
        debugInfo("Ruido lado oposto: "       + String(ruidoOposto)       + "dB");
        debugInfo("ComandoAr lado oposto: "   + String(analise["comandoAr"].as<int>()));
        debugInfo("AlertaSom lado oposto: "   + String(analise["alertaSom"].as<int>()));
        debugInfo("Eco lado oposto: "         + String(analise["eco"].as<bool>() ? "true" : "false"));
        debugInfo("=============================================================");

        if(!syncRealizado){
            syncRealizado = true;
            ESPSync(); 
            debugInfo("Sincronização inicial realizada entre os ESPs.");
        }
        else if(!syncRespondido){
            syncRespondido = true;
            diferencaTemp();
            alertaSomEco();
            ESPSync();
            debugInfo("Sync recebido, lado oposto reiniciou. Respondendo com dados atuais.");
            debugInfo("===== ESTADO ATUAL APÓS RECALCULO =====");
            debugInfo("ComandoAr: " + String(comandoAr));
            debugInfo("AlertaSom: " + String(alertaSom));
            debugInfo("Eco: "       + String(eco ? "true" : "false"));
            debugInfo("=============================================================");
        }
        else{
            syncRespondido = false;
            debugInfo("Sync recebido novamente — aguardando próximo reset do lado oposto.");
        }
        return;
    }

    debugInfo("===== DADOS RECEBIDOS =====");
    if(analise["temperatura"].is<float>())
        debugInfo("Temperatura lado oposto: " + String(temperaturaOposto) + "°C");
    if(analise["umidade"].is<float>())
        debugInfo("Umidade lado oposto: " + String(umidadeOposto) + "%");
    if(analise["ruido"].is<float>())
        debugInfo("Ruido lado oposto: " + String(ruidoOposto) + "dB");
    if(analise["comandoAr"].is<int>())
        debugInfo("ComandoAr lado oposto: " + String(analise["comandoAr"].as<int>()));
    if(analise["alertaSom"].is<int>())
        debugInfo("AlertaSom lado oposto: " + String(alertaSomOposto));
    if(analise["eco"].is<bool>())
        debugInfo("Eco lado oposto: " + String(ecoOposto ? "true" : "false"));
    debugInfo("=============================================================");
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
    debugInfo("PUBLICANDO DADOS PARA SINCRONIZAÇÃO...");
    debugInfo("=================================================");
    debugInfo("Temperatura: " + String(valorTemperatura) + "°C");
    debugInfo("Umidade: "     + String(valorUmidade)     + "%");
    debugInfo("Ruido: "       + String(ruido)            + "dB");
    debugInfo("ComandoAr: "   + String(comandoAr));
    debugInfo("AlertaSom: "   + String(alertaSom));
    debugInfo("Eco: "         + String(eco ? "true" : "false"));
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
    // ── Configurações do sensor ──
    const unsigned long duracaoRuido = 300;
    const unsigned long duracaoEco = 900000;
    const int limiteSom = 70;

    // ── Variáveis de Memória Isoladas ──
    static unsigned long inicioRuido = 0;
    static unsigned long inicioSilencio = 0;
    static bool ativo = false;
    static bool silencio = false;
    static int alertaSomAnterior = -1;
    static bool ecoAnterior = false;

    unsigned long agora = millis();

    // Processamento do ruído local (Deste Lado)
    if(ruido >= limiteSom){
        silencio = false;
        if(!ativo){ 
            ativo = true; inicioRuido = agora; 
        }
    } 
    else{
        ativo = false; inicioRuido = 0;
        if(!silencio){ 
            silencio = true; inicioSilencio = agora; 
        }
    }

    bool alertaLocal = ativo && (agora - inicioRuido >= duracaoRuido);
    bool ecoLocal = silencio && (agora - inicioSilencio >= duracaoEco);

    // Cruzamento de estados (Local + Oposto)
    if(alertaLocal && (alertaSomOposto == 1 || alertaSomOposto == 3)){
        alertaSom = 3; 
    } 
    else if(alertaLocal){
        alertaSom = 1; 
    } 
    else if(mensagemRecebidaOposto && (alertaSomOposto == 1 || alertaSomOposto == 3)){
        alertaSom = 2; 
    } 
    else{
        alertaSom = 0;
    }

    // Modo economia ativo apenas se ambos os lados estiverem vazios
    if(mensagemRecebidaOposto){
        eco = ecoLocal && ecoOposto;
    } 
    else{
        eco = false;
    }

    // Logs de alteração no Monitor Serial
    if(alertaSom != alertaSomAnterior){
        alertaSomAnterior = alertaSom;
        debugInfo("===== ALERTA SOM / ECO =====");
        debugInfo("Ruido deste lado: "   + String(ruido)       + "dB");
        debugInfo("Ruido lado oposto: "  + String(ruidoOposto) + "dB");

        if(alertaSom == 0)      debugInfo("alertaSom = 0; Nível de ruído dentro dos limites de tolerância.");
        else if(alertaSom == 1) debugInfo("alertaSom = 1; Conversa alta persistente detectada neste lado.");
        else if(alertaSom == 2) debugInfo("alertaSom = 2; Conversa alta persistente detectada no lado oposto.");
        else                    debugInfo("alertaSom = 3; Conversa alta persistente detectada em ambos os lados.");
        
        debugInfo("=============================================================");
    }

    if(eco != ecoAnterior){
        ecoAnterior = eco;
        debugInfo(eco ? "Sala vazia - necessário ativar modo de economia." : "Sala não está vazia.");
        debugInfo("=============================================================");
    }
}

// ── Publicação Baseada em Delta ──────────
void publicarDadosAnalise() 
{
    unsigned long timestamp = time(nullptr);
    JsonDocument doc;
    JsonObject analise = doc["analise"].to<JsonObject>();

    debugInfo("==============================");
    debugInfo("VERIFICANDO MUDANÇAS...");
    debugInfo("==============================");

    bool alteracao = false;

    if(abs(valorTemperatura - pastPublishedTemperatura) >= 1){
        analise["temperatura"] = valorTemperatura;
        pastPublishedTemperatura = valorTemperatura;
        debugInfo("Temperatura mudou: " + String(valorTemperatura) + "°C");
        alteracao = true;
    }

    if(abs(valorUmidade - pastPublishedUmidade) >= 1){
        analise["umidade"] = valorUmidade;
        pastPublishedUmidade = valorUmidade;
        debugInfo("Umidade mudou: " + String(valorUmidade) + "%");
        alteracao = true;
    }
    
    if(abs(ruido - pastPublishedRuido) >= 1){
        analise["ruido"] = ruido;
        pastPublishedRuido = ruido;
        debugInfo("Ruido mudou: " + String(ruido) + "dB");
        alteracao = true;
    }

    if(comandoAr != pastPublishedComandoAr){
        analise["comandoAr"] = comandoAr;
        pastPublishedComandoAr = comandoAr;
        debugInfo("ComandoAr mudou: " + String(comandoAr));
        alteracao = true;
    }

    if(alertaSom != pastPublishedAlertaSom){
        analise["alertaSom"] = alertaSom;
        pastPublishedAlertaSom = alertaSom;
        debugInfo("AlertaSom mudou: " + String(alertaSom));
        alteracao = true;
    }

    if(eco != pastPublishedEco){
        analise["eco"] = eco;
        pastPublishedEco = eco;
        debugInfo("Eco mudou: " + String(eco ? "true" : "false"));
        alteracao = true;
    }
    
    if(alteracao){
        analise["timestamp"] = timestamp;
        debugInfo("timestamp: " + String(timestamp));
        debugInfo(">>> ENVIANDO PACOTE DELTA PARA A AWS <<<");
        debugInfo("=============================================================");
        
        char buffer[512];
        serializeJson(doc, buffer, sizeof(buffer));
        conectividade.publicar(0, buffer);
    } else {
        debugInfo("Nada mudou. Nenhuma mensagem enviada.");
        debugInfo("=============================================================");
    }
}