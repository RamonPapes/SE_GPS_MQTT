/**
 * @file lib_mqtt.hpp
 * @author Daniel Januario (daniel.rocha@fieb.org.br)
 * @brief Embeddo MQTT layer library
 * @version 0.1
 * @date 2023-11-28
 *
 */

#ifndef LIB_MQTT_HPP
#define LIB_MQTT_HPP

#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_log.h"

#define MQTT_MAX_TOPICS 16

typedef void (*mqtt_callback_t)(const char *topic, const char *data);

class MQTT

{
public: 
    MQTT();
    void init(uint16_t m_port, char * m_host);
    void write(char * topic, char * data);
    void read(const char *topic);
    void onMessage(mqtt_callback_t cb);
    mqtt_callback_t message_callback = nullptr;

private:
    char * m_topic;
    char * m_data;
    // static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
    //                                int32_t event_id, void *event_data);

};

#endif