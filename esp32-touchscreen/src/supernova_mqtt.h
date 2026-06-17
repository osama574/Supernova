#ifndef SUPERNOVA_MQTT_H
#define SUPERNOVA_MQTT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void supernova_mqtt_init(void);
void supernova_mqtt_poll(void);
bool supernova_mqtt_connected(void);
const char * supernova_mqtt_broker(void);
const char * supernova_mqtt_status_text(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERNOVA_MQTT_H */
