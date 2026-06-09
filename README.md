# 🌡️ Projeto final Módulo de análise

Sistema embarcado para monitoramento ambiental em tempo real com dois ESP32 comunicando-se via MQTT. Coleta temperatura, umidade e nível de ruído de ambos os lados de um ambiente, detecta desequilíbrios térmicos e emite alertas de som — tudo de forma não-bloqueante.

---

## 📋 Índice

- [Visão Geral](#visão-geral)
- [Funcionalidades](#funcionalidades)
- [Hardware necessário](#hardware-necessário)
- [Dependências](#dependências)
- [Instalação e configuração](#instalação-e-configuração)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Exemplos de uso](#exemplos-de-uso)
- [Payload MQTT](#payload-mqtt)
- [Níveis de debug](#níveis-de-debug)

---

## Visão Geral

O projeto consiste em **dois ESP32** instalados em lados opostos de um ambiente (ex: uma sala de aula). Cada unidade lê seus próprios sensores, publica os dados via MQTT e consome os dados do ESP32 oposto para executar análises combinadas:

- **Desequilíbrio térmico**: detecta diferença de temperatura ≥ 4 °C entre os lados e indica qual lado está mais quente.
- **Alerta de ruído**: identifica conversa alta persistente (≥ 70% de atividade sonora por 300 ms) em um ou ambos os lados.
- **Modo ECO**: detecta silêncio prolongado (15 minutos) nos dois lados, sinalizando que a sala está vazia.

A conectividade é gerenciada pela classe `ESP32Connectivity`, que implementa máquinas de estado não-bloqueantes para WiFi e MQTT, fila offline de mensagens e suporte a TLS e AWS IoT Core.

---

## Funcionalidades

- Leitura de temperatura e umidade via **DHT22**
- Leitura de nível de ruído via **sensor KY-038** (microfone digital)
- Comunicação entre dois ESP32 via **MQTT** com sincronização inicial automática
- Publicação somente quando há variação significativa (≥ 1 unidade), evitando tráfego desnecessário
- Fila offline: mensagens geradas sem conexão são armazenadas e enviadas ao reconectar
- Suporte a três modos de conexão: MQTT simples, MQTT com TLS e AWS IoT Core
- Sistema de debug configurável por pino físico (sem necessidade de recompilar)
- Sincronização de horário via NTP (`b.ntp.br`)

---

## Hardware necessário

| Componente        | Quantidade | Observação                          |
|-------------------|-----------|--------------------------------------|
| ESP32             | 2         | Qualquer variante com WiFi           |
| Sensor DHT22      | 2         | Temperatura e umidade                |
| Sensor KY-038     | 2         | Microfone digital — saída digital    |
| Broker MQTT       | 1         | Local (ex: Mosquitto) ou na nuvem    |

**Conexões padrão** (definidas em `main.cpp`):

| Pino | Função        |
|------|---------------|
| 8    | DHT22 (dados) |
| 9    | KY-038 (D0)   |

---

## Dependências

| Biblioteca            | Versão testada | Finalidade                              |
|----------------------|---------------|-----------------------------------------|
| [ConectividadeESP32](https://github.com/professorThiago/ConectividadeESP32) | v3.0.0 | Gerenciamento não-bloqueante de WiFi + MQTT |
| `DHT sensor library` | ≥ 1.4         | Leitura do DHT22                        |
| `ArduinoJson`        | ≥ 7.x         | Serialização do payload MQTT            |
| `PubSubClient`       | ≥ 2.8         | Cliente MQTT (dependência da biblioteca acima) |
| `WiFi` (ESP32)       | built-in      | Conectividade WiFi                      |

---

## Instalação e configuração

**1. Clone o repositório**

```bash
git clone https://github.com/FabricioAzevedoAlmeida/Projeto_Final_SENAI_grupo_2.git
cd Projeto_Final_SENAI_grupo_2
```

**2. Instale a biblioteca ConectividadeESP32**

Via PlatformIO — adicione ao `platformio.ini`:

```ini
lib_deps =
    https://github.com/professorThiago/ConectividadeESP32.git
    adafruit/DHT sensor library
    bblanchon/ArduinoJson
    knolleary/PubSubClient
```

**3. Crie o arquivo `secrets.h`**

Este arquivo **não está versionado** (adicione ao `.gitignore`). Crie-o na pasta `src/` ou `include/` com o seguinte conteúdo:

```cpp
// secrets.h

// ── WiFi ─────────────────────────────────
#define WIFI_SSID     "sua-rede"
#define WIFI_SENHA    "sua-senha"

// ── MQTT ─────────────────────────────────
#define MQTT_BROKER      "seu-broker"
#define MQTT_PORTA       8883
#define MQTT_CLIENT_ID   "esp32-sala-A"
#define MQTT_USUARIO     ""   // deixe vazio se não usar autenticação
#define MQTT_SENHA       ""

// ── Tópicos ──────────────────────────────
#define TOTAL_TOPICOS_PUBLICAR  2
#define TOTAL_TOPICOS_RECEBER   2
const char* TOPICOS_PUBLICAR[] = { "sala/A/analise", "sala/A/sync" };
const char* TOPICOS_RECEBER[]  = { "sala/B/analise", "sala/B/sync" };

// ── Modos opcionais ───────────────────────
#define USAR_AWS_IOT     false
#define MQTT_USAR_TLS    false
#define MQTT_CERTIFICADO_CA ""

// ── Debug ────────────────────────────────
#define PINO_HABILITA_DEBUG_COMPLETO  0   // GPIO com pull-up; LOW = debug completo
#define DEBUG_NIVEL_INICIAL           DEBUG_ERRO

// ── AWS IoT (preencha somente se USAR_AWS_IOT = true) ─────────
#define AWS_IOT_ENDPOINT  ""
#define AWS_IOT_PORT      8883
#define AWS_IOT_CLIENT_ID ""
#define AWS_CERT_CA       ""
#define AWS_CERT_CRT      ""
#define AWS_CERT_PRIVATE  ""
```

**3. Compile e faça upload**

**4. Configure o segundo ESP32**

Repita o processo alterando `MQTT_CLIENT_ID`, `TOPICOS_PUBLICAR` e `TOPICOS_RECEBER` para os valores do lado B.

---

## Estrutura do projeto

```
.
├── src/
│   ├── main.cpp              # Setup, loop e lógica principal de análise
│   ├── ESP32Connectivity.cpp # Gerenciador não-bloqueante de WiFi + MQTT (v3.0.0)
│   ├── WiFiManager.cpp       # Abstração de conexão WiFi 
│   ├── MqttManager.cpp       # Abstração de cliente MQTT 
│   ├── KY038.cpp             # Driver do sensor de som KY-038
│   └── DebugManager.cpp      # Sistema de log por níveis via Serial
├── include/
│   ├── secrets.h             # ⚠️ NÃO versionar — credenciais locais
│   ├── DebugManager.h        # ferramenta usada para identificar e analisar erros (Bugs) em um programa 
│   ├── ESP32Connectivity.h   # Gerenciamento não-bloqueante de WiFi e MQTT para ESP32.
│   ├── KY038.h               # Biblioteca utilizada para a configuração do sensor de ruído 
│   ├── MqttManager.h         # É um arquivo de cabeçalho para organizar o código relacionado ao MQTT 
│   └── WiFiManager.h         # É uma biblioteca que facilita a configuração do Wi-Fi
└── README.md
```

> A classe `ESP32Connectivity` (v3.0.0) é uma **biblioteca externa** instalada via `lib_deps` — veja [ConectividadeESP32](https://github.com/professorThiago/ConectividadeESP32).

---

## Payload MQTT

As mensagens são publicadas em JSON no seguinte formato:

```json
{
  "analise": {
    "temperatura": 24.5,
    "umidade": 60.0,
    "ruido": 35.0,
    "comandoAr": 1,
    "alertaSom": 0,
    "eco": false,
    "timestamp": 1718000000
  }
}
```

Campos omitidos quando a variação é inferior a 1 unidade desde a última publicação.

| Campo        | Tipo    | Descrição                                                               |
|-------------|---------|-------------------------------------------------------------------------|
| `temperatura` | float | °C medidos pelo DHT22                                                   |
| `umidade`    | float   | % medida pelo DHT22                                                     |
| `ruido`      | float   | % de atividade sonora (0–100)                                           |
| `comandoAr`  | int     | `0` = equilibrado · `1` = este lado mais quente · `2` = lado oposto mais quente |
| `alertaSom`  | int     | `0` = normal · `1` = alerta neste lado · `2` = alerta no oposto · `3` = ambos |
| `eco`        | bool    | `true` = sala vazia (silêncio ≥ 15 min nos dois lados)                  |
| `timestamp`  | long    | Unix timestamp sincronizado via NTP                                     |

---

## Níveis de debug

O nível é definido em tempo de execução pelo pino `PINO_HABILITA_DEBUG_COMPLETO`:

| Pino        | Nível ativo   | O que é exibido                            |
|-------------|---------------|--------------------------------------------|
| HIGH (pull-up) | `DEBUG_TUDO` | INFO, AVISO, VERBOSE, TUDO + ERRO         |
| LOW            | `DEBUG_ERRO` | Apenas mensagens de erro                  |

Para alterar o nível via código:

```cpp
nivelDebugAtual = DEBUG_TUDO;  // ou DEBUG_ERRO
```

---

## 👥Grupo

Alisson Almeida Gomes
Fabricio Azevedo Almeida
Heloísa Tomé de Araujo
Kael Fontes Araujo
Luis Otávio Coelho ferreira
Victor Bueno
