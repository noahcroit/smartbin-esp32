#include "lid_opener.h"

int opener_init(opener_config_t *config){
    //initialize GPIOs
    //zero-initialize the GPIO config structure.
    gpio_config_t io_conf = {};

    // GPIO config for direction output 
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << (config->in1)) | (1ULL << (config->in2));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    //GPIO config for limit switch
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << (config->sw_fullclose)) | (1ULL << (config->sw_fullopen));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    return 0;
}

/* open the lid
 * This is blocked function if LID_OPEN_NON_BLOCK is not set to 0
 */
int opener_open(opener_config_t *config){
    gpio_set_level(config->in1, 1);
    while (!opener_isfullopen(config)){
#if(LID_OPEN_NON_BLOCK != 0)
        vTaskDelay(50 / portTICK_PERIOD_MS);
#endif
    }
    gpio_set_level(config->in1, 0);
    return 0;
}

/* close the lid
 * This is blocked function if LID_OPEN_NON_BLOCK is not set to 0
 */
int opener_close(opener_config_t *config){
    gpio_set_level(config->in2, 1);
    while (!opener_isfullclose(config)){
#if(LID_OPEN_NON_BLOCK != 0)
        vTaskDelay(50 / portTICK_PERIOD_MS);
#endif
    }
    gpio_set_level(config->in2, 0);
    return 0;
}

/*
 * Check lid limit switch's status
 *
 */
int opener_isfullopen(opener_config_t *config){
    return !gpio_get_level(config->sw_fullopen);
}
int opener_isfullclose(opener_config_t *config){
    return !gpio_get_level(config->sw_fullclose);
}
