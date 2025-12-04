#include "mqtt.hpp"

const char *MQTT_TAG = "MQTT_TASK";

esp_mqtt_client_handle_t client;

static MQTT *global_mqtt_instance = nullptr;

static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

MQTT::MQTT()
{
    global_mqtt_instance = this;
}

void MQTT::init(uint16_t m_port, char *m_host)
{
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = m_host;
    mqtt_cfg.broker.address.port = m_port;

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client,
                                   (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler,
                                   NULL);

    esp_mqtt_client_start(client);
}

void MQTT::write(char *topic, char *data)
{
    esp_mqtt_client_publish(client, topic, data, 0, 1, 0);
}

void MQTT::read(const char *topic)
{
    esp_mqtt_client_subscribe(client, topic, 1);
}

void MQTT::onMessage(mqtt_callback_t cb)
{
    this->message_callback = cb;
}

void MQTT::onConnected(mqtt_connected_callback_t cb)
{
    this->connected_callback = cb;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        if (global_mqtt_instance && global_mqtt_instance->connected_callback)
        {
            global_mqtt_instance->connected_callback();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "Mensagem recebida!");
        ESP_LOGI(MQTT_TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(MQTT_TAG, "DATA=%.*s", event->data_len, event->data);

        if (global_mqtt_instance && global_mqtt_instance->message_callback)
        {
            // Convertendo para strings terminadas em '\0'
            char topic[256];
            char data[512];

            memcpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';

            memcpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';

            global_mqtt_instance->message_callback(topic, data);
        }
        break;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        break;
    }
}
