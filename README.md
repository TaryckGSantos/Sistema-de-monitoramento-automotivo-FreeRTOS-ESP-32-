# Sistema-de-monitoramento-automotivo-FreeRTOS-ESP-32-

Implementação de um **sistema de monitoramento e controle automotivo** utilizando o **FreeRTOS** na **ESP32**, desenvolvido para atender aos requisitos de uma fabricante de automóveis. O objetivo é monitorar e controlar três subsistemas do veículo (motor, frenagem e equipamentos de suporte à vida), garantindo respostas em tempo real a eventos críticos e apresentando as informações em um display integrado.

## Funcionalidades do Sistema
- **Monitoramento e Controle de Subsistemas:**
  - **Motor:** Injeção eletrônica e temperatura interna.
  - **Frenagem:** Sistema ABS das rodas dianteiras.
  - **Equipamentos de Suporte à Vida:** Airbag e cinto de segurança.
- **Atualização em Tempo Real:**
  - Respostas baseadas em deadlines específicos para cada subsistema.
  - Apresentação de alertas e estados no display, com atualização a cada 1 segundo.
- **Coleta de Dados do Motor:**
  - Monitoramento da velocidade média e consumo médio de gasolina, com 200 amostras atualizadas continuamente.

## Implementações
O projeto está dividido em três versões, cada uma representando uma abordagem de arquitetura de software:

1. **Executivo Cíclico:** 
   - Todas as tarefas são executadas em sequência fixa, dentro de um laço principal.
2. **Laço de Repetição com Tratador de Interrupções:** 
   - Tarefas são executadas com base em eventos gerados por interrupções externas.
3. **Microkernel com Threads:** 
   - Uso de threads criadas com o FreeRTOS (`xTaskCreate`) para multitarefa baseada em prioridades.

## Requisitos Técnicos
- **Hardware:** Placa ESP32.
- **Software:** FreeRTOS com SDK ESP-IDF.
- **Conexão de Sensores e Atuadores:** 
  - 5 sensores e 5 atuadores distribuídos nos subsistemas.
  - Comunicação via barramento com velocidade de 1 Mbps e latência de 10 µs.
