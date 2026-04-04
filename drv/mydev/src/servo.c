#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "oled.h"
#include "servo.h"

#define SERVO_DEFAULT_GPIO 116
#define SERVO_DEFAULT_PERIOD_US 20000
#define SERVO_DEFAULT_HOLD_FRAMES 40

static int servo_gpio = SERVO_DEFAULT_GPIO;
static int period_us = SERVO_DEFAULT_PERIOD_US;
static int min_pulse_us = 500;
static int mid_pulse_us = 1500;
static int max_pulse_us = 2500;
static int hold_frames = SERVO_DEFAULT_HOLD_FRAMES;
static int servo_autostart = 0;
static int lock_pulse_us = 1500;
static int unlock_pulse_us = 500;

module_param(servo_gpio, int, 0644);
MODULE_PARM_DESC(servo_gpio, "GPIO number for the servo signal line");
module_param(period_us, int, 0644);
MODULE_PARM_DESC(period_us, "Servo PWM period in microseconds");
module_param(min_pulse_us, int, 0644);
MODULE_PARM_DESC(min_pulse_us, "Pulse width for one end stop in microseconds");
module_param(mid_pulse_us, int, 0644);
MODULE_PARM_DESC(mid_pulse_us, "Pulse width for the center position in microseconds");
module_param(max_pulse_us, int, 0644);
MODULE_PARM_DESC(max_pulse_us, "Pulse width for the other end stop in microseconds");
module_param(hold_frames, int, 0644);
MODULE_PARM_DESC(hold_frames, "How many PWM periods to hold each position before moving");
module_param(servo_autostart, int, 0644);
MODULE_PARM_DESC(servo_autostart, "Start servo sweep automatically when mydev loads");
module_param(lock_pulse_us, int, 0644);
MODULE_PARM_DESC(lock_pulse_us, "Pulse width for the lock position in microseconds");
module_param(unlock_pulse_us, int, 0644);
MODULE_PARM_DESC(unlock_pulse_us, "Pulse width for the unlock position in microseconds");

static struct task_struct *servo_task;
static int servo_initialized;

static int servo_emit_frame(int pulse_width_us)
{
    int low_time_us;

    if (!servo_initialized)
        return -ENODEV;

    if (pulse_width_us <= 0 || pulse_width_us >= period_us)
        return -EINVAL;

    low_time_us = period_us - pulse_width_us;
    gpio_set_value(servo_gpio, 1);
    usleep_range(pulse_width_us, pulse_width_us + 100);
    gpio_set_value(servo_gpio, 0);
    usleep_range(low_time_us, low_time_us + 100);
    return 0;
}

static int servo_hold_position(int pulse_width_us, int frames)
{
    int i;
    for (i = 0; i < frames; ++i) {
        int ret = servo_emit_frame(pulse_width_us);
        if (ret)
            return ret;
    }
    return 0;
}

static int servo_thread_fn(void *unused)
{
    int frame_count = (3000 * 1000) / period_us;

    if (frame_count <= 0)
        frame_count = 150;

    pr_info("mydev servo: unlock once gpio=%d period=%dus lock=%dus unlock=%dus hold_frames=%d\n",
            servo_gpio, period_us, lock_pulse_us, unlock_pulse_us, frame_count);

    oled_update_servo_status("OPEN");
    servo_hold_position(unlock_pulse_us, frame_count);
    oled_update_servo_status("CLOSE");
    servo_hold_position(lock_pulse_us, hold_frames);
    servo_task = NULL;
    gpio_set_value(servo_gpio, 0);
    return 0;
}

int servo_init(void)
{
    int ret;

    if (servo_initialized)
        return 0;
    if (hold_frames <= 0)
        hold_frames = SERVO_DEFAULT_HOLD_FRAMES;
    if (lock_pulse_us <= 0 || lock_pulse_us >= period_us)
        lock_pulse_us = mid_pulse_us;
    if (unlock_pulse_us <= 0 || unlock_pulse_us >= period_us)
        unlock_pulse_us = mid_pulse_us;

    ret = gpio_request(servo_gpio, "mydev_servo_gpio");
    if (ret) {
        pr_err("mydev servo: failed to request gpio %d: %d\n", servo_gpio, ret);
        return ret;
    }

    ret = gpio_direction_output(servo_gpio, 0);
    if (ret) {
        pr_err("mydev servo: failed to set gpio %d as output: %d\n", servo_gpio, ret);
        gpio_free(servo_gpio);
        return ret;
    }

    servo_initialized = 1;
    pr_info("mydev servo: ready on gpio %d (autostart=%d lock=%dus unlock=%dus)\n",
            servo_gpio, servo_autostart, lock_pulse_us, unlock_pulse_us);
    oled_update_servo_status("LOCK");

    if (servo_autostart)
        return servo_start_sweep();
    return servo_move_lock();
}

void servo_deinit(void)
{
    if (!servo_initialized)
        return;
    servo_stop_sweep();
    gpio_set_value(servo_gpio, 0);
    gpio_free(servo_gpio);
    servo_initialized = 0;
    pr_info("mydev servo: deinitialized\n");
}

int servo_start_sweep(void)
{
    if (!servo_initialized)
        return -ENODEV;
    if (servo_task)
        return 0;

    servo_task = kthread_run(servo_thread_fn, NULL, "mydev_servo");
    if (IS_ERR(servo_task)) {
        int ret = PTR_ERR(servo_task);
        servo_task = NULL;
        pr_err("mydev servo: failed to start thread: %d\n", ret);
        return ret;
    }
    return 0;
}

void servo_stop_sweep(void)
{
    if (servo_task) {
        kthread_stop(servo_task);
        servo_task = NULL;
        pr_info("mydev servo: unlock action stopped\n");
    }
    servo_move_lock();
}

int servo_move_center(void)
{
    servo_stop_sweep();
    pr_info("mydev servo: move center %dus\n", mid_pulse_us);
    return servo_hold_position(mid_pulse_us, hold_frames);
}

int servo_move_left(void)
{
    servo_stop_sweep();
    pr_info("mydev servo: move left %dus\n", min_pulse_us);
    return servo_hold_position(min_pulse_us, hold_frames);
}

int servo_move_right(void)
{
    servo_stop_sweep();
    pr_info("mydev servo: move right %dus\n", max_pulse_us);
    return servo_hold_position(max_pulse_us, hold_frames);
}

int servo_move_lock(void)
{
    if (!servo_initialized)
        return -ENODEV;
    pr_info("mydev servo: move lock %dus\n", lock_pulse_us);
    oled_update_servo_status("LOCK");
    return servo_hold_position(lock_pulse_us, hold_frames);
}

int servo_move_unlock(void)
{
    if (!servo_initialized)
        return -ENODEV;
    pr_info("mydev servo: move unlock %dus\n", unlock_pulse_us);
    oled_update_servo_status("OPEN");
    return servo_hold_position(unlock_pulse_us, hold_frames);
}
