#include "sort_servo.h"

static inline uint32_t angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

int sort_servo_init(sort_servo_config_t *config){
    //initialize GPIOs
    //zero-initialize the GPIO config structure.
    //gpio_config_t io_conf = {};

    // GPIO config for direction output
    /*
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << (config->gpio_servo));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    */
    // create timer
    config->timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    mcpwm_new_timer(&timer_config, &(config->timer));
    
    // create operator
    config->op = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    mcpwm_new_operator(&operator_config, &(config->op));
    
    // connect timer to operator
    mcpwm_operator_connect_timer(config->op, config->timer);

    // create comparator
    config->cmp = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    mcpwm_new_comparator(config->op, &comparator_config, &(config->cmp));

    // create generator
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = config->gpio_servo,
    };
    mcpwm_new_generator(config->op, &generator_config, &(config->gen));
    
    // set the initial compare value, so that the servo will spin to the center position
    mcpwm_comparator_set_compare_value(config->cmp, angle_to_compare(0));

    // go high on counter empty
    mcpwm_generator_set_action_on_timer_event(config->gen, 
                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                                            MCPWM_TIMER_EVENT_EMPTY, 
                                                                            MCPWM_GEN_ACTION_HIGH));
    // go low on compare threshold
    mcpwm_generator_set_action_on_compare_event(config->gen,
                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, 
                                                                                config->cmp, 
                                                                                MCPWM_GEN_ACTION_LOW));

    return 0;
}

int sort_servo_enable(sort_servo_config_t *config){
    mcpwm_timer_enable(config->timer);
    mcpwm_timer_start_stop(config->timer, MCPWM_TIMER_START_NO_STOP);
    return 0;
}

int sort_servo_disable(sort_servo_config_t *config){
    mcpwm_timer_start_stop(config->timer, MCPWM_TIMER_STOP_EMPTY);
    mcpwm_timer_disable(config->timer);
    return 0;
}

int sort_servo_set_angle(sort_servo_config_t *config, int angle){
    mcpwm_comparator_set_compare_value(config->cmp, angle_to_compare(angle));
    return 0;
}

