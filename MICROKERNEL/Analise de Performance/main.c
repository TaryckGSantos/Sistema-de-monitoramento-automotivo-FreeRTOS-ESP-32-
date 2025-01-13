#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include <time.h>
#include <string.h>
#include "esp_log.h"

// Definições de GPIOs para sensores e atuadores
#define TOUCH_INJECAO TOUCH_PAD_NUM0 // GPIO4
#define TOUCH_TEMPERATURA TOUCH_PAD_NUM8 // GPIO0
#define TOUCH_ABS TOUCH_PAD_NUM9 // GPIO32
#define TOUCH_AIRBAG TOUCH_PAD_NUM3 // GPIO15
#define TOUCH_CINTO TOUCH_PAD_NUM4 // GPIO13

#define GPIO_ATUADOR_INJECAO GPIO_NUM_18
#define GPIO_ATUADOR_TEMPERATURA GPIO_NUM_19
#define GPIO_ATUADOR_ABS GPIO_NUM_21
#define GPIO_ATUADOR_AIRBAG GPIO_NUM_22
#define GPIO_ATUADOR_CINTO GPIO_NUM_23

// Deadlines em microssegundos
#define DEADLINE_INJECAO 500 // 500 us
#define DEADLINE_TEMPERATURA 20000 // 20 ms
#define DEADLINE_ABS 100000 // 100 ms
#define DEADLINE_AIRBAG 100000 // 100 ms
#define DEADLINE_CINTO 1000000 // 1 segundo

// Parâmetros do (m,k)-firme
#define JANELA_ATIVACOES 10 // k: número de ativações na janela
#define M_ATIVACOES 10

// Número de amostras e configuração do timer
#define NUM_AMOSTRAS 200
int vetor_velocidade[NUM_AMOSTRAS] = {0};
int vetor_consumo[NUM_AMOSTRAS] = {0};
int indice = 0;

#define TIMER_DIVIDER 80 // Divisor de clock para precisão em microssegundos
#define TIMER_BASE_CLK 80000000 // Clock base do timer
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // Escala do timer para conversão de microssegundos

// Variáveis globais para armazenar os valores de WCRT e WCET
volatile uint64_t WCRT_injecao = 0;
volatile uint64_t WCRT_temperatura = 0;
volatile uint64_t WCRT_abs = 0;
volatile uint64_t WCRT_airbag = 0;
volatile uint64_t WCRT_cinto = 0;

volatile uint64_t WCET_injecao = 0;
volatile uint64_t WCET_temperatura = 0;
volatile uint64_t WCET_abs = 0;
volatile uint64_t WCET_airbag = 0;
volatile uint64_t WCET_cinto = 0;

// Variáveis globais para armazenar os valores de HWM e LWM
volatile uint64_t HWM_injecao = 0;
volatile uint64_t HWM_temperatura = 0;
volatile uint64_t HWM_abs = 0;
volatile uint64_t HWM_airbag = 0;
volatile uint64_t HWM_cinto = 0;

// Variáveis globais para armazenar os valores de HWM e LWM
volatile uint64_t LWM_injecao = UINT64_MAX;
volatile uint64_t LWM_temperatura = UINT64_MAX;
volatile uint64_t LWM_abs = UINT64_MAX;
volatile uint64_t LWM_airbag = UINT64_MAX;
volatile uint64_t LWM_cinto = UINT64_MAX;

// Variáveis globais para contagem de ativações
int cont_ativacoes_injecao = 0;
int cont_ativacoes_temperatura = 0;
int cont_ativacoes_abs = 0;
int cont_ativacoes_airbag = 0;
int cont_ativacoes_cinto = 0;

// Variáveis globais para contagem de sucesso na deadline
int cont_dl_injecao = 0;
int cont_dl_temperatura = 0;
int cont_dl_abs = 0;
int cont_dl_airbag = 0;
int cont_dl_cinto = 0;

// Variáveis globais para indicar o último skip
int ultimo_skip_injecao = 0;
int ultimo_skip_temperatura = 0;
int ultimo_skip_abs = 0;
int ultimo_skip_airbag = 0;
int ultimo_skip_cinto = 0;

static const char *TAG = "Touch pad";

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

static bool s_pad_activated[TOUCH_PAD_MAX];
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];

// Função para atualizar as amostras de velocidade e consumo
void atualizar_amostras() {
    int nova_velocidade = rand() % 150;
    int novo_consumo = rand() % 20;
    vetor_velocidade[indice] = nova_velocidade;
    vetor_consumo[indice] = novo_consumo;
    indice = (indice + 1) % NUM_AMOSTRAS;
}

// Função para calcular a média de amostras
float calcular_media(int* vetor) {
    int soma = 0;
    for (int i = 0; i < NUM_AMOSTRAS; i++) {
        soma += vetor[i];
    }
    return (float)soma / NUM_AMOSTRAS;
}

// Configuração do Timer
void configurar_timer() {
    timer_config_t config = {
        .divider = TIMER_DIVIDER, // Configura o divisor de clock
        .counter_dir = TIMER_COUNT_UP, // Configura o timer para contagem crescente
        .counter_en = TIMER_PAUSE, // Inicia o timer em pausa
        .alarm_en = TIMER_ALARM_DIS, // Desabilita alarmes
        .auto_reload = false, // Desabilita recarga automática
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config); // Inicializa o timer com a configuração
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL); // Zera o valor do timer
    timer_start(TIMER_GROUP_0, TIMER_0); // Inicia o timer
}

// Função para obter o tempo em microssegundos usando o driver de timer
uint64_t obter_tempo() {
    uint64_t timer_val; // Variável para armazenar o valor do timer
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &timer_val); // Obtém o valor do contador
    return timer_val / (TIMER_BASE_CLK / TIMER_DIVIDER / 1000000); // Converte o valor para microssegundos
}

// Função para lidar com deadlines ultrapassados
void alta_deadline(char* nome_acao, uint64_t tempo_execucao) {
    if (strcmp(nome_acao, "da injeção eletrônica") == 0){
        printf("Alerta: Tempo de resposta da %s excedeu o deadline (%llu us)\n", nome_acao, tempo_execucao);
    } else if (strcmp(nome_acao, "da temperatura do motor") == 0 || 
               strcmp(nome_acao, "do ABS") == 0 || 
               strcmp(nome_acao, "do airbag") == 0) {
        uint64_t tempo_execucao_micro = (tempo_execucao / 1000);
        printf("Alerta: Tempo de resposta da %s excedeu o deadline (%llu ms)\n", nome_acao, tempo_execucao_micro);
    } else if (strcmp(nome_acao, "do cinto de segurança") == 0){
        uint64_t tempo_execucao_segundos = (tempo_execucao / 1000000);
        printf("Alerta: Tempo de resposta da %s excedeu o deadline (%llu s)\n", nome_acao, tempo_execucao_segundos);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));  // Delay para atualização do display
}

// Função para exibir o tempo de resposta
void mostrar_tempo(char* nome_acao, uint64_t tempo_execucao, uint64_t tempo_resposta) {
    if (tempo_execucao < 1000) {
        printf("%s ativado\nTempo de execução para a ativação do %s: %llu us\n", nome_acao, nome_acao, tempo_execucao);
    } else if (tempo_execucao < 1000000) {
        printf("%s ativado\nTempo de execução para %s: %.2f ms\n", nome_acao, nome_acao, tempo_execucao / 1000.0);
    } else {
        printf("%s colocado\nTempo de execução para a verificação do %s: %.2f s\n", nome_acao, nome_acao, tempo_execucao / 1000000.0);
    }

    if (tempo_resposta < 1000) {
        printf("Tempo de resposta para a ativação da %s: %llu us\n", nome_acao, tempo_resposta);
    } else if (tempo_resposta < 1000000) {
        printf("Tempo de resposta para %s: %.2f ms\n", nome_acao, tempo_resposta / 1000.0);
    } else {
        printf("Tempo de resposta para a verificação do %s: %.2f s\n\n", nome_acao, tempo_resposta / 1000000.0);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));  // Delay para atualização do display
}

static void tp_example_set_thresholds(void) // Configura os thresholds para os sensores touch
{
    uint16_t touch_value;

    // Definir o threshold para o sensor TOUCH_INJECAO
    touch_pad_read_filtered(TOUCH_INJECAO, &touch_value); // Lê o valor filtrado atual do sensor TOUCH_INJECAO e armazena o resultado em touch_value.
    s_pad_init_val[TOUCH_INJECAO] = touch_value; // O valor lido é armazenado
    ESP_LOGI(TAG, "TOUCH_INJECAO init: val is %d", touch_value); // Registra o valor inicial do sensor no log
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_INJECAO, touch_value * 2 / 3)); // Garante que a operação seja verificada para erros

    // Definir o threshold para o sensor TOUCH_TEMPERATURA
    touch_pad_read_filtered(TOUCH_TEMPERATURA, &touch_value);
    s_pad_init_val[TOUCH_TEMPERATURA] = touch_value;
    ESP_LOGI(TAG, "TOUCH_TEMPERATURA init: val is %d", touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_TEMPERATURA, touch_value * 2 / 3));

    // Definir o threshold para o sensor TOUCH_ABS
    touch_pad_read_filtered(TOUCH_ABS, &touch_value);
    s_pad_init_val[TOUCH_ABS] = touch_value;
    ESP_LOGI(TAG, "TOUCH_ABS init: val is %d", touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_ABS, touch_value * 2 / 3));

    // Definir o threshold para o sensor TOUCH_AIRBAG
    touch_pad_read_filtered(TOUCH_AIRBAG, &touch_value);
    s_pad_init_val[TOUCH_AIRBAG] = touch_value;
    ESP_LOGI(TAG, "TOUCH_AIRBAG init: val is %d", touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_AIRBAG, touch_value * 2 / 3));

    // Definir o threshold para o sensor TOUCH_CINTO
    touch_pad_read_filtered(TOUCH_CINTO, &touch_value);
    s_pad_init_val[TOUCH_CINTO] = touch_value;
    ESP_LOGI(TAG, "TOUCH_CINTO init: val is %d", touch_value);
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_CINTO, touch_value * 2 / 3));
}

// Função para verificar a condição (m,k)-firme
void verificar_mk_firme(uint64_t exec_time, int *cont_ativacoes, int *cont_sucesso, int *ultimo_skip, uint64_t deadline, char* nome_acao) {
    static int skip_count = 0;   // Contador de skips entre falhas

    // Incrementa o contador de ativações na janela
    (*cont_ativacoes)++;

    // Verifica se a ativação cumpre o deadline
    if (exec_time <= deadline) {
        (*cont_sucesso)++;
        skip_count++;  // Incrementa o contador de skips entre falhas
    } else {
        *ultimo_skip = skip_count;  // Registra o skip atual
        skip_count = 0;             // Reinicia o contador de skips após a falha
    }

    // Verifica se a janela está completa (10 ativações)
    if (*cont_ativacoes == JANELA_ATIVACOES) {
        // Verifica a condição (m,k)-firme
        if (*cont_sucesso >= M_ATIVACOES) {
            printf("A tarefa está (m,k)-firme em %s: Atendidas %d/%d (m = %d)\n\n", nome_acao, *cont_sucesso, JANELA_ATIVACOES, M_ATIVACOES);
        } else {
            printf("A tarefa NÃO está (m,k)-firme em %s: Atendidas %d/%d (m = %d) Número de skip: %d\n\n", nome_acao, *cont_sucesso, JANELA_ATIVACOES, M_ATIVACOES, skip_count);
        }

        // Reseta contadores para a próxima janela
        *cont_ativacoes = 0;
        *cont_sucesso = 0;
    }
}

static void verificar_injecao_eletronica(void *pvParameter)
{
    uint64_t tempo_inicio, tempo_fim_WCET, tempo_fim_HWM, tempo_execucao, tempo_resposta;

    while (1) {
        touch_pad_intr_enable(); // Habilita as interrupções do touch pad para detectar toques no sensor.
        
        if (s_pad_activated[TOUCH_INJECAO] == true) { // Verifica se o sensor de injeção foi ativado.
            tempo_inicio = obter_tempo();
            
            vTaskDelay(pdMS_TO_TICKS(1) / 1000); // Simula tempo de aquisição de 1 us
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us) 

            gpio_set_level(GPIO_ATUADOR_INJECAO, 1); // Ativa o atuador
            vTaskDelay(pdMS_TO_TICKS(5) / 1000); // Simula tempo de controle do atuador (5 us)
            gpio_set_level(GPIO_ATUADOR_INJECAO, 0); // Desativa o atuador
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            tempo_fim_WCET = obter_tempo(); // Captura o tempo após a execução
            tempo_execucao = tempo_fim_WCET - tempo_inicio; // Calcula o tempo total de execução

            if (tempo_execucao > WCET_injecao) { // Atualiza o WCET se o tempo de execução atual for o maior registrado.
                WCET_injecao = tempo_execucao;
            }

            s_pad_activated[TOUCH_INJECAO] = false; // Reseta o estado do sensor de injeção.

            tempo_fim_HWM = obter_tempo(); // Captura o tempo após a resposta completa
            tempo_resposta = tempo_fim_HWM - tempo_inicio; // Calcula o tempo total de resposta
            
            if(tempo_resposta > HWM_injecao){ // Atualiza o HWM se o tempo de resposta atual for o maior registrado
                HWM_injecao = tempo_resposta;
            }
            
            if(tempo_resposta < LWM_injecao){ // Atualiza o LWM se o tempo de resposta atual for o menor registrado
                LWM_injecao = tempo_resposta;
            }

            WCRT_injecao = HWM_injecao + (HWM_injecao - LWM_injecao) + HWM_abs + HWM_airbag; // Calcula o WCRT da injeção eletrônica

            if (HWM_injecao > DEADLINE_INJECAO) { // Verifica se o tempo excedeu o deadline
                alta_deadline("da injeção eletrônica", tempo_resposta);
            } 

            verificar_mk_firme(tempo_resposta, &cont_ativacoes_injecao, &cont_dl_injecao, &ultimo_skip_injecao, DEADLINE_INJECAO, "Injeção eletrônica"); // Verifica a condição (m,k)-firme para a tarefa de injeção

            mostrar_tempo("Injeção eletrônica", tempo_execucao, tempo_resposta); // Exibe os tempos de execução e resposta no log
        }
        vTaskDelay(pdMS_TO_TICKS(100));   // Delay antes da próxima verificação
    }
}

static void verificar_temperatura_motor(void *pvParameter)
{
    uint64_t tempo_inicio, tempo_fim_WCET, tempo_fim_HWM, tempo_execucao, tempo_resposta;

    while (1) {
        touch_pad_intr_enable();

        if (s_pad_activated[TOUCH_TEMPERATURA] == true) {
            tempo_inicio = obter_tempo();

            vTaskDelay(pdMS_TO_TICKS(1) / 1000); // Simula tempo de aquisição de 1 us
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 1); // Ativa o atuador
            vTaskDelay(pdMS_TO_TICKS(5) / 1000); // Simula tempo de controle do atuador (5 us)
            gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 0); // Desativa o atuador
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            tempo_fim_WCET = obter_tempo(); // Captura o tempo após a execução
            tempo_execucao = tempo_fim_WCET - tempo_inicio; // Calcula o tempo total de execução

            if (tempo_execucao > WCET_temperatura) { // Verificação do maior tempo de execução
                WCET_temperatura = tempo_execucao;
            }

            s_pad_activated[TOUCH_TEMPERATURA] = false; // Reset

            tempo_fim_HWM = obter_tempo(); // Captura o tempo após a resposta
            tempo_resposta = tempo_fim_HWM - tempo_inicio; // Calcula o tempo total de resposta

            if (tempo_resposta > HWM_temperatura) { // Verifica o maior tempo de resposta
                HWM_temperatura = tempo_resposta;
            }

            if (tempo_resposta < LWM_temperatura) { // Verifica o menor tempo de resposta
                LWM_temperatura = tempo_resposta;
            }

            WCRT_temperatura = HWM_temperatura + (HWM_temperatura - LWM_temperatura) + HWM_abs + HWM_airbag + HWM_injecao; // Calculo do WCRT da temperatura

            if (HWM_temperatura > DEADLINE_TEMPERATURA) { // Verifica se o tempo excedeu o deadline
                alta_deadline("da temperatura", tempo_resposta);
            }

            verificar_mk_firme(tempo_resposta, &cont_ativacoes_temperatura, &cont_dl_temperatura, &ultimo_skip_temperatura, DEADLINE_TEMPERATURA, "Temperatura do motor");

            mostrar_tempo("Temperatura do motor", tempo_execucao, tempo_resposta);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Delay antes da próxima verificação
    }
}

static void verificar_abs(void *pvParameter)
{
    uint64_t tempo_inicio, tempo_fim_WCET, tempo_fim_HWM, tempo_execucao, tempo_resposta;

    while (1) {
        // Modo de interrupção: habilitar interrupção para o sensor ABS
        touch_pad_intr_enable();

        if (s_pad_activated[TOUCH_ABS] == true) {
            tempo_inicio = obter_tempo();

            vTaskDelay(pdMS_TO_TICKS(1) / 1000); // Simula tempo de aquisição de 1 us
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            gpio_set_level(GPIO_ATUADOR_ABS, 1); // Ativa o atuador
            vTaskDelay(pdMS_TO_TICKS(5) / 1000); // Simula tempo de controle do atuador (5 us)
            gpio_set_level(GPIO_ATUADOR_ABS, 0); // Desativa o atuador
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            tempo_fim_WCET = obter_tempo(); // Captura o tempo após a execução
            tempo_execucao = tempo_fim_WCET - tempo_inicio; // Calcula o tempo total de execução

            if (tempo_execucao > WCET_abs) { // Verificação do maior tempo de execução
                WCET_abs = tempo_execucao;
            }

            s_pad_activated[TOUCH_ABS] = false; // Reset

            tempo_fim_HWM = obter_tempo(); // Captura o tempo após a resposta
            tempo_resposta = tempo_fim_HWM - tempo_inicio; // Calcula o tempo total de resposta

            if (tempo_resposta > HWM_abs) { // Verifica o maior tempo de resposta
                HWM_abs = tempo_resposta;
            }

            if (tempo_resposta < LWM_abs) { // Verifica o menor tempo de resposta
                LWM_abs = tempo_resposta;
            }

            WCRT_abs = HWM_abs + (HWM_abs - LWM_abs) + HWM_airbag; // Calculo do WCRT do ABS

            if (HWM_abs > DEADLINE_ABS) { // Verifica se o tempo excedeu o deadline
                alta_deadline("do ABS", tempo_resposta);
            }

            verificar_mk_firme(tempo_resposta, &cont_ativacoes_abs, &cont_dl_abs, &ultimo_skip_abs, DEADLINE_ABS, "ABS");

            mostrar_tempo("ABS", tempo_execucao, tempo_resposta);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Delay antes da próxima verificação
    }
}

static void verificar_airbag(void *pvParameter)
{
    uint64_t tempo_inicio, tempo_fim_WCET, tempo_fim_HWM, tempo_execucao, tempo_resposta;

    while (1) {
        // Modo de interrupção: habilitar interrupção para o sensor Airbag
        touch_pad_intr_enable();

        if (s_pad_activated[TOUCH_AIRBAG] == true) {
            tempo_inicio = obter_tempo();

            vTaskDelay(pdMS_TO_TICKS(1) / 1000); // Simula tempo de aquisição de 1 us
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            gpio_set_level(GPIO_ATUADOR_AIRBAG, 1); // Ativa o atuador
            vTaskDelay(pdMS_TO_TICKS(5) / 1000); // Simula tempo de controle do atuador (5 us)
            gpio_set_level(GPIO_ATUADOR_AIRBAG, 0); // Desativa o atuador
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            tempo_fim_WCET = obter_tempo(); // Captura o tempo após a execução
            tempo_execucao = tempo_fim_WCET - tempo_inicio; // Calcula o tempo total de execução

            if (tempo_execucao > WCET_airbag) { // Verificação do maior tempo de execução
                WCET_airbag = tempo_execucao;
            }

            s_pad_activated[TOUCH_AIRBAG] = false; // Reset

            tempo_fim_HWM = obter_tempo(); // Captura o tempo após a resposta
            tempo_resposta = tempo_fim_HWM - tempo_inicio; // Calcula o tempo total de resposta

            if (tempo_resposta > HWM_airbag) { // Verifica o maior tempo de resposta
                HWM_airbag = tempo_resposta;
            }

            if (tempo_resposta < LWM_airbag) { // Verifica o menor tempo de resposta
                LWM_airbag = tempo_resposta;
            }

            WCRT_airbag = HWM_airbag + (HWM_airbag - LWM_airbag); // Calculo do WCRT do airbag

            if (HWM_airbag > DEADLINE_AIRBAG) { // Verifica se o tempo excedeu o deadline
                alta_deadline("do airbag", tempo_resposta);
            }

            verificar_mk_firme(tempo_resposta, &cont_ativacoes_airbag, &cont_dl_airbag, &ultimo_skip_airbag, DEADLINE_AIRBAG, "Airbag");

            mostrar_tempo("Airbag", tempo_execucao, tempo_resposta);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Delay antes da próxima verificação
    }
}

static void verificar_cinto_seguranca(void *pvParameter)
{
    uint64_t tempo_inicio, tempo_fim_WCET, tempo_fim_HWM, tempo_execucao, tempo_resposta;

    while (1) {
        // Modo de interrupção: habilitar interrupção para o sensor do Cinto de Segurança
        touch_pad_intr_enable();

        if (s_pad_activated[TOUCH_CINTO] == true) {
            tempo_inicio = obter_tempo();

            vTaskDelay(pdMS_TO_TICKS(1) / 1000); // Simula tempo de aquisição de 1 us
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            gpio_set_level(GPIO_ATUADOR_CINTO, 1); // Ativa o atuador
            vTaskDelay(pdMS_TO_TICKS(5) / 1000); // Simula tempo de controle do atuador (5 us)
            gpio_set_level(GPIO_ATUADOR_CINTO, 0); // Desativa o atuador
            vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

            tempo_fim_WCET = obter_tempo(); // Captura o tempo após a execução
            tempo_execucao = tempo_fim_WCET - tempo_inicio; // Calcula o tempo total de execução

            if (tempo_execucao > WCET_cinto) { // Verificação do maior tempo de execução
                WCET_cinto = tempo_execucao;
            }

            s_pad_activated[TOUCH_CINTO] = false; // Reset

            tempo_fim_HWM = obter_tempo(); // Captura o tempo após a resposta
            tempo_resposta = tempo_fim_HWM - tempo_inicio; // Calcula o tempo total de resposta

            if (tempo_resposta > HWM_cinto) { // Verifica o maior tempo de resposta
                HWM_cinto = tempo_resposta;
            }

            if (tempo_resposta < LWM_cinto) { // Verifica o menor tempo de resposta
                LWM_cinto = tempo_resposta;
            }

            WCRT_cinto = HWM_cinto + (HWM_cinto - LWM_cinto) + HWM_injecao + HWM_abs + HWM_airbag + HWM_temperatura; // Calculo do WCRT do cinto

            if (HWM_cinto > DEADLINE_CINTO) { // Verifica se o tempo excedeu o deadline
                alta_deadline("do cinto de segurança", tempo_resposta);
            }

            verificar_mk_firme(tempo_resposta, &cont_ativacoes_cinto, &cont_dl_cinto, &ultimo_skip_cinto, DEADLINE_CINTO, "Cinto de segurança");

            mostrar_tempo("Cinto de segurança", tempo_execucao, tempo_resposta);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Delay antes da próxima verificação
    }
}

// Função de interrupção para o touch pad, responsável por verificar quais sensores foram ativados
static void tp_example_rtc_intr(void *arg)
{
    uint32_t pad_intr = touch_pad_get_status(); // Obtém o status atual dos sensores touch.
    touch_pad_clear_status(); // Limpa o status dos sensores touch após a leitura.
    
    // Verifica se o sensor foi ativado
    if ((pad_intr >> TOUCH_INJECAO) & 0x01) {
        s_pad_activated[TOUCH_INJECAO] = true; // Marca o sensor de injeção como ativado
    }
    if ((pad_intr >> TOUCH_TEMPERATURA) & 0x01) {
        s_pad_activated[TOUCH_TEMPERATURA] = true;
    }
    if ((pad_intr >> TOUCH_ABS) & 0x01) {
        s_pad_activated[TOUCH_ABS] = true;
    }
    if ((pad_intr >> TOUCH_AIRBAG) & 0x01) {
        s_pad_activated[TOUCH_AIRBAG] = true;
    }
    if ((pad_intr >> TOUCH_CINTO) & 0x01) {
        s_pad_activated[TOUCH_CINTO] = true;
    }
}

// Função de inicialização dos sensores touch pad, configurando-os sem limiar (threshold) de uso imediato
static void tp_example_touch_pad_init(void)
{
    touch_pad_config(TOUCH_INJECAO, TOUCH_THRESH_NO_USE);
    touch_pad_config(TOUCH_TEMPERATURA, TOUCH_THRESH_NO_USE);
    touch_pad_config(TOUCH_ABS, TOUCH_THRESH_NO_USE);
    touch_pad_config(TOUCH_AIRBAG, TOUCH_THRESH_NO_USE);
    touch_pad_config(TOUCH_CINTO, TOUCH_THRESH_NO_USE);
}

// Função para atualizar o display
void atualizar_display(void *pvParameters) {
    while (1) { // Loop infinito
        atualizar_amostras();  // Atualiza as amostras de velocidade e consumo
        float media_velocidade = calcular_media(vetor_velocidade); // Calcula a média de velocidade
        float media_consumo = calcular_media(vetor_consumo); // Calcula a média de consumo
        printf("\n\n\nVelocidade média: %.2f km/h\n", media_velocidade); // Exibe a média de velocidade
        printf("Consumo médio: %.2f L/100km\n", media_consumo); // Exibe a média de consumo

        // Exibe o WCET, HWM, e WCRT para cada subsistema
        printf("\nWCET da injeção eletrônica: %llu", WCET_injecao);
        printf("\nHWM da injeção eletrônica: %llu", HWM_injecao);
        printf("\nWCRT da injeção eletrônica: %llu\n", WCRT_injecao);

        printf("\nWCET da temperatura do motor: %llu", WCET_temperatura);
        printf("\nHWM da temperatura do motor: %llu", HWM_temperatura);
        printf("\nWCRT da temperatura do motor: %llu\n", WCRT_temperatura);

        printf("\nWCET do ABS: %llu", WCET_abs); 
        printf("\nHWM do ABS: %llu", HWM_abs);
        printf("\nWCRT do ABS: %llu\n", WCRT_abs);

        printf("\nWCET do airbag: %llu", WCET_airbag);
        printf("\nHWM do airbag: %llu", HWM_airbag);
        printf("\nWCRT do airbag: %llu\n", WCRT_airbag);

        printf("\nWCET do cinto de segurança: %llu", WCET_cinto);
        printf("\nHWM do cinto de segurança: %llu", HWM_cinto);
        printf("\nWCRT do cinto de segurança: %llu\n\n", WCRT_cinto); 

        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay de 1 segundo para atualização
    }
}

void app_main(void)
{
    // Configura os GPIOs dos atuadores como saída
    gpio_set_direction(GPIO_ATUADOR_INJECAO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_TEMPERATURA, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_ABS, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_AIRBAG, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_CINTO, GPIO_MODE_OUTPUT);

    touch_pad_init(); // Inicializa o touch pad

    configurar_timer(); // configura o timer
    
    // Configurações do touch pad para detecção de toques
    ESP_LOGI(TAG, "Initializing touch pad");
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    tp_example_touch_pad_init();
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    tp_example_set_thresholds();
    touch_pad_isr_register(tp_example_rtc_intr, NULL);

    // Criação de tasks para monitorar e controlar cada subsistema
    xTaskCreate(verificar_injecao_eletronica, "InjecaoEletronicaTask", 2048, NULL, 4, NULL);
    xTaskCreate(verificar_temperatura_motor, "TemperaturaMotorTask", 2048, NULL, 3, NULL);
    xTaskCreate(verificar_abs, "ABSTask", 2048, NULL, 5, NULL);
    xTaskCreate(verificar_airbag, "AirbagTask", 2048, NULL, 6, NULL);
    xTaskCreate(verificar_cinto_seguranca, "CintoSegurancaTask", 2048, NULL, 2, NULL);

    // Criação de task para atualização do display
    xTaskCreate(atualizar_display, "DisplayTask", 2048, NULL, 1, NULL);
}
