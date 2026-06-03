/**
 * @file ESP32Connectivity.h
 * @brief Gerenciamento não-bloqueante de WiFi e MQTT para ESP32.
 *
 * @details
 * Biblioteca independente — não depende de `secrets.h` do projeto.
 * Toda a configuração é passada via structs em `begin()`.
 *
 * @par Modos de conexão MQTT
 * | Modo | Quando usar |
 * |------|-------------|
 * | `ModoConexao::SIMPLES`  | Broker local sem criptografia |
 * | `ModoConexao::TLS`      | Broker público com TLS (HiveMQ, Mosquitto, etc.) |
 * | `ModoConexao::AWS_IOT`  | AWS IoT Core com mTLS (certificado de dispositivo) |
 *
 * @par Uso básico
 * @code
 * #include <ESP32Connectivity.h>
 *
 * // Preencha as structs com suas credenciais
 * ConfigWiFi wifi   = { "MinhaRede", "minha_senha" };
 * ConfigMQTT mqtt   = { "broker.exemplo.com", 8883, "esp32_id", "user", "pass" };
 * ConfigTLS  tls    = { meuCertificadoCA };
 * const char* pub[] = { "meu/topico/status" };
 * const char* rec[] = { "meu/topico/comando" };
 * ConfigTopicos topicos = { pub, 1, rec, 1 };
 *
 * void setup() {
 *     configurarDebug(DEBUG_INFO, 4);
 *     conectividade.begin(wifi, mqtt, ModoConexao::TLS, tls, topicos);
 * }
 *
 * void loop() {
 *     conectividade.update();
 *     conectividade.publicar(0, "hello");
 * }
 * @endcode
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @version 3.0.0
 * @license MIT
 */

#ifndef ESP32_CONNECTIVITY_H
#define ESP32_CONNECTIVITY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------------------------------------------------------------------------
// Configuração da fila offline
// Redefina ANTES do #include para personalizar sem alterar a biblioteca.
// ---------------------------------------------------------------------------

/** @brief Slots na fila offline (número de mensagens). */
#ifndef CONNECTIVITY_FILA_SLOTS
  #define CONNECTIVITY_FILA_SLOTS 10
#endif

/** @brief Tamanho máximo do tópico em cada slot (bytes). */
#ifndef CONNECTIVITY_FILA_TOPICO_MAX
  #define CONNECTIVITY_FILA_TOPICO_MAX 64
#endif

/** @brief Tamanho máximo do payload em cada slot (bytes). */
#ifndef CONNECTIVITY_FILA_PAYLOAD_MAX
  #define CONNECTIVITY_FILA_PAYLOAD_MAX 256
#endif

/**
 * @brief Habilita alocação da fila na PSRAM (ESP32 com PSRAM externa).
 *
 * Quando definido como 1, a fila é alocada via `ps_malloc()` na PSRAM,
 * liberando a SRAM interna para o restante do programa.
 *
 * Requer:
 *  - ESP32 com PSRAM (ex: N16R8, N8R8, N4R2)
 *  - `board_build.arduino.memory_type = qio_opi` no platformio.ini
 *  - `build_flags = -DBOARD_HAS_PSRAM` no platformio.ini
 *
 * @par platformio.ini para ESP32-S3 N16R8
 * @code
 * board_build.arduino.memory_type = qio_opi
 * board_upload.flash_size = 16MB
 * board_build.partitions = default_16MB.csv
 * build_flags =
 *     -DBOARD_HAS_PSRAM
 *     -DCONNECTIVITY_USAR_PSRAM=1
 * @endcode
 */
#ifndef CONNECTIVITY_USAR_PSRAM
  #define CONNECTIVITY_USAR_PSRAM 0
#endif

// ---------------------------------------------------------------------------
// Modo de conexão MQTT
// ---------------------------------------------------------------------------

/**
 * @brief Define o protocolo e nível de segurança da conexão MQTT.
 */
enum class ModoConexao : uint8_t {
    SIMPLES,   ///< MQTT sem criptografia (porta 1883 — apenas redes locais confiáveis)
    TLS,       ///< MQTT com TLS/SSL (porta 8883 — brokers públicos)
    AWS_IOT    ///< AWS IoT Core com mTLS — requer certificado de dispositivo (porta 8883)
};

/**
 * @brief Define o comportamento da fila quando o MQTT está offline.
 *
 * | Política | Quando usar |
 * |----------|-------------|
 * | `FILA_COMPLETA` | Dados históricos importantes — logs, eventos, alarmes |
 * | `APENAS_ULTIMO` | Só o estado atual importa — status, posição, modo |
 * | `DESCARTAR` | Dados em tempo real — temperatura, sensores contínuos |
 *
 * @par Exemplo
 * @code
 * // Sensor de temperatura: valor antigo não tem utilidade
 * conectividade.definirPoliticaFila(PoliticaFila::DESCARTAR);
 *
 * // Status do dispositivo: só o mais recente importa
 * conectividade.definirPoliticaFila(PoliticaFila::APENAS_ULTIMO);
 *
 * // Log de eventos: todos os eventos devem chegar
 * conectividade.definirPoliticaFila(PoliticaFila::FILA_COMPLETA); // padrão
 * @endcode
 */
enum class PoliticaFila : uint8_t {
    FILA_COMPLETA, ///< Enfileira todas as mensagens. Descarta a mais antiga quando cheia.
    APENAS_ULTIMO, ///< Mantém apenas a mensagem mais recente. Substitui se já houver uma.
    DESCARTAR      ///< Não enfileira. Descarta silenciosamente se offline.
};

// ---------------------------------------------------------------------------
// Structs de configuração
// ---------------------------------------------------------------------------

/**
 * @brief Credenciais WiFi.
 */
struct ConfigWiFi {
    const char* ssid;   ///< Nome da rede (SSID)
    const char* senha;  ///< Senha da rede
};

/**
 * @brief Configuração do broker MQTT.
 * Usada nos modos `SIMPLES` e `TLS`.
 */
struct ConfigMQTT {
    const char* broker;    ///< Endereço do broker (hostname ou IP)
    int         porta;     ///< Porta (1883 sem TLS, 8883 com TLS)
    const char* clientId;  ///< ID único deste dispositivo
    const char* usuario;   ///< Usuário (deixe "" para conexão anônima)
    const char* senha;     ///< Senha  (deixe "" para conexão anônima)
};

/**
 * @brief Certificado CA para modo `TLS`.
 * O certificado CA do broker em formato PEM.
 * Deixe `certificadoCA = ""` para usar `setInsecure()` (apenas testes).
 */
struct ConfigTLS {
    const char* certificadoCA;  ///< Certificado CA do broker em PEM
};

/**
 * @brief Certificados e configuração do AWS IoT Core (modo `AWS_IOT`).
 */
struct ConfigAWS {
    const char* endpoint;    ///< Endpoint AWS (xxxx.iot.região.amazonaws.com)
    int         porta;       ///< Porta (normalmente 8883)
    const char* clientId;    ///< Thing Name ou ID do dispositivo
    const char* certCA;      ///< Certificado raiz Amazon (AmazonRootCA1.pem)
    const char* certCRT;     ///< Certificado do dispositivo (.crt)
    const char* certPrivate; ///< Chave privada do dispositivo (.key)
};

/**
 * @brief Tópicos MQTT para publicação e recebimento.
 */
struct ConfigTopicos {
    const char** publicar;   ///< Array de tópicos de publicação
    int          nPublicar;  ///< Número de tópicos de publicação
    const char** receber;    ///< Array de tópicos de recebimento (inscrição)
    int          nReceber;   ///< Número de tópicos de recebimento
};

// ---------------------------------------------------------------------------
// Tipos de callback
// ---------------------------------------------------------------------------

typedef void (*CallbackMensagemMQTT)(const char* topico, const String& mensagem);
typedef void (*CallbackWiFi)(void);
typedef void (*CallbackMQTT)(void);

// ---------------------------------------------------------------------------
// Classe principal
// ---------------------------------------------------------------------------

/**
 * @brief Gerencia WiFi + MQTT de forma não-bloqueante com fila offline.
 */
class ESP32Connectivity
{
public:
    ESP32Connectivity() = default;

    // -----------------------------------------------------------------------
    // Inicialização
    // -----------------------------------------------------------------------

    /**
     * @brief Inicia no modo SIMPLES — sem criptografia.
     * Indicado para redes locais confiáveis. Porta padrão: 1883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginSimples(wifi, mqtt, topicos);
     * @endcode
     */
    void beginSimples(const ConfigWiFi& wifi,
                      const ConfigMQTT& mqtt,
                      const ConfigTopicos& topicos);

    /**
     * @brief Inicia no modo TLS — conexão criptografada.
     * Indicado para brokers públicos (HiveMQ, Mosquitto Cloud, etc.). Porta padrão: 8883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginTLS(wifi, mqtt, tls, topicos);
     * @endcode
     */
    void beginTLS(const ConfigWiFi& wifi,
                  const ConfigMQTT& mqtt,
                  const ConfigTLS& tls,
                  const ConfigTopicos& topicos);

    /**
     * @brief Inicia no modo AWS IoT Core — mTLS com certificado de dispositivo.
     * Porta padrão: 8883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginAWS(wifi, aws, topicos);
     * @endcode
     */
    void beginAWS(const ConfigWiFi& wifi,
                  const ConfigAWS& aws,
                  const ConfigTopicos& topicos);

    // -----------------------------------------------------------------------
    // Loop principal
    // -----------------------------------------------------------------------

    /**
     * @brief Avança as máquinas de estado e drena a fila offline.
     * **Deve ser chamado em todo `loop()`.**
     */
    void update();

    // -----------------------------------------------------------------------
    // Configuração da fila (chame antes do begin(), se necessário)
    // -----------------------------------------------------------------------

    /**
     * @brief Define a política de enfileiramento quando o MQTT está offline.
     *
     * Padrão: `PoliticaFila::FILA_COMPLETA`.
     * Pode ser alterada a qualquer momento, inclusive durante a execução.
     *
     * @param politica  `FILA_COMPLETA`, `APENAS_ULTIMO` ou `DESCARTAR`.
     *
     * @par Exemplo
     * @code
     * // Sensor contínuo — descarta se offline
     * conectividade.definirPoliticaFila(PoliticaFila::DESCARTAR);
     *
     * // Status — substitui pelo mais recente
     * conectividade.definirPoliticaFila(PoliticaFila::APENAS_ULTIMO);
     * @endcode
     */
    void definirPoliticaFila(PoliticaFila politica);

    /** @brief Retorna a política de fila atualmente configurada. */
    PoliticaFila obterPoliticaFila() const;

    /**
     * @brief Configura o tamanho do buffer interno do PubSubClient.
     *
     * Por padrão o PubSubClient limita mensagens a 256 bytes. Use este
     * método para aumentar o limite se publicar payloads maiores (ex: JSON
     * longos, leituras de sensores completas).
     *
     * @param tamanhoBytes  Tamanho máximo da mensagem MQTT em bytes.
     *                      Inclui cabeçalho + tópico + payload.
     *                      Padrão: 256. Recomendado máximo: 4096 no ESP32.
     *
     * @note Chame **antes** de `begin()`.
     *
     * @par Exemplo
     * @code
     * conectividade.configurarBufferMQTT(1024);   // mensagens até 1 KB
     * conectividade.begin(wifi, mqtt, topicos);
     * @endcode
     */
    void configurarBufferMQTT(uint16_t tamanhoBytes);

    // -----------------------------------------------------------------------
    // Estado
    // -----------------------------------------------------------------------

    /** @brief Retorna `true` se o WiFi está conectado. */
    bool wifiConectado() const;

    /** @brief Retorna `true` se o MQTT está conectado. */
    bool mqttConectado() const;

    /** @brief Retorna o número de mensagens na fila offline. */
    uint8_t mensagensNaFila() const;

    // -----------------------------------------------------------------------
    // Publicação
    // -----------------------------------------------------------------------

    /**
     * @brief Publica em um tópico pelo índice.
     *
     * @param indiceTopico  Índice em `ConfigTopicos::publicar[]`.
     * @param mensagem      Payload a publicar.
     * @param enfileirar    Se `true` (padrão), respeita a `PoliticaFila` configurada
     *                      quando offline. Se `false`, descarta a mensagem se offline,
     *                      independente da política.
     * @return `true` se publicado imediatamente; `false` se enfileirado ou descartado.
     *
     * @par Exemplo
     * @code
     * conectividade.publicar(0, "online");         // enfileira se offline (padrão)
     * conectividade.publicar(0, "ping", false);    // descarta se offline
     * @endcode
     */
    bool publicar(int indiceTopico, const char* mensagem, bool enfileirar = true);

    /**
     * @brief Publica em um tópico pelo nome completo.
     *
     * @param topico        Tópico MQTT completo.
     * @param mensagem      Payload a publicar.
     * @param enfileirar    Se `true` (padrão), respeita a `PoliticaFila` configurada.
     *                      Se `false`, descarta se offline.
     * @return `true` se publicado imediatamente; `false` se enfileirado ou descartado.
     */
    bool publicar(const char* topico, const char* mensagem, bool enfileirar = true);

    /**
     * @brief Publica com controle total de QoS e flag retained.
     *
     * @details
     * O PubSubClient suporta apenas **QoS 0** e **QoS 1**:
     * - `QoS 0` (at most once): dispara e esquece. Mais rápido, sem confirmação.
     * - `QoS 1` (at least once): o broker confirma o recebimento (PUBACK).
     *   A mensagem pode chegar mais de uma vez se o PUBACK se perder.
     * - `QoS 2` não é suportado pelo PubSubClient.
     *
     * A flag `retained` instrui o broker a guardar a última mensagem do tópico
     * e entregá-la imediatamente a novos assinantes.
     *
     * @param topico      Tópico MQTT completo.
     * @param mensagem    Payload a publicar.
     * @param qos         Nível de QoS: `0` ou `1`.
     * @param retained    `true` para mensagem retida no broker.
     * @param enfileirar  Se `true` (padrão), respeita a `PoliticaFila` se offline.
     * @return `true` se publicado imediatamente; `false` se enfileirado ou descartado.
     *
     * @par Exemplo
     * @code
     * // Status retained — novos assinantes recebem imediatamente
     * conectividade.publicarQoS("sala/status", "online", 1, true);
     *
     * // Leitura de sensor — QoS 0, sem retenção
     * conectividade.publicarQoS("sala/temperatura", "23.5", 0, false);
     * @endcode
     */
    bool publicarQoS(const char* topico, const char* mensagem,
                     uint8_t qos, bool retained, bool enfileirar = true);

    /**
     * @brief Define o QoS e retained padrão para todas as publicações.
     *
     * Evita repetir os parâmetros em cada chamada quando o projeto usa
     * sempre os mesmos valores.
     *
     * @param qos       Nível de QoS padrão: `0` ou `1`. (padrão: `0`)
     * @param retained  Flag retained padrão. (padrão: `false`)
     *
     * @par Exemplo
     * @code
     * conectividade.definirQoSPadrao(1, true);   // todas as publicações: QoS 1, retained
     * conectividade.publicar(0, "online");        // usa QoS 1 e retained automaticamente
     * @endcode
     */
    void definirQoSPadrao(uint8_t qos, bool retained);

    // -----------------------------------------------------------------------
    // Consulta de tópicos
    // -----------------------------------------------------------------------

    /** @brief Retorna o tópico de publicação pelo índice, ou `""` se inválido. */
    const char* topicoPublicacao(int indice) const;

    /** @brief Retorna o tópico de recebimento pelo índice, ou `""` se inválido. */
    const char* topicoRecebimento(int indice) const;

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    void registrarCallbackMensagem         (CallbackMensagemMQTT cb);
    void registrarCallbackWiFiConectado    (CallbackWiFi cb);
    void registrarCallbackWiFiDesconectado (CallbackWiFi cb);
    void registrarCallbackMQTTConectado    (CallbackMQTT cb);
    void registrarCallbackMQTTDesconectado (CallbackMQTT cb);

private:
    // -----------------------------------------------------------------------
    // Máquina de estados
    // -----------------------------------------------------------------------

    enum class EstadoWiFi : uint8_t { DESCONECTADO, CONECTANDO, CONECTADO };
    enum class EstadoMQTT : uint8_t { DESCONECTADO, CONECTANDO, CONECTADO };

    EstadoWiFi _estadoWiFi = EstadoWiFi::DESCONECTADO;
    EstadoMQTT _estadoMQTT = EstadoMQTT::DESCONECTADO;

    static constexpr uint32_t WIFI_TIMEOUT_MS       = 15000;
    static constexpr uint32_t WIFI_RETRY_INTERVALO  = 5000;
    static constexpr uint32_t MQTT_RETRY_INTERVALO  = 5000;
    static constexpr uint8_t  MQTT_MAX_TENTATIVAS   = 5;

    uint32_t _inicioConexaoWiFi  = 0;
    uint32_t _ultimaTentativaMQTT = 0;
    uint8_t  _tentativasMQTT     = 0;

    void _processarWiFi();
    void _processarMQTT();
    bool _tentarConectarMQTT();
    void _inscreverTopicos();

    // -----------------------------------------------------------------------
    // Configuração armazenada
    // -----------------------------------------------------------------------

    ModoConexao  _modo = ModoConexao::SIMPLES;
    ConfigWiFi   _wifi    = {};
    ConfigMQTT   _mqtt    = {};
    ConfigTLS    _tls     = {};
    ConfigAWS    _aws     = {};
    ConfigTopicos _topicos = {};
    PoliticaFila _politicaFila = PoliticaFila::FILA_COMPLETA;

    void _iniciarWiFi();
    void _configurarClienteMQTT();

    // -----------------------------------------------------------------------
    // Clientes de rede
    // -----------------------------------------------------------------------

    WiFiClient        _wifiCliente;
    WiFiClientSecure  _wifiClienteSeguro;
    PubSubClient      _mqttClient;

    // -----------------------------------------------------------------------
    // Fila offline (FIFO circular, tamanho fixo em compile-time)
    // -----------------------------------------------------------------------

    struct MensagemFila {
        char    topico  [CONNECTIVITY_FILA_TOPICO_MAX];
        char    payload [CONNECTIVITY_FILA_PAYLOAD_MAX];
        uint8_t qos     = 0;
        bool    retained = false;
        bool    ocupado  = false;
    };

    MensagemFila* _fila      = nullptr;
    uint16_t      _filaSlots = 0;
    uint16_t      _filaInicio  = 0;
    uint16_t      _filaFim     = 0;
    uint16_t      _filaTamanho = 0;

    // QoS e retained padrão (usados por publicar())
    uint8_t _qosPadrao      = 0;
    bool    _retainedPadrao = false;

    void _enfileirar(const char* topico, const char* payload,
                     uint8_t qos, bool retained);
    void _drenaFila();
    bool _publicarDireto(const char* topico, const char* payload,
                         uint8_t qos, bool retained);

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    CallbackMensagemMQTT _cbMensagem             = nullptr;
    CallbackWiFi         _cbWiFiConectado        = nullptr;
    CallbackWiFi         _cbWiFiDesconectado     = nullptr;
    CallbackMQTT         _cbMQTTConectado        = nullptr;
    CallbackMQTT         _cbMQTTDesconectado     = nullptr;

    static ESP32Connectivity* _instancia;
    static void _callbackMQTTEstatico(char* topico, byte* payload, unsigned int tamanho);
    void        _callbackMQTT(char* topico, byte* payload, unsigned int tamanho);
};

extern ESP32Connectivity conectividade;

#endif // ESP32_CONNECTIVITY_H