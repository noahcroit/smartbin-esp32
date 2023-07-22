/*
 * sort_servo.h
 *
 */
#ifndef SORT_SERVO_H
#define SORT_SERVO_H



#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             2        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

#if(LID_OPENER_NON_BLOCK != 0)
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
#endif

typedef struct{
    gpio_num_t gpio_servo;
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t  op;
    mcpwm_cmpr_handle_t  cmp;
    mcpwm_gen_handle_t   gen;

}sort_servo_config_t;


int sort_servo_init(sort_servo_config_t *config);
int sort_servo_enable(sort_servo_config_t *config);
int sort_servo_disable(sort_servo_config_t *config);
int sort_servo_set_angle(sort_servo_config_t *config, int angle);

#endif
