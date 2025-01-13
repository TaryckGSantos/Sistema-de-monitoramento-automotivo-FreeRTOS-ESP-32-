# Análise de Performance - Versão Microkernel

Métodos e resultados obtidos na análise temporal da versão Microkernel do sistema de monitoramento e controle automotivo desenvolvido para a ESP32 utilizando o FreeRTOS. O objetivo é avaliar o desempenho das tarefas do sistema, garantindo o cumprimento dos deadlines.

---

## A análise foi conduzida com base nos seguintes critérios e etapas:

1. **Especificação do Modelo de Tarefa:**
   - **Tempo de Execução:** Medido para cada tarefa considerando o comportamento no pior caso.
   - **Tempo de Interferência:** Calculado com base na distribuição de prioridades entre as tarefas.
   - **Período de Ocorrência:** Classificação das tarefas como periódicas, esporádicas ou aperiódicas.
   - **Deadline:** Determinado para cada tarefa de acordo com os requisitos do sistema.

2. **Medições Realizadas:**
   - **WCET (Worst Case Execution Time):** Identificado para cada tarefa.
   - **WCRT (Worst Case Response Time):** Calculado com base nos tempos de execução e interferência.

3. **Hardware Considerado:**
   - Características do microcontrolador ESP32, incluindo:
     - Velocidade do barramento (1 Mbps).
     - Latência de propagação de 10 µs.
     - Limitações de processamento e memória.

4. **Critérios medidos:**
   - Conceito de (m,k)-firme com uma janela de 10 ativações para cada tarefa.
   - **HWM (High Water Mark)** de 100%.
   - Fator skip entre perdas de deadlines.

5. **Medições de Tempo:**
   - Realizadas pelo menos 20 amostras por tarefa para garantir a precisão dos resultados.
