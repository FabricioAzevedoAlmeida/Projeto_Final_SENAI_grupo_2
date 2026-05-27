#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include<Arduino.h>

void configurarMQTT();
void conectarMQTT();
void garantirMQTTConectado();
void MQTTLoop();

void publicarMensagem(const char* topico, const char* mensagem);
void publicarMesagemNoTopico(int indiceTopico, const char* mensagem);

bool mqttestaConectado();

const char* obterTopicoPublicacao(int indiceTopico);
const char* obterTopicoRecebimento(int indiceTopico);
int  obterTotalTopicosRecebimentos();

typedef void (*callbackMensagemMQTT)(const char* topico, const String& mensagem);

void registrarCallbackMensagem(callbackMensagemMQTT callback);

#endif