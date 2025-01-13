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
#define TIMER_DIVIDER 80  // Define o divisor de clock para o timer
#define TIMER_BASE_CLK 80000000 // Base do clock do timer (80 MHz)
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER)  // Calcula o fator de escala para o timer

// Definição do número de amostras e vetores para armazenar dados de velocidade e consumo
#define NUM_AMOSTRAS 200
int vetor_velocidade[NUM_AMOSTRAS] = {0};
int vetor_consumo[NUM_AMOSTRAS] = {0};
int indice = 0;

// Função para configurar o Timer
void configurar_timer() {

    // Configurações básicas do timer
    timer_config_t config = {
        .divider = TIMER_DIVIDER, // Divisor de clock
        .counter_dir = TIMER_COUNT_UP,  // Contagem crescente
        .counter_en = TIMER_PAUSE,  // Iniciar pausado
        .alarm_en = TIMER_ALARM_DIS,  // Sem alarmes
        .auto_reload = false  // Sem recarga automática
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);  // Inicializa o timer
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);  // Zera o valor do timer
    timer_start(TIMER_GROUP_0, TIMER_0);  // Inicia o timer
}

// Função para obter o tempo em microssegundos usando o driver de timer
uint64_t obter_tempo() {
    uint64_t timer_val;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &timer_val);  // Obtém o valor atual do contador do timer
    return timer_val / (TIMER_BASE_CLK / TIMER_DIVIDER / 1000000);  // Converte o valor do contador para microssegundos
}

// Função para exibir o tempo de execução com a unidade apropriada
void mostrar_tempo(char* nome_acao, uint64_t tempo_execucao) {
    // Exibe o tempo de execução de acordo com a unidade (us, ms ou s)
    if (tempo_execucao < 1000) {
        printf("Tempo de resposta para %s: %llu us\n", nome_acao, tempo_execucao);
    } else if (tempo_execucao < 1000000) {
        printf("Tempo de resposta para %s: %.2f ms\n", nome_acao, tempo_execucao / 1000.0);
    } else {
        printf("Tempo de resposta para %s: %.2f s\n", nome_acao, tempo_execucao / 1000000.0);
    }
}

// Função para configurar os GPIOs e Touch Pads
void configurar_gpio() {
    // Configura os GPIOs dos atuadores como saídas
    gpio_set_direction(GPIO_ATUADOR_INJECAO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_TEMPERATURA, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_ABS, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_AIRBAG, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_ATUADOR_CINTO, GPIO_MODE_OUTPUT);

    // Inicializar os touch pads
    touch_pad_init();
    
    // Configura cada touch pad com um valor de sensibilidade
    touch_pad_config(TOUCH_INJECAO, 40);  // Configura o sensor de injeção
    touch_pad_config(TOUCH_TEMPERATURA, 40);  // Configura o sensor de temperatura
    touch_pad_config(TOUCH_ABS, 40);  // Configura o sensor ABS
    touch_pad_config(TOUCH_AIRBAG, 40);  // Configura o sensor Airbag
    touch_pad_config(TOUCH_CINTO, 40);  // Configura o sensor do cinto de segurança
}

// Função para verificar e controlar os atuadores com base nos sensores touch
void verificar_e_controlar_sensores() {
    uint16_t leitura_touch;
    uint64_t tempo_inicio, tempo_fim, tempo_execucao;

    // Zera o timer antes de cada medição
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    // Verificação da injeção eletrônica
    touch_pad_read(TOUCH_INJECAO, &leitura_touch); // Lê o valor do touch pad de injeção
    tempo_inicio = obter_tempo();  // Captura o tempo atual

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Simula um delay de 1 us de aquisição da amostra

    if (leitura_touch < 500) {
        // Acionamento do atuador da injeção eletrônica
        printf("Ativando atuador da Injeção Eletrônica\n");

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        gpio_set_level(GPIO_ATUADOR_INJECAO, 1); // Ativação do atuador

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo de controle do atuador (5 us)

        gpio_set_level(GPIO_ATUADOR_INJECAO, 0); // Desativação do atuador

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo de envio dos dados (10 us)

        tempo_fim = obter_tempo();  // Captura o tempo após a ação
        tempo_execucao = tempo_fim - tempo_inicio; // Calcula o tempo de resposta

        // Verifica se o tempo de resposta excedeu o deadline
        if (tempo_execucao > DEADLINE_INJECAO) {
            printf("Alerta: Tempo de resposta da Injeção Eletrônica excedeu o deadline (%llu us)\n", tempo_execucao);
        } else { // Caso não exceda, chama a função de mostrar
            mostrar_tempo("a injeção eletrônica", tempo_execucao);
        }
    } else { // caso o sensor não esteja sendo tocado, aparece a mensagem
        printf("Injeção eletrônica desativada\n");
    }

    // Zera o timer antes da medição e reinicia todas as verificações
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    // Temperatura do motor
    touch_pad_read(TOUCH_TEMPERATURA, &leitura_touch);
    tempo_inicio = obter_tempo();  // Captura o tempo

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  // Tempo 1 us

    if (leitura_touch < 500) {
        printf("A temperatura do motor execeu 110º C\n");

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo  (10 us)

        gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  // Simula tempo  (5 us)

        gpio_set_level(GPIO_ATUADOR_TEMPERATURA, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); // Simula tempo  (10 us)

        tempo_fim = obter_tempo();
        tempo_execucao = tempo_fim - tempo_inicio;

        if (tempo_execucao > DEADLINE_TEMPERATURA) {
            printf("Alerta: Tempo de resposta da Temperatura excedeu o deadline (%llu ms)\n", tempo_execucao);
        } else {
            mostrar_tempo("a verificação da temperatura do motor", tempo_execucao);
        }
    } else {
        printf("Temperatura do motor normal\n");
    }

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    // ABS
    touch_pad_read(TOUCH_ABS, &leitura_touch);
    tempo_inicio = obter_tempo();

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  

    if (leitura_touch < 500) {
        printf("Ativando ABS\n");

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        gpio_set_level(GPIO_ATUADOR_ABS, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  

        gpio_set_level(GPIO_ATUADOR_ABS, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        tempo_fim = obter_tempo();
        tempo_execucao = tempo_fim - tempo_inicio;

        if (tempo_execucao > DEADLINE_ABS) {
            printf("Alerta: Tempo de resposta do ABS excedeu o deadline (%llu ms)\n", tempo_execucao);
        } else {
            mostrar_tempo("a verificação do ABS", tempo_execucao);
        }
    } else {
        printf("ABS desativado\n");
    }

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    // Airbag
    touch_pad_read(TOUCH_AIRBAG, &leitura_touch);
    tempo_inicio = obter_tempo();

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  

    if (leitura_touch < 500) {
        printf("Choque detectado! Ativando atuador do Airbag\n");

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        gpio_set_level(GPIO_ATUADOR_AIRBAG, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  

        gpio_set_level(GPIO_ATUADOR_AIRBAG, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        tempo_fim = obter_tempo();
        tempo_execucao = tempo_fim - tempo_inicio;

        if (tempo_execucao > DEADLINE_AIRBAG) {
            printf("Alerta: Tempo de resposta do Airbag excedeu o deadline (%llu ms)\n", tempo_execucao);
        } else {
            mostrar_tempo("o acionamento do airbag", tempo_execucao);
        }
    } else {
        printf("Airbag pronto\n");
    }

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    // Cinto de segurança
    touch_pad_read(TOUCH_CINTO, &leitura_touch);
    tempo_inicio = obter_tempo();

    vTaskDelay(pdMS_TO_TICKS(1) / 1000);  

    if (leitura_touch < 500) {
        printf("Cinto de Segurança colocado\n");

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        gpio_set_level(GPIO_ATUADOR_CINTO, 1);

        vTaskDelay(pdMS_TO_TICKS(5) / 1000);  

        gpio_set_level(GPIO_ATUADOR_CINTO, 0);

        vTaskDelay(pdMS_TO_TICKS(10) / 1000); 

        tempo_fim = obter_tempo();
        tempo_execucao = tempo_fim - tempo_inicio;

        if (tempo_execucao > DEADLINE_CINTO) {
            printf("Alerta: Tempo de resposta do Cinto de Segurança excedeu o deadline (%llu s)\n", tempo_execucao);
        } else {
            mostrar_tempo("a verificação do cinto de segurança", tempo_execucao);
        }
    } else {
        printf("O Cinto de segurança não está colocado\n");
    }
}

// Função para atualizar as amostras de velocidade e consumo
void atualizar_amostras() {
    int nova_velocidade = rand() % 150;  // Gera um valor aleatório para a velocidade entre 0 e 150 km/h
    int novo_consumo = rand() % 20;  // Gera um valor aleatório para o consumo entre 0 e 20 L/100km

    // Substitui o valor mais antigo no vetor (circular)
    vetor_velocidade[indice] = nova_velocidade; // Armazena o valor no vetor
    vetor_consumo[indice] = novo_consumo; // Armazena o valor no vetor
    
    // Atualiza o índice para o próximo 
    indice = (indice + 1) % NUM_AMOSTRAS;
}

// Função para calcular a média de 200 amostras
float calcular_media(int* vetor) {
    int soma = 0;
    for (int i = 0; i < NUM_AMOSTRAS; i++) {
        soma += vetor[i]; // Soma todos os valores do vetor
    }
    return (float)soma / NUM_AMOSTRAS; // Retorna a média das amostras
}

// Função principal
void app_main() {
    configurar_gpio(); // Configura os GPIOs e sensores touch
    
    configurar_timer(); // Configurar o Timer

    srand(time(NULL));  // Inicializa o gerador de números aleatórios

    // Loop principal que verifica os sensores e atualiza os dados periodicamente
    while (1) {
        atualizar_amostras(); // Atualiza as amostras de velocidade e consumo

        // Calcula a média das últimas 200 amostras
        float media_velocidade = calcular_media(vetor_velocidade);
        float media_consumo = calcular_media(vetor_consumo);

        // Atualiza o display e exibe os dados de velocidade e consumo
        printf("\nVelocidade média: %.2f km/h\n", media_velocidade);
        printf("Consumo médio: %.2f L/100km\n", media_consumo);

        verificar_e_controlar_sensores();  // Verifica os sensores e controla os atuadores
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay de 1 segundo (atualização de display)
    }
}