#ifndef _OLED_H
#define _OLED_H

#include <linux/types.h>

// OLED配置
#define OLED_I2C_ADDR    0x3C
#define OLED_WIDTH       128
#define OLED_HEIGHT      64

// OLED函数声明
int oled_init(void);
void oled_deinit(void);
void oled_clear(void);
void oled_display(void);
void oled_draw_string(int x, int y, const char *str);
void oled_update_sensor_status(char sensor_state);
void oled_update_led_status(int led_state);
void oled_update_face_result(const char *name, int score_percent);
void oled_update_password_status(const char *status);
void oled_update_servo_status(const char *status);

#endif
