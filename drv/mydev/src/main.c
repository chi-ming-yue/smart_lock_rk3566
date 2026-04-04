#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>   
#include <linux/fs.h>       
#include <linux/uaccess.h> 
#include <linux/device.h> 
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <linux/string.h>




#include "key.h"
#include "sensor.h"
#include "led.h"
#include "oled.h"
#include "servo.h"

#define MY_NAME "chardev"

// 添加IOCTL命令定义
#define KEY_IOCTL_SCAN _IO('k', 1)
#define KEY_IOCTL_CLEAR _IO('k', 2)
#define KEY_IOCTL_GET_KEY _IOR('k', 3, char)
#define KEY_IOCTL_GET_DEBUG _IOR('k', 4, struct key_debug_sample)

int major = 0;
struct class *cls = NULL;
struct device *dev = NULL;

// 在文件操作结构体中添加ioctl
long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    char key;
    struct key_debug_sample sample;
    
    switch (cmd) {
    case KEY_IOCTL_SCAN:
        key = key_scan();
        if (key) {
            key_process_input(key);
            printk("Manual scan: key '%c' processed\n", key);
        }
        break;
        
    case KEY_IOCTL_CLEAR:
        key_clear_input();
        printk("Input buffer cleared\n");
        break;

    case KEY_IOCTL_GET_KEY:
        key = key_scan();
        if (copy_to_user((char __user *)arg, &key, 1)) {
            printk("copy key to user failed!\n");
            return -EFAULT;
        }
        break;

    case KEY_IOCTL_GET_DEBUG:
        sample = key_scan_debug();
        if (copy_to_user((struct key_debug_sample __user *)arg, &sample, sizeof(sample))) {
            printk("copy debug sample to user failed!\n");
            return -EFAULT;
        }
        break;
        
    default:
        return -ENOTTY;
    }
    
    return 0;
}

// 文件操作函数
int my_open(struct inode *inode, struct file *file)
{
    printk("open!\n");
    return 0;
}

ssize_t my_read(struct file *file, char __user *ubuf, size_t size, loff_t *offset)
{
    if (size < 1)
        return -EINVAL;

    if (*offset > 0)
        return 0;
    
    if(copy_to_user(ubuf, &sensor_state, 1))
    {
        printk("copy to user failed!\n");
        return -EIO;
    }
    
    // 更新OLED显示
    oled_update_sensor_status(sensor_state);
    *offset += 1;
    
    return 1;
}

ssize_t my_write(struct file *file, const char __user *ubuf, size_t size, loff_t *offset)
{
    char kbuf[128] = {0};
    if(size > sizeof(kbuf))
        size = sizeof(kbuf);
    if(copy_from_user(kbuf, ubuf, size))
    {
        printk("copy from user failed!\n");
        return -EIO;
    }   
    
    if (size >= 3 && kbuf[0] == 'O' && kbuf[1] == ':')
    {
        char *payload = kbuf + 2;
        char *sep = strchr(payload, '|');
        int score_percent = 0;
        if (sep != NULL) {
            *sep = '\0';
            kstrtoint(sep + 1, 10, &score_percent);
        }
        oled_update_face_result(payload, score_percent);
    }
    else if(kbuf[0] == '1')
    {
        printk("Turning LED ON\n");
        led_set_state(1);  // 使用LED控制函数
        oled_update_led_status(1);
    }
    else if(kbuf[0] == '0')
    {
        printk("Turning LED OFF\n");
        led_set_state(0);  // 使用LED控制函数
        oled_update_led_status(0);
    }
    else if (kbuf[0] == 'S')
    {
        printk("Moving servo to unlock position\n");
        servo_start_sweep();
    }
    else if (kbuf[0] == 'P')
    {
        printk("Returning servo to lock position\n");
        servo_stop_sweep();
    }
    else if (kbuf[0] == 'C')
    {
        printk("Moving servo to center\n");
        servo_move_center();
    }
    else if (kbuf[0] == 'L')
    {
        printk("Moving servo to left\n");
        servo_move_left();
    }
    else if (kbuf[0] == 'R')
    {
        printk("Moving servo to right\n");
        servo_move_right();
    }
    return size;
}

int my_release(struct inode *inode,struct file *file)
{
    printk("release!\n");
    return 0;
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .release = my_release,
    .unlocked_ioctl = my_ioctl,  // 添加这行
};

// 设备初始化
static int __init mydev_init(void)
{
    int ret;
    
    printk("mydev init\n");
    
    ret = led_init();
    if (ret < 0) {
        printk("GPIO init failed!\n");
        return ret;
    }
    
    ret = servo_init();
    if (ret < 0) {
        printk("Servo init failed!\n");
        servo_deinit();
        servo_deinit();
        led_deinit();
        return ret;
    }

    ret = hc_sr501_init();
    if (ret < 0) {
        printk("HC-SR501 init failed!\n");
        led_deinit();
        return ret;
    }
    
    ret = oled_init();
    if (ret < 0) {
        printk("OLED init failed!\n");
        hc_sr501_deinit();
        servo_deinit();
        led_deinit();
        return ret;
    }

    // 添加按键初始化
    ret = key_matrix_init();
    if (ret < 0) {
        printk("Key matrix init failed!\n");
        oled_deinit();
        hc_sr501_deinit();
        servo_deinit();
        led_deinit();
        return ret;
    }
    
    major = register_chrdev(0, MY_NAME, &fops);
    if (major < 0) 
    {
        printk("chrdev register failed!\n");
        oled_deinit();
        hc_sr501_deinit();
        servo_deinit();
        led_deinit();
        return -1;
    }

    cls = class_create(THIS_MODULE, "hi");
    if(IS_ERR(cls))
    {
        printk("class create failed!\n");
        unregister_chrdev(major, MY_NAME);
        oled_deinit();
        hc_sr501_deinit();
        servo_deinit();
        led_deinit();
        return PTR_ERR(cls);
    }
    
    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, "test");
    if(IS_ERR(dev))
    {
        printk("device create failed!\n");
        class_destroy(cls);
        unregister_chrdev(major, MY_NAME);
        oled_deinit();
        hc_sr501_deinit();
        servo_deinit();
        led_deinit();
        return PTR_ERR(dev);
    }
    
    printk("Device registered successfully with major: %d\n", major);
    return 0;
}

static void __exit mydev_exit(void)
{
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    key_matrix_deinit();
    oled_deinit();
    hc_sr501_deinit();
    servo_deinit();
    led_deinit();
    unregister_chrdev(major, MY_NAME);
    printk("mydev exit\n");
}

module_init(mydev_init);
module_exit(mydev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jj");
