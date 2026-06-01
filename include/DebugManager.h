#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <Arduino.h>

#define DEBUG_NENHUM 0
#define DEBUG_ERRO 1
#define DEBUG_TUDO 2
#define DEBUG_AVISO 3
#define DEBUG_VERBOSE 4
#define DEBUG_INFO 5

void configurarDebug();

void debugErro(const String& mensagem);
void debugInfo(const String& mensagem);
void debugAviso(const String& mensagem);
void debugVerbose(const String& mensagem);
void debugTudo(const String& mensagem);
void debugNenhum(const String& mensagem);

void debugErroSemLinha(const String& mensagem);
void debugInfoSemLinha(const String& mensagem);

int obterNivelDebugAtual();

#endif