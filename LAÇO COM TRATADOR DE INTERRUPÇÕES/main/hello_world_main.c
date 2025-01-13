// Bibliotecas usadas no código
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"
#include <time.h>

// Mapeamento dos sensores com GPIOs
#define TOUCH_INJECAO TOUCH_PAD_NUM0  // GPIO4
#define TOUCH_TEMPERATURA TOUCH_PAD_NUM8  // GPIO0
#define TOUCH_ABS TOUCH_PAD_NUM9  // GPIO32
#define TOUCH_AIRBAG TOUCH_PAD_NUM3  // GPIO15
#define TOUCH_CINTO TOUCH_PAD_NUM4  // GPIO13

// Define os GPIOs para os atuadores correspondentes aos sensores
#define GPIO_ATUADOR_INJECAO GPIO_NUM_18
#define GPIO_ATUADOR_TEMPERATURA GPIO_NUM_19
#define GPIO_ATUADOR_ABS GPIO_NUM_21
#define GPIO_ATUADOR_AIRBAG GPIO_NUM_22
#define GPIO_ATUADOR_CINTO GPIO_NUM_23

// Deadlines em microssegundos
#define DEADLINE_INJECAO 500      // 500 us
#define DEADLINE_TEMPERATURA 20000 // 20 ms
#define DEADLINE_ABS 100000       // 100 ms
#define DEADLINE_AIRBAG 100000    // 100 ms
#define DEADLINE_CINTO 1000000    // 1 segundo

// Definindo o timer
#define TIMER_DIVIDER 80  // Divisor de clock para o timer
#define TIMER_BASE_CLK 80000000 // Base do clock do timer (80 MHz)
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)  // Calcula o fator de escala para o timer

// Definição do número de amostras e vetores para armazenar dados de velocidade e consumo
#define NUM_AMOSTRAS 200
int vetor_velocidade[NUM_AMOSTRAS] = {0};
int vetor_consumo[NUM_AMOSTRAS] = {0};
int indice = 0;

// Variáveis de controle para o timer e amostras
static bool executar_verificacao_injecao = false;
static bool executar_verificacao_temperatura = false;
static bool executar_verificacao_display = false;
static bool executar_verificacao_abs_airbag_cinto = false;

// Variáveis que indicam se os sensores foram acionados
static bool sensor_injecao_acionado = false;
static bool sensor_temperatura_acionado = false;
static bool sensor_abs_acionado = false;
static bool sensor_airbag_acionado = false;
static bool sensor_cinto_acionado = false;

// Variáveis para armazenar o tempo de resposta de cada sensor
uint64_t tempo_resposta_injecao = 0;
uint64_t tempo_resposta_temperatura = 0;
uint64_t tempo_resposta_abs = 0;
uint64_t tempo_resposta_airbag = 0;
uint64_t tempo_resposta_cinto = 0;

// Função para configurar os GPIOs e Touch Pads
void configurar_gpio() {
    // Configurar atuadores como saídas
    gpio_set_direction(GPIO_ATUADOR_INJECAO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_TEMPERATURA, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_ABS, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_AIRBAG, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_CINTO, GPIO_MODE_OUTPUT);

    // Inicializar os touch pads
    touch_pad_init();
    
    // Configura os sensores touch com sensibilidade ajustada
    touch_pad_config(TOUCH_INJECAO, 40);  // Configura o sensor de injeção
    touch_pad_config(TOUCH_TEMPERATURA, 40);  // Configura o sensor de temperatura
    touch_pad_config(TOUCH_ABS, 40);  // Configura o sensor ABS
    touch_pad_config(TOUCH_AIRBAG, 40);  // Configura o sensor Airbag
    touch_pad_config(TOUCH_CINTO, 40);  // Configura o sensor do cinto de segurança
}

// Função para atualizar as amostras de velocidade e consumo
void atualizar_amostras() {
    // Gera nova amostra de velocidade (0 a 150 km/h) e consumo (0 a 20 L/100km)
    int nova_velocidade = rand() % 150;
    int novo_consumo = rand() % 20;  

    // Atualiza os vetores de amostras de forma circular
    vetor_velocidade[indice] = nova_velocidade;
    vetor_consumo[indice] = novo_consumo;
    
    // Atualiza o índice para o próximo
    indice = (indice + 1) % NUM_AMOSTRAS;
}

// Função para calcular a média de 200 amostras
float calcular_media(int* vetor) {
    int soma = 0;
    for (int i = 0; i < NUM_AMOSTRAS; i++) {
        soma += vetor[i];
    }
    return (float)soma / NUM_AMOSTRAS;
}

// Função de interrupção (ISR) para verificar a injeção eletrônica (executa a cada 500 us)
void IRAM_ATTR timer_injecao_isr_handler(void* arg) {
    executar_verificacao_injecao = true; // Sinaliza que a injeção deve ser verificada
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0); // Limpa o status da interrupção
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_0); // Habilita o alarme novamente
}

// ISR para verificar a temperatura do motor (executa a cada 20 ms)
void IRAM_ATTR timer_temperatura_isr_handler(void* arg) {
    executar_verificacao_temperatura = true; // Sinaliza que a temperatura deve ser verificada
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_1, TIMER_0); // Limpa o status da interrupção
    timer_group_enable_alarm_in_isr(TIMER_GROUP_1, TIMER_0); // Habilita o alarme novamente
}

// ISR para atualização do display (executa a cada 1 segundo)
void IRAM_ATTR timer_display_isr_handler(void* arg) {
    executar_verificacao_display = true;  // Sinaliza que o display deve ser atualizado
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_1); // Limpa o status da interrupção
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, TIMER_1); // Habilita o alarme novamente
}

// ISR para verificar ABS, Airbag e Cinto (a cada 100 ms)
void IRAM_ATTR timer_abs_airbag_cinto_isr_handler(void* arg) {
    executar_verificacao_abs_airbag_cinto = true;  // Sinaliza que ABS, Airbag e Cinto devem ser verificados
    timer_group_clr_intr_status_in_isr(TIMER_GROUP_1, TIMER_1); // Limpa o status da interrupção
    timer_group_enable_alarm_in_isr(TIMER_GROUP_1, TIMER_1); // Habilita o alarme novamente
}

// Função para configurar os timers com diferentes propósitos
void configurar_timers() {
    // Configuração padrão dos timers
    timer_config_t config = {
        .divider = TIMER_DIVIDER, // Define o divisor do clock
        .counter_dir = TIMER_COUNT_UP, // Contagem crescente
        .counter_en = TIMER_PAUSE, // Começa pausado
        .alarm_en = TIMER_ALARM_EN, // Habilita alarme
        .auto_reload = true // Habilita recarga automática após alarme
    };

    // Timer para injeção eletrônica (500 us)
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL); // Zera o timer
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, TIMER_SCALE * 0.0005);  // Define o alarme em 500 us
    timer_enable_intr(TIMER_GROUP_0, TIMER_0); // Habilita a interrupção
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_injecao_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL); // Registra a ISR
    timer_start(TIMER_GROUP_0, TIMER_0); // Inicia o timer
 
    // Timer para temperatura do motor (20 ms)
    timer_init(TIMER_GROUP_1, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_1, TIMER_0, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_1, TIMER_0, TIMER_SCALE * 0.02);  // 20 ms
    timer_enable_intr(TIMER_GROUP_1, TIMER_0);
    timer_isr_register(TIMER_GROUP_1, TIMER_0, timer_temperatura_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_1, TIMER_0);

    // Timer para display  (1 segundo)
    timer_init(TIMER_GROUP_0, TIMER_1, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, TIMER_SCALE * 1);  // 1 segundo
    timer_enable_intr(TIMER_GROUP_0, TIMER_1);
    timer_isr_register(TIMER_GROUP_0, TIMER_1, timer_display_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, TIMER_1);

    // Timer para ABS e Airbag e Cinto (100 ms)
    timer_init(TIMER_GROUP_1, TIMER_1, &config);
    timer_set_counter_value(TIMER_GROUP_1, TIMER_1, 0x00000000ULL);
    timer_set_alarm_value(TIMER_GROUP_1, TIMER_1, TIMER_SCALE * 0.1);  // 100 ms
    timer_enable_intr(TIMER_GROUP_1, TIMER_1);
    timer_isr_register(TIMER_GROUP_1, TIMER_1, timer_abs_airbag_cinto_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_1, TIMER_1);
}

// Função para exibir o tempo de execução em diferentes unidades
void mostrar_tempo(char* nome_acao, uint64_t tempo_execucao) {
    if (tempo_execucao < 1000) {
        printf("Tempo de resposta para %s: %llu us\n", nome_acao, tempo_execucao);
    } else if (tempo_execucao < 1000000) {
        printf("Tempo de resposta para %s: %.2f ms\n", nome_acao, tempo_execucao / 1000.0);
    } else {
        printf("Tempo de resposta para %s: %.2f s\n", nome_acao, tempo_execucao / 1000000.0);
    }
}

// Função para obter o tempo do timer em microssegundos
uint64_t obter_tempo(int timer_group, int timer_idx) {
    uint64_t timer_val;
    timer_get_counter_value(timer_group, timer_idx, &timer_val);  // Obtém o valor do timer
    return (timer_val * 1000000) / TIMER_SCALE;  // Converte para microssegundos
}

// Função para verificar e controlar a injeção eletrônica
void verificar_e_controlar_injecao() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio, tempo_fim;

    // Leitura do sensor touch para injeção eletrônica
    touch_pad_read(TOUCH_INJECAO, &leitura_touch);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL); // Reseta o timer
    tempo_inicio = obter_tempo(TIMER_GROUP_0, TIMER_0);  // Captura o tempo de início

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Simula tempo de aquisição de 1 us

    if (leitura_touch < 500) {
        sensor_injecao_acionado = true; // Sinaliza que o sensor foi acionado

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_INJECAO, 1); // Ativação do atuador

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_INJECAO, 0); // Desativação do atuador

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo(TIMER_GROUP_0, TIMER_0);  // Captura o tempo final
        tempo_resposta_injecao = tempo_fim - tempo_inicio; // Calcula o tempo de resposta

    } else {
        sensor_injecao_acionado = false;
    }
}

// Função para verificar e controlar o freio ABS
void verificar_e_controlar_abs() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio = 0, tempo_fim = 0;

    // ABS
    touch_pad_read(TOUCH_ABS, &leitura_touch);
    tempo_inicio = obter_tempo(TIMER_GROUP_1, TIMER_1);

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Tempo de aquisição de 1 us

    if (leitura_touch < 500) {
        sensor_abs_acionado = true;

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_ABS, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_ABS, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo(TIMER_GROUP_1, TIMER_1);
        tempo_resposta_abs = tempo_fim - tempo_inicio;

    } else {
        sensor_abs_acionado = false;
    }
}

// Função para verificar e controlar a temperatura do motor
void verificar_e_controlar_temperatura() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio = 0, tempo_fim = 0;

    // Temperatura do motor
    touch_pad_read(TOUCH_TEMPERATURA, &leitura_touch);
    tempo_inicio = obter_tempo(TIMER_GROUP_1, TIMER_0);  // Captura o tempo

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Tempo de aquisição de 1 us

    if (leitura_touch < 500) {
        sensor_temperatura_acionado = true;

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo(TIMER_GROUP_1, TIMER_0);
        tempo_resposta_temperatura = tempo_fim - tempo_inicio;

    } else {
        sensor_temperatura_acionado = false;
    }
}

// Função para verificar e controlar o Airbag
void verificar_e_controlar_airbag() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio = 0, tempo_fim = 0;

    // Airbag
    touch_pad_read(TOUCH_AIRBAG, &leitura_touch);
    tempo_inicio = obter_tempo(TIMER_GROUP_1, TIMER_1);

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Tempo de aquisição de 1 us

    if (leitura_touch < 500) {
        sensor_airbag_acionado = true;

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_AIRBAG, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_AIRBAG, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo(TIMER_GROUP_1, TIMER_1);
        tempo_resposta_airbag = tempo_fim - tempo_inicio;

    } else {
        sensor_airbag_acionado = false;
    }
}

// Função para verificar e controlar o cinto de segurança
void verificar_e_controlar_cinto() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio = 0, tempo_fim = 0;

    // Cinto de segurança
    touch_pad_read(TOUCH_CINTO, &leitura_touch);
    tempo_inicio = obter_tempo(TIMER_GROUP_1, TIMER_1);

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Tempo de aquisição de 1 us

    if (leitura_touch < 500) {
        sensor_cinto_acionado = true;
        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_CINTO, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_CINTO, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo(TIMER_GROUP_1, TIMER_1);
        tempo_resposta_cinto = tempo_fim - tempo_inicio;

    } else {
        sensor_cinto_acionado = false;
    }
}

// Função principal
void app_main() {
    // Configurar GPIOs e sensores touch
    configurar_gpio();
    
    // Configurar o Timer com interrupção
    configurar_timers();

    srand(time(NULL));  // Inicializa o gerador de números aleatórios

    // Laço principal
     while (1) {
        // Verificar injeção eletrônica
        if (executar_verificacao_injecao) {
            verificar_e_controlar_injecao();  // Função que lida com a injeção
            executar_verificacao_injecao = false;  // Reseta a flag
        }

        // Verificar temperatura do motor
        if (executar_verificacao_temperatura) {
            verificar_e_controlar_temperatura();  // Função que lida com a temperatura do motor
            executar_verificacao_temperatura = false;  // Reseta a flag
        }

        // Verificar ABS e Airbag
        if (executar_verificacao_abs_airbag_cinto) {
            verificar_e_controlar_airbag(); // Função que lida com o airbag
            verificar_e_controlar_abs(); // Função que lida com o ABS 
            verificar_e_controlar_cinto(); // Função que lida com o cinto de segurança
            executar_verificacao_abs_airbag_cinto = false;  // Reseta a flag
        }

        // Atualizar o display e o cinto de segurança a cada 1 segundo
        if (executar_verificacao_display) {
            // Atualiza as amostras de velocidade e consumo
            atualizar_amostras();

            // Calcula as médias das últimas 200 amostras
            float media_velocidade = calcular_media(vetor_velocidade);
            float media_consumo = calcular_media(vetor_consumo);

            // Exibir as médias no display
            printf("\nVelocidade média: %.2f km/h\n", media_velocidade);
            printf("Consumo médio: %.2f L/100km\n", media_consumo);

            // Verificar e mostrar os tempos de resposta se os sensores estiverem acionados
            if (sensor_injecao_acionado) {
                if (tempo_resposta_injecao > DEADLINE_INJECAO) {
                    printf("Alerta: Tempo de resposta da Injeção Eletrônica excedeu o deadline (%lld us)\n", tempo_resposta_injecao);
                } else {
                    mostrar_tempo("Injeção eletrônica", tempo_resposta_injecao);
                }
            } else { // caso o sensor não esteja sendo tocado, aparece a mensagem
                printf("Injeção eletrônica desativada\n");
            } 
            if (sensor_temperatura_acionado) {
                if (tempo_resposta_temperatura > DEADLINE_TEMPERATURA) {
                    printf("Alerta: Tempo de resposta da temperatura excedeu o deadline (%f ms)\n", tempo_resposta_temperatura / 1000.0);
                } else {
                    mostrar_tempo("Temperatura do motor", tempo_resposta_temperatura);
                }
            } else {
                printf("Temperatura do motor normal\n");
            }
            if (sensor_abs_acionado) {
                if (tempo_resposta_abs > DEADLINE_ABS) {
                    printf("Alerta: Tempo de resposta do ABS excedeu o deadline (%f ms)\n", tempo_resposta_abs / 1000.0);
                } else {
                    mostrar_tempo("ABS", tempo_resposta_abs);
                }
            } else {
                printf("ABS desativado\n");
            }
            if (sensor_airbag_acionado) {
                if (tempo_resposta_airbag > DEADLINE_AIRBAG) {
                    printf("Alerta: Tempo de resposta do Airbag excedeu o deadline (%f ms)\n", tempo_resposta_airbag / 1000.0);
                } else {
                    mostrar_tempo("Airbag", tempo_resposta_airbag);
                }
            } else {
                printf("Airbag pronto\n");
            }
            if (sensor_cinto_acionado) {
                if (tempo_resposta_airbag > DEADLINE_AIRBAG) {
                    printf("Alerta: Tempo de resposta do Airbag excedeu o deadline (%f ms)\n", tempo_resposta_airbag / 1000.0);
                } else {
                    mostrar_tempo("Cinto de segurança", tempo_resposta_cinto);
                }
            } else {
                printf("O Cinto de segurança não está colocado\n");
            }

            executar_verificacao_display = false;  // Reseta a flag de atualização do display
        }

        // Delay mínimo para evitar consumo de 100% da CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}