/*
 * app.c
 *
 *  Created on: Sep 23, 2024
 *      Author: stephan
 */

#include "main.h"
#include "app.h"
#include "hw.h"
#include "Izhikevich.h"

// Parâmetros do modelo de Izhikevich
#define a 0.02
#define b 0.2
#define c -65
#define d 8
#define vth 30
#define V_min 1.5
#define V_max 3.3
#define dt 2
#define G 20

// Endereços definidos pela HAL
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim6;

// Manipulador de colunas para leitura dos taxels
extern uint8_t colunaAtual;

// Variáveis de controle do ADC
const uint8_t buffer_size = 4;
uint32_t adc_buffer[4];
uint8_t adc_ready = 0;

uint32_t sensor_map[4][4];

typedef struct {
    float V_sensor_old;
    float V_sensor_new;
    float v_m_old;
    float v_m_new;
    float u_old;
    float u_new;
    float I;
} Taxel;

#define NUM_TAXELS 16
Taxel taxels[NUM_TAXELS];

GPIO_Map gpio_map[NUM_TAXELS] = {
    {GPIOD, GPIO_PIN_13},
    {GPIOD, GPIO_PIN_12},
    {GPIOD, GPIO_PIN_11},
    {GPIOE, GPIO_PIN_2},
    {GPIOC, GPIO_PIN_2},
    {GPIOF, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_2},
    {GPIOE, GPIO_PIN_13},
    {GPIOF, GPIO_PIN_15},
    {GPIOG, GPIO_PIN_14},
    {GPIOG, GPIO_PIN_9},
    {GPIOE, GPIO_PIN_8},
    {GPIOE, GPIO_PIN_7},
    {GPIOE, GPIO_PIN_10},
    {GPIOE, GPIO_PIN_12}
};

void initialize_taxels(Taxel *taxels, int num) {
    for (int i = 0; i < num; i++) {
        taxels[i].V_sensor_old = 0;
        taxels[i].V_sensor_new = 3.3; // Exemplo de valor inicial
        taxels[i].v_m_old = -30;
        taxels[i].v_m_new = -30;
        taxels[i].u_old = 0;
        taxels[i].u_new = 0;
        taxels[i].I = 0;
    }
}


void app_setup(void)
{
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, buffer_size); // Iniciar ADC com DMA
    HAL_TIM_Base_Start_IT(&htim6);
    initialize_taxels(taxels, NUM_TAXELS);
}


void AlternarColuna(uint8_t coluna)
{
    // Define todas as colunas como GPIO_PIN_SET (desativadas)
    HAL_GPIO_WritePin(COLUNA_0_GPIO_Port, COLUNA_0_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(COLUNA_1_GPIO_Port, COLUNA_1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(COLUNA_2_GPIO_Port, COLUNA_2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(COLUNA_3_GPIO_Port, COLUNA_3_Pin, GPIO_PIN_SET);

    // Ativa apenas a coluna especificada
    switch (coluna)
    {
        case 0:
            HAL_GPIO_WritePin(COLUNA_0_GPIO_Port, COLUNA_0_Pin, GPIO_PIN_RESET);
            break;
        case 1:
            HAL_GPIO_WritePin(COLUNA_1_GPIO_Port, COLUNA_1_Pin, GPIO_PIN_RESET);
            break;
        case 2:
            HAL_GPIO_WritePin(COLUNA_2_GPIO_Port, COLUNA_2_Pin, GPIO_PIN_RESET);
            break;
        case 3:
            HAL_GPIO_WritePin(COLUNA_3_GPIO_Port, COLUNA_3_Pin, GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    static uint8_t coluna_atual = 0; // Coluna ativa

    if (htim == &htim6) // Verifica se a interrupção veio do Timer 6
    {
        // Buffer temporário para armazenar leituras do ADC
        uint16_t adc_values[4]; 

        // Suponha que adc_buffer contém as leituras da coluna atual
        for (uint8_t coluna; coluna < 4; coluna++) {
            for (uint8_t linha = 0; linha < 4; linha++) {
                adc_values[linha] = adc_buffer[linha];
            }
            update_taxels(&taxels[coluna_atual * 4], adc_values, 4);
        }

        // Atualiza os taxels da coluna atual
        update_taxels(&taxels[coluna_atual * 4], adc_values, 4);

        // Alterna para a próxima coluna
        AlternarColuna(coluna_atual);
        coluna_atual = (coluna_atual + 1) % 4;
    }
}


void update_taxels(Taxel *taxels, uint16_t *adc_values, int num) {
    for (int i = 0; i < num; i++) {
        // Atualiza a leitura do sensor
        taxels[i].V_sensor_old = taxels[i].V_sensor_new;
        taxels[i].V_sensor_new = adc_values[i] * (3.3 / 4095.0); // Converte ADC para tensão

        V_old_normalized = NormalizedSignal(taxels[i].V_sensor_old);
        V_new_normalized = NormalizedSignal(taxels[i].V_sensor_new);

        // Calcula a corrente de entrada com base na leitura do sensor
        taxels[i].I = G*(V_new_normalized - V_old_normalized) / dt;

        // Atualiza o potencial de membrana e recuperação usando o modelo de Izhikevich
        taxels[i].v_m_old = taxels[i].v_m_new;
        taxels[i].u_old = taxels[i].u_new;

        taxels[i].v_m_new = taxels[i].v_m_old + dt * (0.04 * taxels[i].v_m_old * taxels[i].v_m_old 
                             + 5 * taxels[i].v_m_old + 140 - taxels[i].u_old + taxels[i].I);

        taxels[i].u_new = taxels[i].u_old + dt * (a * (b * taxels[i].v_m_old - taxels[i].u_old));

        // Detecta spikes
        if (taxels[i].v_m_new >= vth) {
            taxels[i].v_m_new = c;
            taxels[i].u_new += d;
            HAL_GPIO_WritePin(gpio_map[i].port, gpio_map[i].pin, GPIO_PIN_SET); // Spike
            HAL_GPIO_WritePin(gpio_map[i].port, gpio_map[i].pin, GPIO_PIN_RESET); 
        }
    }
}


float NormalizedSignal(float V){
    // Normalização dos valores de tensão lidos pelo sensor
    float V_normalized = (V - V_min)/(V_max - V_min);
    return V_normalized;
}



/* taxels
 *
 * 00 04 08 12
 * 01 05 09 13
 * 02 06 10 14
 * 03 07 11 15
 */

