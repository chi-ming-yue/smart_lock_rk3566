#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>   
#include <linux/gpio.h>
#include "led.h"

#define LED_GPIO 108    // GPIO3_B4

int led_init(void)
{
    int ret;
    
    printk("Initializing LED on GPIO %d...\n", LED_GPIO);
    // 申请GPIO
    ret = gpio_request(LED_GPIO, "led_gpio");
    if (ret) {
        printk("Failed to request GPIO %d: %d\n", LED_GPIO, ret);
        return ret;
    }
    // 设置为输出模式，初始低电平（熄灭）
    ret = gpio_direction_output(LED_GPIO, 0);
    if (ret) {
        printk("Failed to set GPIO %d as output: %d\n", LED_GPIO, ret);
        gpio_free(LED_GPIO);
        return ret;
    }
    
    printk("LED initialization complete\n");
    return 0;
}

int led_deinit(void)
{
    // 熄灭LED
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
    printk("LED deinitialized\n");
    return 0;
}

// 添加LED状态控制函数
void led_set_state(int state)
{
    gpio_set_value(LED_GPIO, state);
    printk("LED set to %s\n", state ? "ON" : "OFF");
}