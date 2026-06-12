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
- [Fluxo de funcionamento](#fluxo-de-funcionamento)
- [Constantes de comportamento](#constantes-de-comportamento)
- [Lógica de análise](#lógica-de-análise)
- [Sincronização entre ESPs](#sincronização-entre-esps)
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
| `KY038`              | ≥ 1.0         | Biblioteca utilizada para a configuração do sensor de ruído |

---

## Instalação e configuração

**1. Clone o repositório**

```bash
git clone https://github.com/FabricioAzevedoAlmeida/Projeto_Final_SENAI_grupo_2.git
cd Projeto_Final_SENAI_grupo_2
```

**2. Instale a biblioteca ConectividadeESP32 e KY038**

Via PlatformIO — adicione ao `platformio.ini`:

```ini
lib_deps =
    https://github.com/professorThiago/ESP32Connectivity
    adafruit/DHT sensor library @ ^1.4.6
    adafruit/Adafruit Unified Sensor @ ^1.1.14
    https://github.com/bblanchon/ArduinoJson
    https://github.com/wKaelzx/Sensor-ruido
```

> A classe `ESP32Connectivity` (v3.0.0) é uma **biblioteca externa** instalada via `lib_deps` — veja [ConectividadeESP32](https://github.com/professorThiago/ConectividadeESP32).

> A classe `KY038` é uma **biblioteca externa** instalada via `lib_deps` — veja [KY038](https://github.com/wKaelzx/Sensor-ruido).

**3. Crie o arquivo `secrets.cpp`**

Este arquivo **não está versionado** (adicione ao `.gitignore`). Crie-o na pasta `src/`
com o seguinte conteúdo:

```cpp
// include/secrets.h

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
```

**4. Compile e faça upload**

**5. Configure o segundo ESP32**

Repita o processo alterando `MQTT_CLIENT_ID`, `TOPICOS_PUBLICAR` e `TOPICOS_RECEBER` para os valores do lado B.

---

## Estrutura do projeto

```
.
├── src/
│   ├── main.cpp         # Setup, loop e lógica principal de análise 
│   └── secrets.cpp      # ⚠️ NÃO versionar — credenciais locais
├── include/
│   └──  secrets.h       # É uma biblioteca que facilita a configuração do credenciais
└── README.md
```

---

## Fluxo de funcionamento

```
setup()
├── Inicializa DHT22 e KY-038
├── Configura callbacks de WiFi e MQTT (Buffer alocado: 1024 bytes para JSON)
├── Conecta ao AWS IoT Core (Porta Segura 8883 via TLS/SSL)
├── 🔄 Inicialização Assíncrona: Libera o avanço imediato para o loop principal.
│   As conexões Wi-Fi, MQTT e a primeira sincronização do relógio via NTP ocorrem totalmente em segundo plano (background),
│   garantindo que o processamento local dos sensores funcione mesmo se o dispositivo iniciar sem internet.
├── Faz leitura inicial dos sensores
└── Executa ESPSync() — publica estado inicial no tópico de sync

loop()  [não-bloqueante, executa continuamente via millis()]
├── conectividade.update() — gerencia WiFi/MQTT in background
├── Lê ruído via KY-038 (getPercentage com amostragem de 100 leituras contínuas)
├── ⏱️ Watchdog de Timeout: Se o lado oposto sumir por > 30s, limpa a memória por segurança
├── diferencaTemp()         — calcula comandoAr com base no desvio térmico absoluto
├── alertaSomEco()          — avalia alertaSom e modo ECO combinados
└── A cada 10 s: lê DHT22 e chama publicarDadosAnalise()

aoReceberMensagem()  [callback MQTT assíncrono]
├── 🛡️ Filtro Antifolha: Rejeita mensagens nos primeiros 5s de boot (descarta retained messages)
├── Valida formato (rejeita strings corrompidas ou pacotes que não iniciem em "{")
├── 🔍 Tipagem Segura via is<float>(): Ignora o campo se o JSON vier incompleto, evitando zerar a memória local
├── Atualiza variáveis do lado oposto (temperatura, umidade, ruído, alertaSom, eco)
└── Se for tópico de sync: executa handshake de sincronização tripla (Reset/Sync) entre ESPs
```

---

## Constantes de comportamento

Estas constantes estão definidas diretamente no `main.cpp` e controlam o comportamento do sistema em tempo real:

| Constante | Valor | Descrição |
|-----------|-------|-----------|
| `intervaloPublicacaoMs` | `10000` ms | Intervalo entre cada ciclo de leitura e publicação |
| `CONNECTIVITY_FILA_SLOTS` | `15` | Máximo de mensagens na fila offline |
| `CONNECTIVITY_FILA_PAYLOAD_MAX` | `512` bytes | Tamanho máximo de cada mensagem na fila |
| `limiteSom` | `70` % | Limiar de atividade sonora para acionar alerta |
| `duracaoRuido` | `300` ms | Tempo mínimo de ruído contínuo para confirmar alerta |
| `duracaoEco` | `900000` ms (15 min) | Tempo de silêncio para ativar modo ECO |
| Limite temperatura | `4` °C | Diferença mínima entre lados para acionar `comandoAr` |
| Limite publicação | `1` unidade | Variação mínima em qualquer campo para disparar publicação |

---

## Lógica de análise

### Desequilíbrio térmico (`diferencaTemp`)

Compara `valorTemperatura` (local) com `temperaturaOposto` (recebido via MQTT). A função só é executada após receber pelo menos uma mensagem do lado oposto.

| Condição | `comandoAr` |
|----------|-------------|
| Diferença < 4 °C | `0` — equilibrado |
| Local > oposto em ≥ 4 °C | `1` — este lado mais quente |
| Oposto > local em ≥ 4 °C | `2` — lado oposto mais quente |

### Alerta de som e modo ECO (`alertaSomEco`)

O ruído local é amostrado continuamente via `sensor.getPercentage(100)`. O alerta só é confirmado após `duracaoRuido` (300 ms) consecutivos acima de `limiteSom` (70%). O cruzamento com o estado do lado oposto determina o valor final de `alertaSom`:

| Situação | `alertaSom` |
|----------|-------------|
| Nenhum lado com alerta | `0` |
| Apenas este lado | `1` |
| Apenas lado oposto | `2` |
| Ambos os lados | `3` |

O modo ECO (`eco = true`) é ativado somente quando **ambos** os lados estão em silêncio por ≥ 15 min.

### Publicação baseada em delta

`publicarDadosAnalise()` só inclui no JSON os campos que variaram ≥ 1 unidade desde a última publicação. Campos sem variação são omitidos, reduzindo o tráfego MQTT. O `timestamp` é adicionado somente se ao menos um campo foi alterado.

---

## Sincronização entre ESPs

Ao iniciar, cada ESP32 publica seus dados no tópico de sync (`sala/X/sync`) e aguarda a resposta do lado oposto. O handshake funciona da seguinte forma:

1. ESP A sobe e publica sync → ESP B ainda não está online, mensagem fica na fila.
2. ESP B sobe, publica seu sync → ESP A recebe, marca `syncRealizado = true` e responde com sync.
3. ESP B recebe o sync de A → ambos têm os dados iniciais do lado oposto antes do primeiro ciclo de publicação regular.

Caso os dois ESPs subam simultaneamente, um mecanismo de debounce (2000 ms) evita eco de sync.

## 🛡️ Resiliência Industrial e Tolerância a Falhas

O firmware foi projetado seguindo preceitos de sistemas embarcados robustos e tolerantes a falhas reais de infraestrutura de campo:

1. **Watchdog de Timeout de Comunicação (30 segundos):** Caso um dos ESP32 sofra uma queda abrupta de energia ou queime, o broker MQTT não notifica o parceiro de forma ativa. Para evitar que o ESP ativo tome decisões automatizadas com base em "dados congelados/fantasmas", o loop monitora a janela temporal de inatividade do vizinho. Passados 30 segundos sem novas mensagens válidas, a memória local do parceiro é zerada e o modo cruzado é desativado preventivamente.
2. **Buffer Estático de Fila Offline:** Em caso de queda de rede, o chip armazena até 15 payloads JSON na memória RAM. Para evitar a poluição do banco de dados na nuvem com a data padrão de 1970 (caso o dispositivo seja ligado já sem internet), a geração e o armazenamento das mensagens na fila aguardam de forma inteligente a validação do primeiro sincronismo de hora via NTP em background.
3. **Filtro de Simultaneidade Antiloop:** Caso ambos os microcontroladores reiniciem exatamente no mesmo milissegundo, uma trava lógica de 2000 ms impede que o envio automático de pacotes `ESPSync()` gere um loop infinito de ecos de rede e sobrecarregue o processamento dos chips.

---


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

| Pino        | Nível ativo   | O que é exibido                            |
|-------------|---------------|--------------------------------------------|
| HIGH (pull-up) | `DEBUG_TUDO` | INFO, AVISO, VERBOSE, TUDO + ERRO         |
| LOW            | `DEBUG_ERRO` | Apenas mensagens de erro                  |

---

## 👥 Grupo

- [Alisson Almeida Gomes](https://github.com/alissonalmeida-dev7)
- [Fabricio Azevedo Almeida](https://github.com/FabricioAzevedoAlmeida)
- [Heloísa Tomé de Araujo](https://github.com/hyopsywan)
- [Kael Fontes Araujo](https://github.com/wKaelzx)
- [Luis Otávio Coelho Ferreira](https://github.com/luisoferreira)
- [Victor Bueno](https://github.com/Vbueno04)