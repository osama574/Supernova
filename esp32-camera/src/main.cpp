#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <img_converters.h>
#include <Wire.h>

/* ESP32-CAM-MB / AI-Thinker pinout, matching CBAA0046-044_UK manual. */
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_GPIO_NUM 4

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

static const uint32_t camera_packet_magic = 0x434D5653UL;
static const uint8_t camera_packet_version = 1;
static const uint8_t camera_packet_credentials = 1;
static const uint8_t camera_packet_announce = 2;
static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const char * old_default_wifi_ssid = "CHANGE_ME_OLD_SSID";
static const char * default_wifi_ssid = "CHANGE_ME_WIFI_SSID";
static const char * default_wifi_password = "CHANGE_ME_WIFI_PASSWORD";
static const uint8_t current_wifi_revision = 2;
static const uint16_t preview_width = 260;
static const uint16_t preview_height = 150;
static const uint16_t camera_capture_width = 160;
static const uint16_t camera_capture_height = 120;

static Preferences prefs;
static httpd_handle_t camera_httpd = NULL;
static char saved_ssid[33] = "";
static char saved_password[65] = "";
static volatile bool pending_wifi_connect = false;
static volatile bool pending_credentials_save = false;
static bool espnow_ready = false;
static bool camera_initialized = false;
static uint8_t listen_channel = 1;
static uint32_t last_channel_hop_ms = 0;
static uint32_t last_wifi_attempt_ms = 0;
static uint32_t last_camera_init_attempt_ms = 0;
static uint32_t last_announce_ms = 0;
static uint32_t last_loop_log_ms = 0;
static uint32_t last_sccb_probe_ms = 0;
static uint16_t * preview_decode_buffer = NULL;
static uint16_t * preview_output_buffer = NULL;

static void save_credentials(const char * ssid, const char * password)
{
    if(ssid != NULL && strcmp(saved_ssid, ssid) == 0 && WiFi.status() == WL_CONNECTED) {
        return;
    }

    strncpy(saved_ssid, ssid == NULL ? "" : ssid, sizeof(saved_ssid) - 1);
    saved_ssid[sizeof(saved_ssid) - 1] = '\0';
    strncpy(saved_password, password == NULL ? "" : password, sizeof(saved_password) - 1);
    saved_password[sizeof(saved_password) - 1] = '\0';
    pending_credentials_save = true;
    pending_wifi_connect = saved_ssid[0] != '\0';
    Serial.printf("Received WiFi credentials for SSID: %s\n", saved_ssid);
}

static void save_credentials_to_flash(void)
{
    if(!pending_credentials_save) {
        return;
    }
    pending_credentials_save = false;
    if(prefs.begin("supernova", false)) {
        prefs.putString("wifi_ssid", saved_ssid);
        prefs.putString("wifi_pass", saved_password);
        prefs.putUChar("wifi_rev", current_wifi_revision);
        prefs.end();
    }
}

static void load_credentials(void)
{
    saved_ssid[0] = '\0';
    saved_password[0] = '\0';
    if(prefs.begin("supernova", true)) {
        String ssid = prefs.getString("wifi_ssid", "");
        String password = prefs.getString("wifi_pass", "");
        uint8_t revision = prefs.getUChar("wifi_rev", 0);
        prefs.end();
        if(ssid.length() > 0 && ssid != old_default_wifi_ssid && revision == current_wifi_revision) {
            ssid.toCharArray(saved_ssid, sizeof(saved_ssid));
            password.toCharArray(saved_password, sizeof(saved_password));
        }
    }

    if(saved_ssid[0] == '\0') {
        strncpy(saved_ssid, default_wifi_ssid, sizeof(saved_ssid) - 1);
        saved_ssid[sizeof(saved_ssid) - 1] = '\0';
        strncpy(saved_password, default_wifi_password, sizeof(saved_password) - 1);
        saved_password[sizeof(saved_password) - 1] = '\0';
        Serial.printf("Using default WiFi credentials for SSID: %s\n", saved_ssid);
    }
}

static void espnow_receive_cb(const uint8_t * mac, const uint8_t * data, int len)
{
    (void)mac;
    if(data == NULL || len < (int)sizeof(supernova_camera_packet_t)) {
        return;
    }

    supernova_camera_packet_t packet = {};
    memcpy(&packet, data, sizeof(packet));
    if(packet.magic != camera_packet_magic || packet.version != camera_packet_version ||
       packet.type != camera_packet_credentials) {
        return;
    }

    packet.ssid[sizeof(packet.ssid) - 1] = '\0';
    packet.password[sizeof(packet.password) - 1] = '\0';
    if(packet.ssid[0] == '\0') {
        return;
    }

    save_credentials(packet.ssid, packet.password);
}

static void ensure_espnow(void)
{
    if(espnow_ready) {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if(esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed on camera");
        return;
    }

    esp_now_register_recv_cb(espnow_receive_cb);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcast_mac, sizeof(broadcast_mac));
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t add_result = esp_now_add_peer(&peer);
    if(add_result != ESP_OK && add_result != ESP_ERR_ESPNOW_EXIST) {
        Serial.printf("ESP-NOW peer add failed: %d\n", add_result);
    }

    espnow_ready = true;
    Serial.println("ESP-NOW ready on ESP32-CAM");
}

static void hop_listen_channel(void)
{
    if(WiFi.status() == WL_CONNECTED || millis() - last_channel_hop_ms < 450UL) {
        return;
    }

    last_channel_hop_ms = millis();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(listen_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    listen_channel++;
    if(listen_channel > 13) {
        listen_channel = 1;
    }
}

static esp_err_t status_handler(httpd_req_t * req)
{
    char body[192];
    snprintf(body, sizeof(body),
             "{\"name\":\"SupernovaCam\",\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"camera\":%s}",
             WiFi.localIP().toString().c_str(),
             WiFi.RSSI(),
             millis() / 1000UL,
             camera_initialized ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t * req)
{
    if(!camera_initialized) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    camera_fb_t * fb = NULL;
    for(uint8_t attempt = 0; attempt < 3 && fb == NULL; ++attempt) {
        fb = esp_camera_fb_get();
        if(fb == NULL) {
            delay(80);
        }
    }
    if(fb == NULL) {
        Serial.println("Camera capture failed: no frame buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char len_text[16];
    snprintf(len_text, sizeof(len_text), "%u", (unsigned)fb->len);
    httpd_resp_set_hdr(req, "Content-Length", len_text);
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

static void * preview_alloc(size_t bytes)
{
    void * ptr = ps_malloc(bytes);
    if(ptr == NULL) {
        ptr = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
    return ptr;
}

static bool ensure_preview_buffers(void)
{
    if(preview_decode_buffer == NULL) {
        preview_decode_buffer = (uint16_t *)preview_alloc((size_t)camera_capture_width *
                                                          camera_capture_height *
                                                          sizeof(uint16_t));
    }
    if(preview_output_buffer == NULL) {
        preview_output_buffer = (uint16_t *)preview_alloc((size_t)preview_width *
                                                          preview_height *
                                                          sizeof(uint16_t));
    }

    if(preview_decode_buffer == NULL || preview_output_buffer == NULL) {
        Serial.println("Camera preview buffers could not be allocated");
        return false;
    }
    return true;
}

static esp_err_t preview565_handler(httpd_req_t * req)
{
    if(!camera_initialized || !ensure_preview_buffers()) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    camera_fb_t * fb = NULL;
    for(uint8_t attempt = 0; attempt < 3 && fb == NULL; ++attempt) {
        fb = esp_camera_fb_get();
        if(fb == NULL) {
            delay(80);
        }
    }
    if(fb == NULL) {
        Serial.println("Camera preview failed: no frame buffer");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const bool decoded = jpg2rgb565(fb->buf,
                                    fb->len,
                                    (uint8_t *)preview_decode_buffer,
                                    JPG_SCALE_NONE);
    esp_camera_fb_return(fb);
    if(!decoded) {
        Serial.println("Camera preview JPEG decode failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const uint16_t crop_height = 92;
    const uint16_t crop_y = (camera_capture_height - crop_height) / 2;
    for(uint16_t y = 0; y < preview_height; ++y) {
        const uint16_t src_y = crop_y + ((uint32_t)y * crop_height) / preview_height;
        const uint16_t * src_row = preview_decode_buffer + (size_t)src_y * camera_capture_width;
        uint16_t * dst_row = preview_output_buffer + (size_t)y * preview_width;
        for(uint16_t x = 0; x < preview_width; ++x) {
            const uint16_t src_x = ((uint32_t)x * camera_capture_width) / preview_width;
            dst_row[x] = src_row[src_x];
        }
    }

    char len_text[16];
    snprintf(len_text, sizeof(len_text), "%u", (unsigned)((size_t)preview_width *
                                                          preview_height *
                                                          sizeof(uint16_t)));
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Preview-Width", "260");
    httpd_resp_set_hdr(req, "X-Preview-Height", "150");
    httpd_resp_set_hdr(req, "Content-Length", len_text);
    return httpd_resp_send(req,
                           (const char *)preview_output_buffer,
                           (size_t)preview_width * preview_height * sizeof(uint16_t));
}

static esp_err_t stream_handler(httpd_req_t * req)
{
    if(!camera_initialized) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    static const char * boundary = "\r\n--supernova\r\n";
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=supernova");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    while(true) {
        camera_fb_t * fb = esp_camera_fb_get();
        if(fb == NULL) {
            return ESP_FAIL;
        }

        char header[96];
        const int header_len = snprintf(header, sizeof(header),
                                        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                                        (unsigned)fb->len);
        esp_err_t res = httpd_resp_send_chunk(req, boundary, strlen(boundary));
        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, header, header_len);
        }
        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK) {
            break;
        }
        delay(60);
    }

    return ESP_OK;
}

static void start_server(void)
{
    if(camera_httpd != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.stack_size = 6144;
    config.max_uri_handlers = 6;

    if(httpd_start(&camera_httpd, &config) != ESP_OK) {
        Serial.println("Camera HTTP server failed");
        camera_httpd = NULL;
        return;
    }

    httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };
    httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
    httpd_uri_t jpg_uri = { .uri = "/jpg", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL };
    httpd_uri_t preview_uri = { .uri = "/preview565", .method = HTTP_GET, .handler = preview565_handler, .user_ctx = NULL };
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
    httpd_register_uri_handler(camera_httpd, &status_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &jpg_uri);
    httpd_register_uri_handler(camera_httpd, &preview_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    Serial.printf("Camera HTTP server ready: http://%s/status\n", WiFi.localIP().toString().c_str());
}

static bool probe_camera_sccb(void)
{
    if(last_sccb_probe_ms != 0 && millis() - last_sccb_probe_ms < 15000UL) {
        return true;
    }
    last_sccb_probe_ms = millis();

    bool found = false;
    ledcSetup(LEDC_CHANNEL_0, 10000000, 1);
    ledcAttachPin(XCLK_GPIO_NUM, LEDC_CHANNEL_0);
    ledcWrite(LEDC_CHANNEL_0, 1);
    delay(80);

    Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM);
    Wire.setClock(100000);

    for(uint8_t address = 1; address < 0x78; ++address) {
        Wire.beginTransmission(address);
        if(Wire.endTransmission() == 0) {
            Serial.printf("Camera SCCB/I2C device found at 0x%02x\n", address);
            found = true;
        }
        delay(2);
    }

    Wire.end();
    ledcDetachPin(XCLK_GPIO_NUM);
    if(!found) {
        Serial.println("No camera SCCB/I2C device found on SIOD=26 SIOC=27");
    }
    return found;
}

static bool init_camera(void)
{
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, HIGH);
    delay(60);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(180);
    probe_camera_sccb();

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    Serial.printf("PSRAM: %s\n", psramFound() ? "found" : "not found");
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 18;
    config.fb_count = 1;
    config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if(err != ESP_OK) {
        Serial.printf("Camera init failed at 20MHz: 0x%x, retrying 10MHz\n", err);
        esp_camera_deinit();
        digitalWrite(PWDN_GPIO_NUM, HIGH);
        delay(80);
        digitalWrite(PWDN_GPIO_NUM, LOW);
        delay(180);
        config.xclk_freq_hz = 10000000;
        err = esp_camera_init(&config);
    }
    if(err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }

    sensor_t * sensor = esp_camera_sensor_get();
    if(sensor != NULL) {
        sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
        sensor->set_quality(sensor, 18);
        sensor->set_vflip(sensor, 0);
        sensor->set_hmirror(sensor, 0);
    }

    delay(900);
    camera_fb_t * test_frame = NULL;
    for(uint8_t attempt = 0; attempt < 6 && test_frame == NULL; ++attempt) {
        test_frame = esp_camera_fb_get();
        if(test_frame == NULL) {
            delay(180);
        }
    }
    if(test_frame == NULL) {
        Serial.println("Camera init test failed: no frame");
        esp_camera_deinit();
        return false;
    }
    Serial.printf("Camera init test frame: %ux%u %u bytes\n",
                  test_frame->width,
                  test_frame->height,
                  (unsigned)test_frame->len);
    esp_camera_fb_return(test_frame);

    Serial.println("Camera sensor ready");
    return true;
}

static void connect_wifi_if_needed(void)
{
    if(saved_ssid[0] == '\0' || WiFi.status() == WL_CONNECTED) {
        return;
    }
    if(!pending_wifi_connect && millis() - last_wifi_attempt_ms < 12000UL) {
        return;
    }

    pending_wifi_connect = false;
    last_wifi_attempt_ms = millis();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(saved_ssid, saved_password);
    Serial.printf("Connecting ESP32-CAM to WiFi: %s\n", saved_ssid);
}

static void announce_camera(void)
{
    if(!espnow_ready || WiFi.status() != WL_CONNECTED || millis() - last_announce_ms < 2000UL) {
        return;
    }

    last_announce_ms = millis();
    supernova_camera_packet_t packet = {};
    packet.magic = camera_packet_magic;
    packet.version = camera_packet_version;
    packet.type = camera_packet_announce;
    packet.port = 80;
    WiFi.localIP().toString().toCharArray(packet.ip, sizeof(packet.ip));
    esp_now_send(broadcast_mac, (const uint8_t *)&packet, sizeof(packet));
}

void setup(void)
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(500);
    Serial.println();
    Serial.println("Starting Supernova ESP32-CAM");
    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    ensure_espnow();
    load_credentials();
    if(saved_ssid[0] != '\0') {
        pending_wifi_connect = true;
    }
}

void loop(void)
{
    if(millis() - last_loop_log_ms > 5000UL) {
        last_loop_log_ms = millis();
        Serial.printf("Loop wifi=%d ssid=%s pending=%d camera=%d channel=%u\n",
                      WiFi.status(),
                      saved_ssid,
                      pending_wifi_connect ? 1 : 0,
                      camera_initialized ? 1 : 0,
                      listen_channel);
    }
    ensure_espnow();
    save_credentials_to_flash();
    connect_wifi_if_needed();
    hop_listen_channel();
    if(WiFi.status() == WL_CONNECTED) {
        start_server();
        if(!camera_initialized && millis() - last_camera_init_attempt_ms > 3000UL) {
            last_camera_init_attempt_ms = millis();
            camera_initialized = init_camera();
        }
        if(camera_initialized) {
            announce_camera();
        }
    }
    delay(20);
}
