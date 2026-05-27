#ifndef SECRETS_H
#define SECRETS_H
extern const char* WIFI_SSID;
extern const char* WIFI_SENHA;

extern const bool USAR_AWS_IOT;

extern const char* MQTT_BROKER;
extern const int MQTT_PORTA;

extern const char* MQTT_CLIENT_ID;

extern const char* MQTT_USUARIO;
extern const char* MQTT_SENHA;

extern const bool MQTT_USAR_TLS; 

extern const char MQTT_CERTIFICADO_CA[];

extern const char* TOPICOS_PUBLICAR[];

extern const int TOTAL_TOPICOS_PUBLICAR;

extern const char* TOPICOS_RECEBER[];

extern const int TOTAL_TOPICOS_RECEBER;

extern const int DEBUG_NIVEL_INICIAL;

extern const int PINO_HABILITA_DEBUG_COMPLETO;

extern const char AWS_CERT_CA [];

extern const char AWS_CERT_CRT [];

extern const char  AWS_CERT_PRIVATE [];

extern const char* AWS_IOT_ENDPOINT;

extern const int AWS_IOT_PORT;

extern const char* AWS_IOT_CLIENT_ID;

#endif 