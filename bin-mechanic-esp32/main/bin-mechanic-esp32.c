#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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



/*
 * Define for SmartBin
 *
 */
#define USE_SIMULATED_BUTTON 1

#define BINSTATE_IDLE       0
#define BINSTATE_YOLO       1
#define BINSTATE_SORTCTRL   2
#define TIMEOUT_SEC_YOLO    10
#define YOLO_OBJCLASS_COLDCUP   1
#define YOLO_OBJCLASS_HOTCUP    2
#define YOLO_OBJCLASS_OTHER     3
#define SERVO_ANGLE_L_COLDCUP   -60
#define SERVO_ANGLE_R_COLDCUP   -60
#define SERVO_ANGLE_L_HOTCUP    60
#define SERVO_ANGLE_R_HOTCUP    60
#define SERVO_ANGLE_L_OTHER     0
#define SERVO_ANGLE_R_OTHER     0
#define SERVO_ANGLE_L_IDLE      0
#define SERVO_ANGLE_R_IDLE      0

typedef struct{
    int binState;
    int objDetected;
    int yoloReady;
    int objClass;

}smartbin_t;

int isObjectPresent(smartbin_t *bin);
int isYoloReady(smartbin_t *bin);



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
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
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

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}



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
    
    /*
     * For testing only : Button for simulation of Presense sensor and YOLO result
     *
     */
#if USE_SIMULATED_BUTTON == 1
#define SIM_BTN_PRESENSE_SENSOR  (12)
#define SIM_BTN_YOLO_READY       (13)
#define SIM_BTN_HOTCUP           (14)
#define SIM_BTN_COLDCUP          (27)
    ESP_LOGI(TAG, "Simulation is used...");
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << SIM_BTN_PRESENSE_SENSOR) | (1ULL << SIM_BTN_YOLO_READY) | (1ULL << SIM_BTN_COLDCUP) | (1ULL << SIM_BTN_HOTCUP);
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

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    //ESP_ERROR_CHECK(example_connect());
    //mqtt_app_start();
    smartbin_t smartbin;
    smartbin.binState = BINSTATE_IDLE;

    while (true){
        switch (smartbin.binState){
            case BINSTATE_IDLE:
                ESP_LOGI(TAG, "IDLE STATE");
                if (isObjectPresent(&smartbin)){
                    smartbin.binState = BINSTATE_YOLO;
                    ESP_LOGI(TAG, "GO TO YOLO STATE");
                }
                break;

            case BINSTATE_YOLO:
                ESP_LOGI(TAG, "YOLO STATE");
                // Publish MQTT Request for YOLO service
                //
                // Wait the result
                int timeout_yolo;
                timeout_yolo = TIMEOUT_SEC_YOLO;
                ESP_LOGI(TAG, "Wait for YOLO");
                while (!isYoloReady(&smartbin)){
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    timeout_yolo--;
                    if (timeout_yolo <= 0){
                        smartbin.binState = BINSTATE_IDLE;
                        ESP_LOGI(TAG, "Timeout, GO TO IDLE");
                        break;
                    }
                }
                smartbin.binState = BINSTATE_SORTCTRL;
                break;

            case BINSTATE_SORTCTRL:
                ESP_LOGI(TAG, "SORT STATE");
                // Read YOLO result
                //
                // Select servo angle coresponds to YOLO's result
                int angle_l, angle_r;
                if (smartbin.objClass == YOLO_OBJCLASS_COLDCUP){
                    angle_l = SERVO_ANGLE_L_COLDCUP;
                    angle_r = SERVO_ANGLE_R_COLDCUP;
                }
                else if (smartbin.objClass == YOLO_OBJCLASS_HOTCUP){
                    angle_l = SERVO_ANGLE_L_HOTCUP;
                    angle_r = SERVO_ANGLE_R_HOTCUP;
                }
                else if (smartbin.objClass == YOLO_OBJCLASS_OTHER){
                    angle_l = SERVO_ANGLE_L_OTHER;
                    angle_r = SERVO_ANGLE_R_OTHER;
                }
                else{
                    smartbin.binState = BINSTATE_IDLE;
                    ESP_LOGI(TAG, "GO TO IDLE");
                    break;
                }
                sort_servo_set_angle(&servo_l, angle_l);
                sort_servo_set_angle(&servo_r, angle_r);
                vTaskDelay(2000 / portTICK_PERIOD_MS);

                // Open Lid, Let object fall into bin
                opener_open(&opener_config);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                opener_close(&opener_config);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                smartbin.binState = BINSTATE_IDLE;
                sort_servo_set_angle(&servo_l, SERVO_ANGLE_L_IDLE);
                sort_servo_set_angle(&servo_r, SERVO_ANGLE_R_IDLE);
                break;
        }

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}



/*
 * All SmartBin functions
 *
 */
int isObjectPresent(smartbin_t *bin){
#if USE_SIMULATED_BUTTON == 1
    if(!gpio_get_level(SIM_BTN_PRESENSE_SENSOR)){
        ESP_LOGI(TAG, "Presense!");
        return 1;
    }
    return 0;
#endif
}

int isYoloReady(smartbin_t *bin){
#if USE_SIMULATED_BUTTON == 1
    if(!gpio_get_level(SIM_BTN_YOLO_READY)){
        bin->yoloReady = 1;
        if(!gpio_get_level(SIM_BTN_COLDCUP)){
            bin->objClass = YOLO_OBJCLASS_COLDCUP;
            ESP_LOGI(TAG, "YOLO Ready!, Class=ColdCup");
        }
        else if(!gpio_get_level(SIM_BTN_HOTCUP)){
            bin->objClass = YOLO_OBJCLASS_HOTCUP;
            ESP_LOGI(TAG, "YOLO Ready!, Class=HotCup");
        }
        return 1;
    }
    return 0;
#endif
}
