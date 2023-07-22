#include <stdio.h>
#include "lid_opener.h"
#include "sort_servo.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//#include "esp_log.h"
//#include "driver/mcpwm_prelude.h"



// Please consult the datasheet of your servo before changing the following parameters
/*
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO1_PULSE_GPIO             2        // GPIO connects to the PWM signal line
#define SERVO2_PULSE_GPIO             4        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}
*/

void app_main(void)
{
    /*
     * Lid Opener Configuration
     *
     */
    opener_config_t opener_config;
    opener_config.in1 = GPIO_NUM_2;
    opener_config.in2 = GPIO_NUM_4;
    opener_config.sw_fullopen  = GPIO_NUM_22;
    opener_config.sw_fullclose = GPIO_NUM_23;
    opener_init(&opener_config);
    
    /*
     * Sorting Servo Configuration
     *
     */
    sort_servo_config_t servo_l;
    sort_servo_config_t servo_r;
    servo_l.gpio_servo = 2;
    servo_r.gpio_servo = 4;
    sort_servo_init(&servo_l);
    sort_servo_init(&servo_r);
    sort_servo_enable(&servo_l);
    sort_servo_enable(&servo_r);


    int angle1 = 0;
    int angle2 = 0;
    int step1 = 2;
    int step2 = 5;
    sort_servo_set_angle(&servo_l, angle1);
    sort_servo_set_angle(&servo_r, angle2);
    while (true){
        /*
        printf("Opening...\n");
        opener_open(&opener_config);
        printf("Open completed!\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        printf("Closing...\n");
        opener_close(&opener_config);
        printf("Close completed!\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        */
        /*
        sort_servo_set_angle(&servo_l, angle1);
        sort_servo_set_angle(&servo_r, angle2);

        //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
        vTaskDelay(pdMS_TO_TICKS(500));
        if ((angle1 + step1) > 60 || (angle1 + step1) < -60) {
            step1 *= -1;
        }
        angle1 += step1;
        
        if ((angle2 + step2) > 60 || (angle2 + step2) < -60) {
            step2 *= -1;
        }
        angle2 += step2;
        */
        sort_servo_enable(&servo_l);
        sort_servo_enable(&servo_r);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        sort_servo_disable(&servo_l);
        sort_servo_disable(&servo_r);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
