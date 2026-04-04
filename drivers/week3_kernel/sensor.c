#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>   
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#include "sensor.h"

#define SENSOR_GPIO 107     // GPIO3_B3
#define SENSOR_HOLD_MS 2000

int irq_number = 0;         // 中断号
char sensor_state = '0';    // 传感器状态：'0'表示无人，'1'表示有人
static struct timer_list sensor_hold_timer;

static void hc_sr501_hold_timeout(struct timer_list *timer)
{
    char current_state = hc_sr501_refresh_state();

    printk("HC-SR501 hold timeout, state: %c\n", current_state);
}

char hc_sr501_refresh_state(void)
{
    int gpio_val = gpio_get_value(SENSOR_GPIO);

    sensor_state = gpio_val ? '1' : '0';
    return sensor_state;
}

// 中断处理函数
static irqreturn_t hc_sr501_irq_handler(int irq, void *dev_id)
{
    int sensor_val = gpio_get_value(SENSOR_GPIO);
    
    if (sensor_val) {
        printk(KERN_ALERT "HC-SR501: Motion detected!\n");
        sensor_state = '1';
        mod_timer(&sensor_hold_timer, jiffies + msecs_to_jiffies(SENSOR_HOLD_MS));
    }

    return IRQ_HANDLED;
}

int hc_sr501_init(void)
{
    int ret;
    int gpio_val;
    
    printk("Initializing HC-SR501 sensor on GPIO %d...\n", SENSOR_GPIO);
    // 申请GPIO
    ret = gpio_request(SENSOR_GPIO, "hc_sr501");
    if (ret) {
        printk("Failed to request GPIO %d\n", SENSOR_GPIO);
        return ret;
    }
    // 设置为输入模式
    ret = gpio_direction_input(SENSOR_GPIO);
    if (ret) {
        printk("Failed to set GPIO %d as input\n", SENSOR_GPIO);
        gpio_free(SENSOR_GPIO);
        return ret;
    }
    // 获取中断号
    irq_number = gpio_to_irq(SENSOR_GPIO);
    if (irq_number < 0) {
        printk("Failed to get IRQ number for GPIO %d\n", SENSOR_GPIO);
        gpio_free(SENSOR_GPIO);
        return irq_number;
    }
    
    printk("HC-SR501 IRQ number: %d\n", irq_number);
    timer_setup(&sensor_hold_timer, hc_sr501_hold_timeout, 0);
    // 注册中断处理函数
    ret = request_irq(irq_number, hc_sr501_irq_handler, 
                     IRQF_TRIGGER_RISING, 
                     "hc_sr501", NULL);
    if (ret) {
        printk("Failed to request IRQ %d\n", irq_number);
        del_timer_sync(&sensor_hold_timer);
        gpio_free(SENSOR_GPIO);
        return ret;
    }
    // 读取初始状态
    sensor_state = hc_sr501_refresh_state();
    gpio_val = (sensor_state == '1');
    printk("HC-SR501 initial state: %c (GPIO value: %d)\n", sensor_state, gpio_val);
    
    printk("HC-SR501 initialization complete\n");
    return 0;
}

void hc_sr501_deinit(void)
{
    del_timer_sync(&sensor_hold_timer);
    // 释放中断
    if (irq_number >= 0) {
        free_irq(irq_number, NULL);
    }
    // 释放GPIO
    if (gpio_is_valid(SENSOR_GPIO)) {
        gpio_free(SENSOR_GPIO);
    }
    
    printk("HC-SR501 deinitialized\n");
}
