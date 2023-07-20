#include <stdio.h>
#include "lid_opener.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


opener_config_t opener_config;



void app_main(void)
{
    opener_config.in1 = GPIO_NUM_2;
    opener_config.in2 = GPIO_NUM_4;
    opener_config.sw_fullopen  = GPIO_NUM_22;
    opener_config.sw_fullclose = GPIO_NUM_23;

    opener_init(&opener_config);

    while (true){
        printf("Opening...\n");
        opener_open(&opener_config);
        printf("Open completed!\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        printf("Closing...\n");
        opener_close(&opener_config);
        printf("Close completed!\n");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
