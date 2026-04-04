#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/string.h>
#include "key.h"
#include "oled.h"

char correct_password[PASSWORD_LENGTH] = {'2', '5', '8', '0'};

static const char kDefaultPassword[PASSWORD_LENGTH] = {'2', '5', '8', '0'};
static const char kResetSequence[] = "ABCD";

static const char key_map[KEY_ROWS][KEY_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static int row_gpios[KEY_ROWS] = {ROW1_GPIO, ROW2_GPIO, ROW3_GPIO, ROW4_GPIO};
static int col_gpios[KEY_COLS] = {COL1_GPIO, COL2_GPIO, COL3_GPIO, COL4_GPIO};

static char input_buffer[PASSWORD_LENGTH + 1];
static int input_index = 0;
static int reset_index = 0;
static int change_password_mode = 0;

extern void led_set_state(int state);

static int key_enable_col_pullup(unsigned gpio)
{
    unsigned long config = pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1);
    int ret = pinctrl_gpio_set_config(gpio, config);

    if (ret == -ENOTSUPP) {
        printk(KERN_WARNING "Col GPIO %u pull-up not supported by pinctrl, continuing without bias\n",
               gpio);
        return 0;
    }

    return ret;
}

struct key_debug_sample key_scan_debug(void)
{
    int row, col, i;
    struct key_debug_sample sample = {0, -1, -1};

    for (row = 0; row < KEY_ROWS; row++) {
        for (i = 0; i < KEY_ROWS; i++) {
            gpio_set_value(row_gpios[i], i == row ? 0 : 1);
        }

        udelay(1000);

        for (col = 0; col < KEY_COLS; col++) {
            if (gpio_get_value(col_gpios[col]) == 0) {
                msleep(10);
                if (gpio_get_value(col_gpios[col]) == 0) {
                    sample.key = key_map[row][col];
                    sample.row = row;
                    sample.col = col;
                    goto found;
                }
            }
        }
    }

found:
    for (i = 0; i < KEY_ROWS; i++) {
        gpio_set_value(row_gpios[i], 1);
    }

    return sample;
}

char key_scan(void)
{
    struct key_debug_sample sample = key_scan_debug();

    if (sample.row >= 0 && sample.col >= 0) {
        if (sample.key == 0) {
            printk(KERN_INFO "Ignored key at [row=%d, col=%d]\n", sample.row, sample.col);
        } else {
            printk(KERN_INFO "Key pressed at [row=%d, col=%d]: %c\n",
                   sample.row, sample.col, sample.key);
        }
    }

    return sample.key;
}

void key_process_input(char key)
{
    if (key == 0)
        return;

    if (key >= 'A' && key <= 'D') {
        if (key == kResetSequence[reset_index]) {
            reset_index++;
            printk(KERN_INFO "Reset sequence progress: %d/%zu\n",
                   reset_index, sizeof(kResetSequence) - 1);
            if (reset_index == (int)(sizeof(kResetSequence) - 1)) {
                change_password_mode = 1;
                reset_index = 0;
                key_clear_input();
                led_set_state(0);
                printk(KERN_INFO "Reset sequence matched, enter change-password mode\n");
            }
        } else {
            reset_index = (key == kResetSequence[0]) ? 1 : 0;
            printk(KERN_INFO "Reset sequence interrupted by %c, progress=%d/%zu\n",
                   key, reset_index, sizeof(kResetSequence) - 1);
        }
        return;
    }

    reset_index = 0;

    if (key == '*') {
        if (input_index > 0) {
            input_buffer[--input_index] = '\0';
            printk(KERN_INFO "%s delete pressed, input now: %.*s (index=%d)\n",
                   change_password_mode ? "Change-password mode" : "Password",
                   input_index, input_buffer, input_index);
        } else {
            printk(KERN_INFO "%s delete pressed, input buffer already empty\n",
                   change_password_mode ? "Change-password mode" : "Password");
        }
        return;
    }

    if (key == '#') {
        if (change_password_mode) {
            if (input_index == PASSWORD_LENGTH) {
                memcpy(correct_password, input_buffer, PASSWORD_LENGTH);
                printk(KERN_INFO "New password saved: %c%c%c%c\n",
                       correct_password[0], correct_password[1], correct_password[2], correct_password[3]);
                change_password_mode = 0;
                oled_update_password_status("CHANGED");
            } else {
                printk(KERN_INFO "Change-password mode requires %d digits, current=%d\n",
                       PASSWORD_LENGTH, input_index);
                oled_update_password_status("BAD LEN");
            }
            key_clear_input();
            return;
        }

        if (input_index > 0) {
            printk(KERN_INFO "Confirm pressed, checking password: %.*s\n",
                   input_index, input_buffer);
            if (input_index == PASSWORD_LENGTH && key_check_password(input_buffer)) {
                printk(KERN_INFO "Password correct! Turning LED ON\n");
                led_set_state(1);
                oled_update_password_status("CORRECT");
            } else {
                printk(KERN_INFO "Password incorrect!\n");
                led_set_state(0);
                oled_update_password_status("WRONG");
            }
        }
        key_clear_input();
        return;
    }

    if (key >= '0' && key <= '9' && input_index < PASSWORD_LENGTH) {
        input_buffer[input_index++] = key;
        printk(KERN_INFO "%s input: %.*s (index=%d)\n",
               change_password_mode ? "New password" : "Password",
               input_index, input_buffer, input_index);
        oled_update_password_status(change_password_mode ? "SETTING" : "INPUT");
    } else if (key >= '0' && key <= '9') {
        printk(KERN_INFO "%s buffer full (index=%d), press # to %s\n",
               change_password_mode ? "New password" : "Input",
               input_index,
               change_password_mode ? "save" : "confirm");
    } else {
        printk(KERN_INFO "Ignoring unsupported key: %c\n", key);
    }
}

int key_check_password(const char *input)
{
    return memcmp(input, correct_password, PASSWORD_LENGTH) == 0;
}

void key_clear_input(void)
{
    memset(input_buffer, 0, sizeof(input_buffer));
    input_index = 0;
    printk(KERN_INFO "Input buffer cleared: index=%d\n", input_index);
}

int key_matrix_init(void)
{
    int ret, i;
    int init_val;

    printk(KERN_INFO "=== Initializing 4x4 Key Matrix ===\n");
    printk(KERN_INFO "Rows: GPIO%d, %d, %d, %d\n",
           row_gpios[0], row_gpios[1], row_gpios[2], row_gpios[3]);
    printk(KERN_INFO "Cols: GPIO%d, %d, %d, %d\n",
           col_gpios[0], col_gpios[1], col_gpios[2], col_gpios[3]);

    for (i = 0; i < KEY_ROWS; i++) {
        ret = gpio_request(row_gpios[i], "key_row");
        if (ret) {
            printk(KERN_ERR "Failed to request row GPIO %d: %d\n", row_gpios[i], ret);
            goto error_rows;
        }
        ret = gpio_direction_output(row_gpios[i], 1);
        if (ret) {
            printk(KERN_ERR "Failed to set row GPIO %d as output: %d\n", row_gpios[i], ret);
            goto error_rows;
        }
        printk(KERN_INFO "Row GPIO %d initialized as output, value=%d\n",
               row_gpios[i], gpio_get_value(row_gpios[i]));
    }

    for (i = 0; i < KEY_COLS; i++) {
        ret = gpio_request(col_gpios[i], "key_col");
        if (ret) {
            printk(KERN_ERR "Failed to request col GPIO %d: %d\n", col_gpios[i], ret);
            goto error_cols;
        }
        ret = gpio_direction_input(col_gpios[i]);
        if (ret) {
            printk(KERN_ERR "Failed to set col GPIO %d as input: %d\n", col_gpios[i], ret);
            goto error_cols;
        }
        ret = key_enable_col_pullup(col_gpios[i]);
        if (ret) {
            printk(KERN_ERR "Failed to enable pull-up on col GPIO %d: %d\n", col_gpios[i], ret);
            goto error_cols;
        }
        init_val = gpio_get_value(col_gpios[i]);
        printk(KERN_INFO "Col GPIO %d initialized as input pull-up, initial value: %d\n",
               col_gpios[i], init_val);
    }

    key_clear_input();
    printk(KERN_INFO "Key matrix initialization complete\n");
    printk(KERN_INFO "Correct password: %c%c%c%c\n",
           correct_password[0], correct_password[1],
           correct_password[2], correct_password[3]);
    printk(KERN_INFO "=== GPIO Status Check ===\n");

    for (i = 0; i < KEY_ROWS; i++) {
        printk(KERN_INFO "Row %d (GPIO%d): value=%d\n",
               i, row_gpios[i], gpio_get_value(row_gpios[i]));
    }
    for (i = 0; i < KEY_COLS; i++) {
        printk(KERN_INFO "Col %d (GPIO%d): value=%d\n",
               i, col_gpios[i], gpio_get_value(col_gpios[i]));
    }

    return 0;

error_cols:
    for (; i >= 0; i--) {
        gpio_free(col_gpios[i]);
    }
    i = KEY_ROWS;
error_rows:
    for (; i >= 0; i--) {
        gpio_free(row_gpios[i]);
    }
    return ret;
}

void key_matrix_deinit(void)
{
    int i;

    printk(KERN_INFO "Deinitializing key matrix...\n");

    for (i = 0; i < KEY_COLS; i++) {
        if (gpio_is_valid(col_gpios[i])) {
            gpio_free(col_gpios[i]);
            printk(KERN_INFO "Col GPIO %d freed\n", col_gpios[i]);
        }
    }

    for (i = 0; i < KEY_ROWS; i++) {
        if (gpio_is_valid(row_gpios[i])) {
            gpio_set_value(row_gpios[i], 1);
            gpio_free(row_gpios[i]);
            printk(KERN_INFO "Row GPIO %d freed\n", row_gpios[i]);
        }
    }

    printk(KERN_INFO "Key matrix deinitialized\n");
}
