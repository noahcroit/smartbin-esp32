#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "driver/i2c.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "lid_opener.h"
#include "sort_servo.h"
#include "vl53l0x.h"
#include "hx711.h"

/*
 * Define for SmartBin
 *
 */
#define USE_SIMULATED_PRESENSE_BUTTON 0
#define USE_SIMULATED_YOLO_BUTTON     0 

#define BINSTATE_IDLE           0
#define BINSTATE_YOLO           1
#define BINSTATE_SORTCTRL       2
#define TIMEOUT_SEC_YOLO        20
#define YOLO_OBJCLASS_COLDCUP   'C'
#define YOLO_OBJCLASS_HOTCUP    'H'
#define YOLO_OBJCLASS_OTHER     'O'
#define YOLO_OBJCLASS_NONE      0
#define MQTT_DISCONNECTED       0
#define MQTT_CONNECTED          1
#define MQTT_TOPIC_YOLOREQUEST  "YOLO/Request?"
#define MQTT_TOPIC_YOLORESULT   "YOLO/Result"
#define MQTT_TOPIC_ALARM        "Alarm"
#define SERVO_ANGLE_L_COLDCUP   -30
#define SERVO_ANGLE_R_COLDCUP   -30
#define SERVO_ANGLE_L_HOTCUP    40
#define SERVO_ANGLE_R_HOTCUP    40
#define SERVO_ANGLE_L_OTHER     0
#define SERVO_ANGLE_R_OTHER     0
#define SERVO_ANGLE_L_IDLE      0
#define SERVO_ANGLE_R_IDLE      0
#define OVERWEIGHT_THRESHOLD    (int32_t)150000
#define ACTIVATE_DISTANCE_MM    210
#define DISTANCE_SENSOR_I2C_PORT    I2C_NUM_0  
#define DISTANCE_SENSOR_I2C_PIN_SCL GPIO_NUM_22  
#define DISTANCE_SENSOR_I2C_PIN_SDA GPIO_NUM_21  
#define DISTANCE_SENSOR_I2C_ADDRESS 0x29  
#define DISTANCE_SENSOR_XSHUT       -1  
#define DISTANCE_SENSOR_2V8         0
#define LED_MQTTCONNECTED 13
#define LED_BINSTATE_IDLE 12
#define LED_BINSTATE_YOLO 14
#define LED_BINSTATE_SORT 27

typedef struct{
    int8_t binState;
    int8_t mqttConnected; 
    int8_t yoloReady;
    char objClass;
}smartbin_t;

int isObjectPresent(smartbin_t *bin);
int isObjectOverweight(smartbin_t *bin);
int isYoloReady(smartbin_t *bin);

/* 
 * Global Var for smartbin
 *
 */
smartbin_t smartbin;
esp_mqtt_client_handle_t client = NULL;
char buf_yolo_request[10];
char buf_yolo_result[10];
char buf_alarm[10];
QueueHandle_t queue_yolo_request;
QueueHandle_t queue_yolo_result;
QueueHandle_t queue_alarm;
uint16_t distance_mm=1000;
int32_t weight=0;



static const char *TAG = "SYSTEM_LOG";
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        smartbin.mqttConnected = MQTT_CONNECTED;
        gpio_set_level(LED_MQTTCONNECTED, 0);

        //msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

        /* Subscribe of every tags should be here, after connection is success
         *
         */
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_YOLORESULT, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        smartbin.mqttConnected = MQTT_DISCONNECTED;
        gpio_set_level(LED_MQTTCONNECTED, 1);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        xQueueSend(queue_yolo_result, (void*)event->data, (TickType_t)0);
        smartbin.yoloReady = 1;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}



void task_readObjectSensor(){
    /*
     * Distance sensor VL53L0X configuration
     *
     */
    ESP_LOGI(TAG, "VL53L0X Configuration ***");
    vl53l0x_t *v_sensor;
    v_sensor = vl53l0x_config(DISTANCE_SENSOR_I2C_PORT, 
                              DISTANCE_SENSOR_I2C_PIN_SCL,
                              DISTANCE_SENSOR_I2C_PIN_SDA,
                              DISTANCE_SENSOR_XSHUT, 
                              DISTANCE_SENSOR_I2C_ADDRESS,
                              DISTANCE_SENSOR_2V8);
    if(v_sensor == NULL){
        ESP_LOGI(TAG, "Cannot see I2C device");
    }
    else{
        ESP_LOGI(TAG, "VL53L0X Config OK!");
        vl53l0x_init (v_sensor);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        vl53l0x_startContinuous(v_sensor, 1000);
    }
    
    /*
     * Weight sensor amplifier HX711 configuration
     *
     */

    hx711_t dev = {
        .dout = CONFIG_HX711_DOUT_GPIO,
        .pd_sck = CONFIG_HX711_PD_SCK_GPIO,
        .gain = HX711_GAIN_A_64
    };
    // initialize device
    ESP_ERROR_CHECK(hx711_init(&dev));



    vTaskDelay(1000 / portTICK_PERIOD_MS);
    while (true){
        // read object distance
        distance_mm = vl53l0x_readRangeContinuousMillimeters(v_sensor);
        
        // read object weight
        esp_err_t r = hx711_wait(&dev, 500);
        if (r != ESP_OK){
            ESP_LOGE(TAG, "Device not found: %d (%s)\n", r, esp_err_to_name(r));
            continue;
        }
        r = hx711_read_average(&dev, CONFIG_HX711_AVG_TIMES, &weight);
        if (r != ESP_OK){
            ESP_LOGE(TAG, "Could not read data: %d (%s)\n", r, esp_err_to_name(r));
            continue;
        }

        ESP_LOGI(TAG, "object distance=%d, object weight=%d", distance_mm, weight);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void task_binstatemachine(){
    
    /*
     * Initialize Smartbin Params
     *
     */
    smartbin.binState = BINSTATE_IDLE;
    smartbin.yoloReady = 0;

    /*
     * Initialize LEDs
     *
     */
    //zero-initialize the GPIO config structure.
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<(LED_MQTTCONNECTED)) | (1ULL<<(LED_BINSTATE_IDLE)) | (1ULL<<(LED_BINSTATE_YOLO)) | (1ULL<<(LED_BINSTATE_SORT));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(LED_MQTTCONNECTED, 1);
    gpio_set_level(LED_BINSTATE_IDLE, 1);
    gpio_set_level(LED_BINSTATE_YOLO, 1);
    gpio_set_level(LED_BINSTATE_SORT, 1);


    /*
     * Lid Opener Configuration
     *
     */
    opener_config_t opener_config;
    opener_config.in1 = GPIO_NUM_2;
    opener_config.in2 = GPIO_NUM_4;
    opener_config.sw_fullopen  = GPIO_NUM_5;
    opener_config.sw_fullclose = GPIO_NUM_23;
    opener_config.duty = 0;
    opener_init(&opener_config);
    opener_setduty(&opener_config, 4000);
    
    /*
     * Sorting Servo Configuration
     *
     */
    sort_servo_config_t servo_l;
    sort_servo_config_t servo_r;
    servo_l.gpio_servo = GPIO_NUM_18;
    servo_r.gpio_servo = GPIO_NUM_19;
    sort_servo_init(&servo_l);
    sort_servo_init(&servo_r);
    sort_servo_enable(&servo_l);
    sort_servo_enable(&servo_r);
    sort_servo_set_angle(&servo_l, SERVO_ANGLE_L_IDLE);
    sort_servo_set_angle(&servo_r, SERVO_ANGLE_R_IDLE);
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    sort_servo_disable(&servo_l);
    sort_servo_disable(&servo_r);
    
    /*
     * For testing only : Button for simulation of Presense sensor and YOLO result
     *
     */
#if USE_SIMULATED_PRESENSE_BUTTON == 1
#define SIM_BTN_PRESENSE_SENSOR  (15)
    ESP_LOGI(TAG, "Simulation is used...");
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SIM_BTN_PRESENSE_SENSOR);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
#endif

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    int overweight_cnt=0;

    while (true){
        switch (smartbin.binState){
            case BINSTATE_IDLE:
                ESP_LOGI(TAG, "IDLE STATE");
                if (isObjectPresent(&smartbin)){
                    if(isObjectOverweight(&smartbin)){
                        overweight_cnt++;
                        if(overweight_cnt > 5){
                            xQueueSend(queue_alarm, (void*)"W", (TickType_t)0);
                            vTaskDelay(2000 / portTICK_PERIOD_MS);
                            overweight_cnt = 0;
                        }
                    }
                    else{
                        // Move to YOLO state
                        smartbin.binState = BINSTATE_YOLO;
                        gpio_set_level(LED_BINSTATE_IDLE, 1);
                        gpio_set_level(LED_BINSTATE_YOLO, 0);
                        ESP_LOGI(TAG, "GO TO YOLO STATE");
                    }
                }
                break;

            case BINSTATE_YOLO:
                ESP_LOGI(TAG, "YOLO STATE");
                // Publish MQTT Request for YOLO service
                //
                // Wait the result
                int timeout_yolo;
                timeout_yolo = TIMEOUT_SEC_YOLO;
                xQueueSend(queue_yolo_request, (void*)"Y", (TickType_t)0);
                ESP_LOGI(TAG, "Publish YOLO request, Wait for YOLO...");
                while (!isYoloReady(&smartbin)){
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    timeout_yolo--;
                    if (timeout_yolo <= 0){
                        break;
                    }
                }
                if (timeout_yolo <= 0){
                    smartbin.binState = BINSTATE_IDLE;
                    gpio_set_level(LED_BINSTATE_YOLO, 1);
                    gpio_set_level(LED_BINSTATE_IDLE, 0);
                    xQueueReset(queue_yolo_result);
                    ESP_LOGI(TAG, "Timeout, GO TO IDLE");
                }
                else{
                    // Move to SORTCTRL state
                    smartbin.binState = BINSTATE_SORTCTRL;
                    gpio_set_level(LED_BINSTATE_YOLO, 1);
                    gpio_set_level(LED_BINSTATE_SORT, 0);
                }
                break;

            case BINSTATE_SORTCTRL:
                ESP_LOGI(TAG, "SORT STATE");
                // Read YOLO result
                //
                // Select servo angle coresponds to YOLO's result
                char rxBuf;
                if (queue_yolo_result != 0){
                    xQueueReceive(queue_yolo_result, &(rxBuf), (TickType_t)10);
                    ESP_LOGI(TAG, "load YOLO result in queue");
                    smartbin.objClass = rxBuf;
                }
                else{
                    ESP_LOGI(TAG, "No YOLO result in queue");
                    smartbin.objClass = YOLO_OBJCLASS_NONE;
                    break;
                }
                int angle_l, angle_r;
                if (smartbin.objClass == YOLO_OBJCLASS_COLDCUP){
                    ESP_LOGI(TAG, "Sort to COLDCUP slot");
                    angle_l = SERVO_ANGLE_L_COLDCUP;
                    angle_r = SERVO_ANGLE_R_COLDCUP;
                }
                else if (smartbin.objClass == YOLO_OBJCLASS_HOTCUP){
                    ESP_LOGI(TAG, "Sort to HOTCUP slot");
                    angle_l = SERVO_ANGLE_L_HOTCUP;
                    angle_r = SERVO_ANGLE_R_HOTCUP;
                }
                else if (smartbin.objClass == YOLO_OBJCLASS_OTHER){
                    ESP_LOGI(TAG, "Sort to OTHER slot");
                    angle_l = SERVO_ANGLE_L_OTHER;
                    angle_r = SERVO_ANGLE_R_OTHER;
                }
                else{
                    // Reset state to IDLE
                    smartbin.binState = BINSTATE_IDLE;
                    gpio_set_level(LED_BINSTATE_SORT, 1);
                    gpio_set_level(LED_BINSTATE_IDLE, 0);
                    ESP_LOGI(TAG, "Not any amazon product, GO TO IDLE");
                    break;
                }
                ESP_LOGI(TAG, "Control Servo");
                sort_servo_enable(&servo_l);
                sort_servo_enable(&servo_r);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                sort_servo_set_angle(&servo_l, angle_l);
                sort_servo_set_angle(&servo_r, angle_r);
                vTaskDelay(2000 / portTICK_PERIOD_MS);

                // Open Lid, Let object fall into bin
                ESP_LOGI(TAG, "Opening");
                opener_open(&opener_config);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                opener_close(&opener_config);
                ESP_LOGI(TAG, "Closing");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                sort_servo_set_angle(&servo_l, SERVO_ANGLE_L_IDLE);
                sort_servo_set_angle(&servo_r, SERVO_ANGLE_R_IDLE);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                sort_servo_disable(&servo_l);
                sort_servo_disable(&servo_r);

                // Reset state to IDLE
                smartbin.binState = BINSTATE_IDLE;
                gpio_set_level(LED_BINSTATE_SORT, 1);
                gpio_set_level(LED_BINSTATE_IDLE, 0);
                break;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void task_mqtt_connection(){

    smartbin.mqttConnected = MQTT_DISCONNECTED; 
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    
    int msg_id;
    char rxBuf='N';

    while (true){
        if (smartbin.mqttConnected == MQTT_DISCONNECTED){
            ESP_ERROR_CHECK(example_connect());
            mqtt_app_start();
            vTaskDelay(3000 / portTICK_PERIOD_MS);
        }
        else{
            /*
             * Checking Queue
             *
             */
            if (queue_yolo_request != 0){
                xQueueReceive(queue_yolo_request, &(rxBuf), (TickType_t)10);
                if (rxBuf == 'Y'){
                    msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_YOLOREQUEST, "Y", 0, 0, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                    rxBuf = 'N';
                }
            }
            
            if (queue_alarm != 0){
                xQueueReceive(queue_alarm, &(rxBuf), (TickType_t)10);
                if (rxBuf == 'W'){
                    msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_ALARM, "W", 0, 0, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                    rxBuf = 'N';
                }
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // create message queue for YOLO Request
    queue_yolo_request = xQueueCreate(5, sizeof(buf_yolo_request));
    queue_yolo_result = xQueueCreate(5, sizeof(buf_yolo_result));
    queue_alarm = xQueueCreate(5, sizeof(buf_alarm));

    // task handler
    TaskHandle_t task_handler_1 = NULL;
    TaskHandle_t task_handler_2 = NULL;
    TaskHandle_t task_handler_3 = NULL;

    // create tasks
    xTaskCreate(&task_readObjectSensor, "task object detection sensor", 4096, NULL, 5, &task_handler_3);
    xTaskCreate(&task_mqtt_connection, "task mqtt check connection", 4096, NULL, 5, &task_handler_1);
    xTaskCreate(&task_binstatemachine, "task smartbin", 4096, NULL, 5, &task_handler_2);

    while (true){
        // nothing is running in this
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}



/*
 * All SmartBin functions
 *
 */
int isObjectPresent(smartbin_t *bin){
#if USE_SIMULATED_PRESENSE_BUTTON == 1
    if(!gpio_get_level(SIM_BTN_PRESENSE_SENSOR)){
        ESP_LOGI(TAG, "Presense!");
        return 1;
    }
    return 0;
#endif
    if(distance_mm < ACTIVATE_DISTANCE_MM){
        ESP_LOGI(TAG, "Presense!");
        return 1;
    }
    return 0;
}

int isObjectOverweight(smartbin_t *bin){
    if(weight > OVERWEIGHT_THRESHOLD){
        ESP_LOGI(TAG, "Alarm, Overweight!");
        return 1;
    }
    return 0;
}

int isYoloReady(smartbin_t *bin){
#if USE_SIMULATED_YOLO_BUTTON == 1
#else
    if (bin->yoloReady){
        bin->yoloReady = 0;
        return 1;
    }
    return 0;
#endif
}

/*
 * Hardware Testing
 *
 */

