#ifndef SUPERNOVA_CAMERA_H
#define SUPERNOVA_CAMERA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUPERNOVA_CAMERA_FRAME_WIDTH 160
#define SUPERNOVA_CAMERA_FRAME_HEIGHT 120
#define SUPERNOVA_CAMERA_FRAME_PIXELS (SUPERNOVA_CAMERA_FRAME_WIDTH * SUPERNOVA_CAMERA_FRAME_HEIGHT)

void supernova_camera_init(void);
void supernova_camera_poll(void);
void supernova_camera_set_streaming(bool enabled);
uint16_t * supernova_camera_preview_buffer(uint16_t width, uint16_t height);
bool supernova_camera_preview_changed(uint32_t * frame_counter);
bool supernova_camera_copy_frame(uint16_t * target, size_t target_pixels, uint32_t * frame_counter);
bool supernova_camera_copy_scaled_frame(uint16_t * target,
                                        uint16_t target_width,
                                        uint16_t target_height,
                                        uint32_t * frame_counter);
const char * supernova_camera_ip(void);
const char * supernova_camera_status_text(void);
bool supernova_camera_live(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERNOVA_CAMERA_H */
