#ifndef _LED_H
#define _LED_H

// LED初始化 - 使用GPIO3_B4
int led_init(void);
int led_deinit(void);
void led_set_state(int state);

#endif // _LED_H
