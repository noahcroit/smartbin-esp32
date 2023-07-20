/*
 * lid_opener.h
 *
 */
#ifndef LID_OPENER_H
#define LID_OPENER_H

#define LID_OPENER_NON_BLOCK 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "driver/gpio.h"

#if(LID_OPENER_NON_BLOCK != 0)
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
#endif

typedef struct{
    gpio_num_t in1;
    gpio_num_t in2;
    gpio_num_t sw_fullclose;
    gpio_num_t sw_fullopen;
}opener_config_t;

int opener_init(opener_config_t *config);
int opener_open(opener_config_t *config);
int opener_close(opener_config_t *config);
int opener_isfullopen(opener_config_t *config);
int opener_isfullclose(opener_config_t *config);

#endif
