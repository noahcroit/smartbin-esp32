#include "lid_opener.h"

static int opener_ledc_init(opener_config_t *config){
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Prepare and then apply the LEDC PWM channel configuration for IN1, IN2
    ledc_channel_config_t ledc_channel_in1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = config->in1,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_in1);

    ledc_channel_config_t ledc_channel_in2 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = config->in2,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_in2);


    return 0;
}

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
    
    opener_ledc_init(config);

    return 0;
}

/*
 * Set lid motor's speed
 * int speed : 0-100 % (as a pwm duty)
 */
int opener_setduty(opener_config_t *config, int duty){
    if(duty > LEDC_DUTY_MAX) duty = LEDC_DUTY_MAX;
    config->duty = duty;
    return 0;
}

/* open the lid
 * This is blocked function if LID_OPEN_NON_BLOCK is not set to 0
 */
int opener_open(opener_config_t *config){
    //gpio_set_level(config->in1, 1);
    ledc_channel_config_t ledc_channel_in1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = config->in1,
        .duty           = config->duty, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_in1);
    while (!opener_isfullopen(config)){
#if(LID_OPEN_NON_BLOCK != 0)
        vTaskDelay(50 / portTICK_PERIOD_MS);
#endif
    }
    //gpio_set_level(config->in1, 0);
    ledc_channel_in1.duty = 0;
    ledc_channel_config(&ledc_channel_in1);
    
    return 0;
}

/* close the lid
 * This is blocked function if LID_OPEN_NON_BLOCK is not set to 0
 */
int opener_close(opener_config_t *config){
    //gpio_set_level(config->in2, 1);
    ledc_channel_config_t ledc_channel_in2 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = config->in2,
        .duty           = config->duty, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_in2);
    while (!opener_isfullclose(config)){
#if(LID_OPEN_NON_BLOCK != 0)
        vTaskDelay(50 / portTICK_PERIOD_MS);
#endif
    }
    //gpio_set_level(config->in2, 0);
    ledc_channel_in2.duty = 0;
    ledc_channel_config(&ledc_channel_in2);
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
