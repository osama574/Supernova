#include "supernova_mqtt.h"

#include "supernova_devices.h"

#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#ifndef SUPERNOVA_MQTT_HOST
#define SUPERNOVA_MQTT_HOST "CHANGE_ME_BROKER_IP"
#endif

#ifndef SUPERNOVA_MQTT_PORT
#define SUPERNOVA_MQTT_PORT 1883
#endif

static WiFiClient mqtt_network_client;
static PubSubClient mqtt_client(mqtt_network_client);
static uint32_t next_mqtt_connect_ms = 0;
static uint32_t next_mqtt_state_ms = 0;
#endif

static const char * mqtt_command_topic = "supernova/motors/cmd";
static const char * mqtt_state_topic = "supernova/motors/state";
static const int8_t mqtt_servo_step = 2;
static char mqtt_status_text[64] = "MQTT waiting";

#if defined(ARDUINO)
static void set_status(const char * text)
{
    snprintf(mqtt_status_text, sizeof(mqtt_status_text), "%s", text);
}

static bool payload_matches(const char * payload, const char * command)
{
    char quoted[24];
    snprintf(quoted, sizeof(quoted), "\"%s\"", command);
    return strcmp(payload, command) == 0 || strstr(payload, quoted) != NULL;
}

static void publish_motor_state(const char * source)
{
    if(!mqtt_client.connected()) {
        return;
    }

    char payload[128];
    snprintf(payload,
             sizeof(payload),
             "{\"pan\":%d,\"tilt\":%d,\"source\":\"%s\"}",
             supernova_devices_servo_pan_angle(),
             supernova_devices_servo_tilt_angle(),
             source == NULL ? "display" : source);
    mqtt_client.publish(mqtt_state_topic, payload, true);
}

static void mqtt_message_cb(char * topic, uint8_t * payload, unsigned int length)
{
    if(topic == NULL || strcmp(topic, mqtt_command_topic) != 0) {
        return;
    }

    char command[64];
    const unsigned int copy_length = length < sizeof(command) - 1 ? length : sizeof(command) - 1;
    for(unsigned int i = 0; i < copy_length; ++i) {
        char c = (char)payload[i];
        if(c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        command[i] = c;
    }
    command[copy_length] = '\0';

    int8_t pan_delta = 0;
    int8_t tilt_delta = 0;

    if(payload_matches(command, "up")) {
        tilt_delta = -mqtt_servo_step;
    }
    else if(payload_matches(command, "down")) {
        tilt_delta = mqtt_servo_step;
    }
    else if(payload_matches(command, "left")) {
        pan_delta = -mqtt_servo_step;
    }
    else if(payload_matches(command, "right")) {
        pan_delta = mqtt_servo_step;
    }
    else {
        Serial.printf("MQTT ignored motor command: %s\n", command);
        return;
    }

    supernova_devices_servo_nudge(pan_delta, tilt_delta);
    publish_motor_state("mqtt");
}

static void mqtt_connect_if_needed(uint32_t now_ms)
{
    if(WiFi.status() != WL_CONNECTED) {
        set_status("MQTT: WiFi needed");
        return;
    }

    if(mqtt_client.connected()) {
        set_status("MQTT: online");
        return;
    }

    if((int32_t)(now_ms - next_mqtt_connect_ms) < 0) {
        return;
    }
    next_mqtt_connect_ms = now_ms + 5000UL;

    char client_id[40];
    snprintf(client_id,
             sizeof(client_id),
             "supernova-display-%06llX",
             (unsigned long long)(ESP.getEfuseMac() & 0xFFFFFFULL));

    Serial.printf("MQTT connecting to %s:%d as %s\n",
                  SUPERNOVA_MQTT_HOST,
                  SUPERNOVA_MQTT_PORT,
                  client_id);

    if(mqtt_client.connect(client_id)) {
        mqtt_client.subscribe(mqtt_command_topic);
        set_status("MQTT: online");
        publish_motor_state("online");
        Serial.printf("MQTT subscribed: %s\n", mqtt_command_topic);
        return;
    }

    snprintf(mqtt_status_text,
             sizeof(mqtt_status_text),
             "MQTT: retry %d",
             mqtt_client.state());
    Serial.printf("MQTT connect failed, state=%d\n", mqtt_client.state());
}
#endif

void supernova_mqtt_init(void)
{
#if defined(ARDUINO)
    mqtt_client.setServer(SUPERNOVA_MQTT_HOST, SUPERNOVA_MQTT_PORT);
    mqtt_client.setCallback(mqtt_message_cb);
    mqtt_client.setKeepAlive(15);
    mqtt_client.setSocketTimeout(1);
    snprintf(mqtt_status_text, sizeof(mqtt_status_text), "MQTT: %s", SUPERNOVA_MQTT_HOST);
#endif
}

void supernova_mqtt_poll(void)
{
#if defined(ARDUINO)
    const uint32_t now_ms = millis();
    mqtt_connect_if_needed(now_ms);
    if(mqtt_client.connected()) {
        mqtt_client.loop();
        if((int32_t)(now_ms - next_mqtt_state_ms) >= 0) {
            next_mqtt_state_ms = now_ms + 3000UL;
            publish_motor_state("status");
        }
    }
#endif
}

bool supernova_mqtt_connected(void)
{
#if defined(ARDUINO)
    return mqtt_client.connected();
#else
    return false;
#endif
}

const char * supernova_mqtt_broker(void)
{
#if defined(ARDUINO)
    return SUPERNOVA_MQTT_HOST;
#else
    return "";
#endif
}

const char * supernova_mqtt_status_text(void)
{
    return mqtt_status_text;
}
