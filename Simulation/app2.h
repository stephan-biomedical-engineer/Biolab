/*
 * app.h
 *
 *  Created on: Jan 12, 2025
 *      Author: stephan
 */

#ifndef APP_H_
#define APP_H_

typedef struct {
    float V_sensor_old;
    float V_sensor_new;
    float v_m_old;
    float v_m_new;
    float u_old;
    float u_new;
    float I;
} FA;

typedef struct {
    float V_sensor;
    float v_m_old;
    float v_m_new;
    float u_old;
    float u_new;
    float I;
} SA;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} FS_Channels;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} RS_Channels;

void initialize_taxels(Taxel *taxels, int num);
void app_setup(void);
void select_row(uint8_t coluna);
void update_taxels(FA *fast_response, SA *regular_response,uint16_t *adc_values, int num);
float normalized_signal(float V);
float absolute_signal(float V1, float V2);

#endif /* APP_H_ */
