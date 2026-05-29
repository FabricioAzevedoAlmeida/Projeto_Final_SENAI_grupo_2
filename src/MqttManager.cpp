
//! MqttManager.cpp

#include <Arduino.h>

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// TODO: Talvez precisa do cpp
#include "secrets.h"
#include "WiFiManager.h"
#include "MqttManager.h"
#include "DebugManager.h"

//================Instancias===============
WiFiClient wifiCliente;
WiFiClientSecure wifiClienteSecure;
PubSubClient mqttClient(wifiCliente);

callbackMensagemMQTT callbackDaAplicacao = nullptr;

void registrarCallbackMensagem(callbackMensagemMQTT callback)
{
    callbackDaAplicacao = callback;

    if (callbackDaAplicacao != nullptr)
    {
        debugInfo("Callback da aplicação registrada com sucesso.");
    }

    else
    {
        debugErro("Callback da aplicação não foi registrada.");
    }
}

const char *obterTopicoPublicacao(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_PUBLICAR)
    {
        debugErro("Indice inválido para tópico de publicação: " + String(indiceTopico));
        return "";
    }

    return TOPICOS_PUBLICAR[indiceTopico];
}

const char *obterTopicoRecebimento(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_RECEBER)
    {
        debugErro("Indice inválido para tópico de recebimento: " + String(indiceTopico));
        return "";
    }

    return TOPICOS_RECEBER[indiceTopico];
}

void callbackInternoMQTT(char *topico, byte *payload, unsigned int tamanho)
{
    String mensagem = "";

    for (unsigned int i = 0; i < tamanho; i++)
    {
        mensagem += (char)payload[i];
    }

    debugInfo("========================");
    debugInfo(" Mensagem MQTT recebida");
    debugInfo("========================");
    debugInfo(" Topico: " + String(topico));
    debugInfo(" Mensagem: " + mensagem);

    if (callbackDaAplicacao != nullptr)
    {
        callbackDaAplicacao(topico, mensagem);
    }

    else
    {
        debugErro("Mensagem recebida, mas nenhum callback da aplicação foi registrado");
    }
}

void configurarMQTT()
{
    debugInfo("=====================");
    debugInfo(" Configuração MQTT...");
    debugInfo("=====================");

    if (USAR_AWS_IOT)
    {
        debugInfo("Modo selecionado: AWS IoT Core");
        debugInfo("Configurando certificados da AWs IoT Core.");

        wifiClienteSecure.setCACert(AWS_CERT_CA);
        wifiClienteSecure.setCertificate(AWS_CERT_CRT);
        wifiClienteSecure.setPrivateKey(AWS_CERT_PRIVATE);

        mqttClient.setClient(wifiClienteSecure);
        mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);

        debugInfo("Endpoint AWS IoT: " + String());
        debugInfo("Porta AWS IoT: " + String());
    }

    else if (MQTT_USAR_TLS)
    {
        debugInfo("Modo selecionado: MQTT com TLS. ");

        if (strlen(MQTT_CERTIFICADO_CA) > 100)
        {
            debugInfo("Certificado CA do broker MQTT configurado.");
            wifiClienteSecure.setCACert(MQTT_CERTIFICADO_CA); // ira verificar o endereço do servidor para saber se realmente e ele e verdadeiro
        }

        else
        {
            debugErro("Certificado CA do MQTT não configurado.");
            wifiClienteSecure.setInsecure();
        }
        mqttClient.setClient(wifiClienteSecure);
        mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT: " + String(MQTT_BROKER));
        debugInfo("Porta MQTT: " + String(MQTT_PORTA));
    }

    else // Conectar no broker sem certificado
    {
        debugInfo("Modo selecionado: MQTT sem TLS.");

        mqttClient.setClient(wifiCliente);
        mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT: " + String(MQTT_BROKER));
        debugInfo("Porta MQTT: " + String(MQTT_PORTA));
    }

    mqttClient.setCallback(callbackInternoMQTT);
    debugInfo("Callback interno do MQTT configurado.");
}

void conectarMQTT()
{
    if (!wifiConectado())
    {
        debugErro("MQTT não pode conectar porque o WiFi está desconectado.");
        return;
    }

    debugInfo("=========================");
    debugInfo("Iniciando conexão MQTT...");
    debugInfo("=========================");

    int tentativasMQTT = 0;
    const int maxTentativasMQTT = 5;

    while (!mqttClient.connected() && tentativasMQTT < maxTentativasMQTT)
    {
        debugInfo("Tentando conectar ao broker MQTT. Tentativa: " + String(tentativasMQTT));

        bool conectado = false;

        if (USAR_AWS_IOT)
        {
            conectado = mqttClient.connect(AWS_IOT_CLIENT_ID);
        }

        else
        {
            if (strlen(MQTT_USUARIO) > 0)
            {
                debugInfo("Conectando MQTT com usuário e senha.");

                conectado = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA);
            }

            else // Sem usuario
            {
                debugInfo("Conectando MQTT sem o usuário e senha.");

                conectado = mqttClient.connect(MQTT_CLIENT_ID);
            }
        }
        if (conectado)
        {
            debugInfo("MQTT conectado com sucesso.");

            int totalTopicos = obterTotalTopicosRecebimento();

            debugInfo("Total de tópicos para inscrição: " + String(totalTopicos));

            for (int i = 0; i < totalTopicos; i++)
            {
                const char *topico = obterTopicoRecebimento(i);

                bool inscrito = mqttClient.subscribe(topico);

                if (inscrito)
                {
                    debugInfo("Inscrito no tópico: " + String(topico));
                }

                else
                {
                    debugErro("Falha ao se inscrever no tópico: " + String(topico));
                }
            }
            // TODO: Publicar uma mensagem em um tópico informando que o esp foi conectado.
        }
        else
        {
            debugErro("Falha ao conectar no MQTT. Código de erro: " + String(mqttClient.state()));

            tentativasMQTT++;
            delay(2000);
        }
    } // Fim do While

    if (!mqttClient.connected())
    {
        debugErro("Não foi possivel conectar ao broker MQTT após " + String(maxTentativasMQTT) + "tentativas");
    }
}

void garantirMQTTConectado()
{
    static unsigned long lastRunOfFunction = 0;
    if (millis() - lastRunOfFunction >= 2000)
    {
        if (!wifiConectado())
        {
            debugErro("MQTT não será reconectado porque o WiFi está desconectado.");
            return;
        }

        if (!mqttClient.connected())
        {
            debugErro("MQTT desconectado. Tentando reconectar...");
            conectarMQTT();
        }
    }
    lastRunOfFunction = millis();
}

void MQTTLoop()
{
    mqttClient.loop();
}

void publicarMensagem(const char *topico, const char *mensagem)
{
    if (!mqttClient.connected())
    {
        debugErro("Não foi possivel publicar. MQTT desconectado.");
        return;
    }

    bool publicado = mqttClient.publish(topico, mensagem);

    if (publicado)
    {
        debugInfo("Mensagem publicada via MQTT.");
        debugInfo("Topico: " + String(topico));
        debugInfo("Mensagem: " + String(mensagem));
    }

    else
    {
        debugErro("Falha ao publicar mensagem no tópico: " + String(topico));
    }
}

void publicarMensagemNoTopico(int indiceTopico, const char *mensagem)
{
    const char *topico = obterTopicoPublicacao(indiceTopico);

    if (strlen(topico) == 0)
    {
        debugErro("Não foi possivel publicar. Indice de topico inválido : " + String(indiceTopico));
        return;
    }
    publicarMensagem(topico, mensagem);
}

bool mqttEstaConectado()
{
    return mqttClient.connected();
}

int obterTotalTopicosRecebimento()
{
    return TOTAL_TOPICOS_RECEBER;
}