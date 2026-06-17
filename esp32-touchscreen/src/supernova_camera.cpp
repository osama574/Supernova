#include "supernova_camera.h"

#include "supernova_state.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(ARDUINO)
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_heap_caps.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <img_converters.h>
#endif

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t channel;
    uint8_t reserved;
    char ssid[33];
    char password[65];
    char ip[16];
    uint16_t port;
} supernova_camera_packet_t;

static const uint32_t camera_packet_magic = 0x434D5653UL; /* SVMC */
static const uint8_t camera_packet_version = 1;
static const uint8_t camera_packet_credentials = 1;
static const uint8_t camera_packet_announce = 2;

#if defined(ARDUINO)
static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static bool espnow_ready = false;
static uint32_t last_credentials_send_ms = 0;
static char camera_ip_value[16] = "";
static uint32_t camera_seen_ms = 0;

static volatile bool stream_requested = false;
static volatile bool stream_task_running = false;
static TaskHandle_t stream_task_handle = NULL;
static portMUX_TYPE frame_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t * camera_frame_buffer = NULL;
static bool camera_frame_busy = false;
static bool camera_frame_reading = false;
static uint32_t camera_frame_counter = 0;
static uint32_t camera_last_frame_ms = 0;
static uint32_t last_buffer_log_ms = 0;
static uint16_t * camera_preview_buffer = NULL;
static uint16_t camera_preview_width = 0;
static uint16_t camera_preview_height = 0;
static volatile bool camera_preview_busy = false;
static uint32_t camera_preview_frame_counter = 0;

static const size_t jpeg_buffer_size = 8000;
static uint8_t * jpeg_buffer = NULL;

typedef struct {
    WiFiClient * client;
    bool chunked;
    bool need_chunk_crlf;
    bool end_reached;
    size_t chunk_remaining;
} http_body_reader_t;

static void espnow_receive_cb(const uint8_t * mac, const uint8_t * data, int len)
{
    (void)mac;
    if(data == NULL || len < (int)sizeof(supernova_camera_packet_t)) {
        return;
    }

    supernova_camera_packet_t packet = {};
    memcpy(&packet, data, sizeof(packet));
    if(packet.magic != camera_packet_magic || packet.version != camera_packet_version ||
       packet.type != camera_packet_announce) {
        return;
    }

    packet.ip[sizeof(packet.ip) - 1] = '\0';
    if(packet.ip[0] == '\0') {
        return;
    }

    strncpy(camera_ip_value, packet.ip, sizeof(camera_ip_value) - 1);
    camera_ip_value[sizeof(camera_ip_value) - 1] = '\0';
    camera_seen_ms = millis();
    Serial.printf("ESP32-CAM announced IP: %s\n", camera_ip_value);
}

static void ensure_espnow(void)
{
    if(espnow_ready || WiFi.status() != WL_CONNECTED) {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if(esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed on touchscreen");
        return;
    }

    esp_now_register_recv_cb(espnow_receive_cb);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcast_mac, sizeof(broadcast_mac));
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t add_result = esp_now_add_peer(&peer);
    if(add_result != ESP_OK && add_result != ESP_ERR_ESPNOW_EXIST) {
        Serial.printf("ESP-NOW broadcast peer failed: %d\n", add_result);
    }

    espnow_ready = true;
    Serial.println("ESP-NOW ready on touchscreen");
}

static void send_wifi_credentials(void)
{
    if(!espnow_ready || WiFi.status() != WL_CONNECTED) {
        return;
    }

    char ssid[33];
    char password[65];
    if(!supernova_wifi_get_saved_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        return;
    }

    uint8_t primary_channel = 0;
    wifi_second_chan_t second_channel = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary_channel, &second_channel);

    supernova_camera_packet_t packet = {};
    packet.magic = camera_packet_magic;
    packet.version = camera_packet_version;
    packet.type = camera_packet_credentials;
    packet.channel = primary_channel;
    packet.port = 80;
    strncpy(packet.ssid, ssid, sizeof(packet.ssid) - 1);
    strncpy(packet.password, password, sizeof(packet.password) - 1);

    esp_now_send(broadcast_mac, (const uint8_t *)&packet, sizeof(packet));
}

static bool ensure_camera_buffers(void)
{
    if(camera_frame_buffer == NULL) {
        camera_frame_buffer = (uint16_t *)malloc(SUPERNOVA_CAMERA_FRAME_PIXELS * sizeof(uint16_t));
    }
    if(jpeg_buffer == NULL) {
        jpeg_buffer = (uint8_t *)malloc(jpeg_buffer_size);
    }

    if(camera_frame_buffer == NULL || jpeg_buffer == NULL) {
        if(millis() - last_buffer_log_ms > 1000UL) {
            last_buffer_log_ms = millis();
            Serial.printf("Camera stream buffers could not be allocated free=%u largest=%u frame=%d jpeg=%d\n",
                          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                          camera_frame_buffer != NULL ? 1 : 0,
                          jpeg_buffer != NULL ? 1 : 0);
        }
        return false;
    }
    return true;
}

static bool text_starts_with(const char * line, const char * prefix)
{
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

static bool raw_read_byte(WiFiClient & client, int * value, uint32_t timeout_ms)
{
    const uint32_t start_ms = millis();
    while(stream_requested && millis() - start_ms < timeout_ms) {
        if(client.available() > 0) {
            *value = client.read();
            return *value >= 0;
        }
        if(!client.connected()) {
            return false;
        }
        delay(1);
    }
    return false;
}

static bool raw_read_line(WiFiClient & client, char * line, size_t line_size, uint32_t timeout_ms)
{
    if(line == NULL || line_size == 0) {
        return false;
    }

    size_t index = 0;
    const uint32_t start_ms = millis();
    while(stream_requested && millis() - start_ms < timeout_ms) {
        int value = 0;
        if(!raw_read_byte(client, &value, timeout_ms)) {
            break;
        }

        if(value == '\r') {
            continue;
        }
        if(value == '\n') {
            line[index] = '\0';
            return true;
        }
        if(index + 1 < line_size) {
            line[index++] = (char)value;
        }
    }

    line[index] = '\0';
    return index > 0;
}

static bool consume_chunk_crlf(http_body_reader_t * reader)
{
    int value = 0;
    if(!raw_read_byte(*reader->client, &value, 1200)) {
        return false;
    }
    if(value == '\r') {
        return raw_read_byte(*reader->client, &value, 1200) && value == '\n';
    }
    return value == '\n';
}

static bool prepare_next_chunk(http_body_reader_t * reader)
{
    if(reader->end_reached) {
        return false;
    }

    if(reader->need_chunk_crlf) {
        if(!consume_chunk_crlf(reader)) {
            return false;
        }
        reader->need_chunk_crlf = false;
    }

    char line[32];
    if(!raw_read_line(*reader->client, line, sizeof(line), 1500)) {
        return false;
    }

    reader->chunk_remaining = strtoul(line, NULL, 16);
    if(reader->chunk_remaining == 0) {
        reader->end_reached = true;
        return false;
    }

    return true;
}

static bool body_read_byte(http_body_reader_t * reader, int * value, uint32_t timeout_ms)
{
    if(!reader->chunked) {
        return raw_read_byte(*reader->client, value, timeout_ms);
    }

    if(reader->chunk_remaining == 0 && !prepare_next_chunk(reader)) {
        return false;
    }

    if(!raw_read_byte(*reader->client, value, timeout_ms)) {
        return false;
    }

    reader->chunk_remaining--;
    if(reader->chunk_remaining == 0) {
        reader->need_chunk_crlf = true;
    }
    return true;
}

static bool body_read_line(http_body_reader_t * reader, char * line, size_t line_size, uint32_t timeout_ms)
{
    if(line == NULL || line_size == 0) {
        return false;
    }

    size_t index = 0;
    const uint32_t start_ms = millis();
    while(stream_requested && millis() - start_ms < timeout_ms) {
        int value = 0;
        if(!body_read_byte(reader, &value, timeout_ms)) {
            break;
        }

        if(value == '\r') {
            continue;
        }
        if(value == '\n') {
            line[index] = '\0';
            return true;
        }
        if(index + 1 < line_size) {
            line[index++] = (char)value;
        }
    }

    line[index] = '\0';
    return index > 0;
}

static bool body_read_bytes(http_body_reader_t * reader, uint8_t * target, size_t target_size, uint32_t timeout_ms)
{
    size_t total = 0;
    while(stream_requested && total < target_size) {
        int value = 0;
        if(!body_read_byte(reader, &value, timeout_ms)) {
            return false;
        }
        target[total++] = (uint8_t)value;
    }
    return total == target_size;
}

static bool decode_camera_jpeg(size_t total)
{
    if(total == 0 || total > jpeg_buffer_size) {
        return false;
    }
    if(!ensure_camera_buffers()) {
        return false;
    }

    portENTER_CRITICAL(&frame_mux);
    if(camera_frame_reading) {
        portEXIT_CRITICAL(&frame_mux);
        return false;
    }
    camera_frame_busy = true;
    portEXIT_CRITICAL(&frame_mux);

    const bool decoded = jpg2rgb565(jpeg_buffer, total, (uint8_t *)camera_frame_buffer, JPG_SCALE_NONE);
    if(!decoded) {
        portENTER_CRITICAL(&frame_mux);
        camera_frame_busy = false;
        portEXIT_CRITICAL(&frame_mux);
        return false;
    }

    portENTER_CRITICAL(&frame_mux);
    camera_frame_busy = false;
    camera_frame_counter++;
    camera_last_frame_ms = millis();
    portEXIT_CRITICAL(&frame_mux);
    return true;
}

static bool fetch_camera_frame(void)
{
    if(camera_ip_value[0] == '\0' || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if(!ensure_camera_buffers()) {
        return false;
    }

    char url[64];
    snprintf(url, sizeof(url), "http://%s/capture", camera_ip_value);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(1200);
    if(!http.begin(client, url)) {
        return false;
    }

    const int code = http.GET();
    if(code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    int len = http.getSize();
    if(len <= 0 || len > (int)jpeg_buffer_size) {
        http.end();
        return false;
    }

    WiFiClient * stream = http.getStreamPtr();
    size_t total = 0;
    uint32_t last_read_ms = millis();
    while(total < (size_t)len && millis() - last_read_ms < 1200UL) {
        const int available = stream->available();
        if(available <= 0) {
            delay(2);
            continue;
        }

        const size_t space = jpeg_buffer_size - total;
        const size_t wanted = (size_t)available < space ? (size_t)available : space;
        const int read_now = stream->readBytes(jpeg_buffer + total, wanted);
        if(read_now > 0) {
            total += (size_t)read_now;
            last_read_ms = millis();
        }
    }
    http.end();

    return decode_camera_jpeg(total);
}

static bool fetch_camera_preview_frame(void)
{
    if(camera_ip_value[0] == '\0' || WiFi.status() != WL_CONNECTED ||
       camera_preview_buffer == NULL || camera_preview_width == 0 || camera_preview_height == 0) {
        return false;
    }

    char url[64];
    snprintf(url, sizeof(url), "http://%s/preview565", camera_ip_value);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(2500);
    if(!http.begin(client, url)) {
        return false;
    }

    const int code = http.GET();
    if(code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    const size_t expected = (size_t)camera_preview_width *
                            camera_preview_height *
                            sizeof(uint16_t);
    const int len = http.getSize();
    if(len > 0 && (size_t)len != expected) {
        Serial.printf("Camera preview size mismatch: got %d expected %u\n",
                      len,
                      (unsigned)expected);
        http.end();
        return false;
    }

    WiFiClient * stream = http.getStreamPtr();
    uint8_t * target = (uint8_t *)camera_preview_buffer;
    size_t total = 0;
    uint32_t last_read_ms = millis();
    camera_preview_busy = true;
    while(stream_requested && total < expected && millis() - last_read_ms < 2500UL) {
        const int available = stream->available();
        if(available <= 0) {
            delay(2);
            continue;
        }

        const size_t remaining = expected - total;
        const size_t wanted = (size_t)available < remaining ? (size_t)available : remaining;
        const int read_now = stream->readBytes(target + total, wanted);
        if(read_now > 0) {
            total += (size_t)read_now;
            last_read_ms = millis();
        }
    }
    camera_preview_busy = false;
    http.end();

    if(total != expected) {
        Serial.printf("Camera preview read failed: got %u expected %u\n",
                      (unsigned)total,
                      (unsigned)expected);
        return false;
    }

    camera_preview_frame_counter++;
    camera_last_frame_ms = millis();
    return true;
}

static bool stream_camera_frames(void)
{
    if(camera_ip_value[0] == '\0' || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    if(!ensure_camera_buffers()) {
        return false;
    }

    WiFiClient client;
    client.setTimeout(1200);
    if(!client.connect(camera_ip_value, 80)) {
        return false;
    }

    client.printf("GET /stream HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", camera_ip_value);

    char line[128];
    if(!raw_read_line(client, line, sizeof(line), 1800) || strstr(line, "200") == NULL) {
        client.stop();
        return false;
    }

    bool chunked = false;
    while(raw_read_line(client, line, sizeof(line), 1200)) {
        if(line[0] == '\0') {
            break;
        }
        if(strstr(line, "Transfer-Encoding: chunked") != NULL ||
           strstr(line, "transfer-encoding: chunked") != NULL) {
            chunked = true;
        }
    }

    http_body_reader_t reader = {};
    reader.client = &client;
    reader.chunked = chunked;

    bool decoded_any = false;
    while(stream_requested && WiFi.status() == WL_CONNECTED && client.connected()) {
        bool found_boundary = false;
        const uint32_t boundary_start_ms = millis();
        while(stream_requested && millis() - boundary_start_ms < 2500UL) {
            if(!body_read_line(&reader, line, sizeof(line), 1500)) {
                client.stop();
                return decoded_any;
            }
            if(text_starts_with(line, "--")) {
                found_boundary = true;
                break;
            }
        }
        if(!found_boundary) {
            client.stop();
            return decoded_any;
        }

        size_t content_length = 0;
        while(stream_requested) {
            if(!body_read_line(&reader, line, sizeof(line), 1500)) {
                client.stop();
                return decoded_any;
            }
            if(line[0] == '\0') {
                break;
            }
            if(text_starts_with(line, "Content-Length:")) {
                content_length = strtoul(line + strlen("Content-Length:"), NULL, 10);
            }
        }

        if(content_length == 0 || content_length > jpeg_buffer_size) {
            client.stop();
            return decoded_any;
        }

        if(!body_read_bytes(&reader, jpeg_buffer, content_length, 1800)) {
            client.stop();
            return decoded_any;
        }

        if(decode_camera_jpeg(content_length)) {
            decoded_any = true;
        }
        delay(1);
    }

    client.stop();
    return decoded_any;
}

static void camera_stream_task(void * parameter)
{
    (void)parameter;
    stream_task_running = true;
    while(stream_requested) {
        fetch_camera_preview_frame();

        for(uint8_t i = 0; stream_requested && i < 7; ++i) {
            delay(100);
        }
    }

    stream_task_running = false;
    stream_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif

void supernova_camera_init(void)
{
#if defined(ARDUINO)
    Serial.println("Supernova camera bridge ready");
#endif
}

void supernova_camera_poll(void)
{
#if defined(ARDUINO)
    ensure_espnow();
    const uint32_t now = millis();
    if(WiFi.status() == WL_CONNECTED && now - last_credentials_send_ms > 3000UL) {
        last_credentials_send_ms = now;
        send_wifi_credentials();
    }
#endif
}

void supernova_camera_set_streaming(bool enabled)
{
#if defined(ARDUINO)
    stream_requested = enabled;
    if(enabled && !stream_task_running && stream_task_handle == NULL) {
        xTaskCreatePinnedToCore(camera_stream_task, "camera_snap", 6144, NULL, 1, &stream_task_handle, 0);
    }
#else
    (void)enabled;
#endif
}

uint16_t * supernova_camera_preview_buffer(uint16_t width, uint16_t height)
{
#if defined(ARDUINO)
    const size_t bytes = (size_t)width * height * sizeof(uint16_t);
    if(camera_preview_buffer == NULL ||
       camera_preview_width != width ||
       camera_preview_height != height) {
        if(camera_preview_buffer != NULL) {
            free(camera_preview_buffer);
            camera_preview_buffer = NULL;
        }
        camera_preview_buffer = (uint16_t *)malloc(bytes);
        camera_preview_width = camera_preview_buffer != NULL ? width : 0;
        camera_preview_height = camera_preview_buffer != NULL ? height : 0;
        camera_preview_frame_counter = 0;
        if(camera_preview_buffer != NULL) {
            memset(camera_preview_buffer, 0, bytes);
        }
        else {
            Serial.printf("Camera preview canvas buffer failed free=%u largest=%u bytes=%u\n",
                          (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                          (unsigned)bytes);
        }
    }
    return camera_preview_buffer;
#else
    (void)width;
    (void)height;
    return NULL;
#endif
}

bool supernova_camera_preview_changed(uint32_t * frame_counter)
{
#if defined(ARDUINO)
    if(camera_preview_busy || camera_preview_frame_counter == 0) {
        return false;
    }
    if(frame_counter != NULL) {
        if(*frame_counter == camera_preview_frame_counter) {
            return false;
        }
        *frame_counter = camera_preview_frame_counter;
    }
    return true;
#else
    if(frame_counter != NULL) {
        *frame_counter = 0;
    }
    return false;
#endif
}

bool supernova_camera_copy_frame(uint16_t * target, size_t target_pixels, uint32_t * frame_counter)
{
#if defined(ARDUINO)
    if(target == NULL || target_pixels < SUPERNOVA_CAMERA_FRAME_PIXELS || camera_frame_counter == 0) {
        return false;
    }

    portENTER_CRITICAL(&frame_mux);
    if(camera_frame_buffer == NULL || camera_frame_busy || camera_frame_reading) {
        portEXIT_CRITICAL(&frame_mux);
        return false;
    }
    camera_frame_reading = true;
    const uint32_t copied_counter = camera_frame_counter;
    portEXIT_CRITICAL(&frame_mux);

    memcpy(target, camera_frame_buffer, SUPERNOVA_CAMERA_FRAME_PIXELS * sizeof(uint16_t));

    portENTER_CRITICAL(&frame_mux);
    camera_frame_reading = false;
    portEXIT_CRITICAL(&frame_mux);
    if(frame_counter != NULL) {
        *frame_counter = copied_counter;
    }
    return true;
#else
    (void)target;
    (void)target_pixels;
    if(frame_counter != NULL) {
        *frame_counter = 0;
    }
    return false;
#endif
}

bool supernova_camera_copy_scaled_frame(uint16_t * target,
                                        uint16_t target_width,
                                        uint16_t target_height,
                                        uint32_t * frame_counter)
{
#if defined(ARDUINO)
    if(target == NULL || target_width == 0 || target_height == 0 || camera_frame_counter == 0) {
        return false;
    }

    portENTER_CRITICAL(&frame_mux);
    if(camera_frame_buffer == NULL || camera_frame_busy || camera_frame_reading) {
        portEXIT_CRITICAL(&frame_mux);
        return false;
    }
    camera_frame_reading = true;
    const uint32_t copied_counter = camera_frame_counter;
    portEXIT_CRITICAL(&frame_mux);

    for(uint16_t y = 0; y < target_height; ++y) {
        const uint16_t src_y = ((uint32_t)y * SUPERNOVA_CAMERA_FRAME_HEIGHT) / target_height;
        const uint16_t * src_row = camera_frame_buffer + (size_t)src_y * SUPERNOVA_CAMERA_FRAME_WIDTH;
        uint16_t * dst_row = target + (size_t)y * target_width;
        for(uint16_t x = 0; x < target_width; ++x) {
            const uint16_t src_x = ((uint32_t)x * SUPERNOVA_CAMERA_FRAME_WIDTH) / target_width;
            dst_row[x] = src_row[src_x];
        }
    }

    portENTER_CRITICAL(&frame_mux);
    camera_frame_reading = false;
    portEXIT_CRITICAL(&frame_mux);
    if(frame_counter != NULL) {
        *frame_counter = copied_counter;
    }
    return true;
#else
    (void)target;
    (void)target_width;
    (void)target_height;
    if(frame_counter != NULL) {
        *frame_counter = 0;
    }
    return false;
#endif
}

const char * supernova_camera_ip(void)
{
#if defined(ARDUINO)
    return camera_ip_value[0] == '\0' ? "" : camera_ip_value;
#else
    return "";
#endif
}

bool supernova_camera_live(void)
{
#if defined(ARDUINO)
    return camera_preview_frame_counter > 0 && millis() - camera_last_frame_ms < 3500UL;
#else
    return false;
#endif
}

const char * supernova_camera_status_text(void)
{
#if defined(ARDUINO)
    if(WiFi.status() != WL_CONNECTED) {
        return "Connect touchscreen WiFi first";
    }
    if(camera_ip_value[0] == '\0') {
        return "Waiting for ESP32-CAM";
    }
    if(supernova_camera_live()) {
        return "Live preview";
    }
    if(millis() - camera_seen_ms < 15000UL) {
        return "Camera found, loading video";
    }
    return "Camera not responding";
#else
    return "Camera bridge unavailable";
#endif
}
