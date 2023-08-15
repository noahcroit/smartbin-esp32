/*
 * lid_opener.h
 *
 */
#ifndef LID_OPENER_H
#define LID_OPENER_H

#define LID_OPENER_NON_BLOCK 0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

#if(LID_OPENER_NON_BLOCK != 0)
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
#endif

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY_MAX           (8191) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY          (5000) // Frequency in Hertz. Set frequency at 5 kHz

typedef struct{
    gpio_num_t in1;
    gpio_num_t in2;
    gpio_num_t sw_fullclose;
    gpio_num_t sw_fullopen;
    int duty;
}opener_config_t;

int opener_init(opener_config_t *config);
int opener_setduty(opener_config_t *config, int duty);
int opener_open(opener_config_t *config);
int opener_close(opener_config_t *config);
int opener_isfullopen(opener_config_t *config);
int opener_isfullclose(opener_config_t *config);

#endif
