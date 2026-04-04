#ifndef _KEY_H
#define _KEY_H

#include <linux/types.h>

// 按键矩阵配置
#define KEY_ROWS 4
#define KEY_COLS 4

// 4x4键盘矩阵，使用RK3566 GPIO编号
// 行: GPIO3_B0, GPIO3_C2, GPIO3_A4, GPIO3_A5
#define ROW1_GPIO 104
#define ROW2_GPIO 114
#define ROW3_GPIO 100
#define ROW4_GPIO 101

// 列: GPIO3_A1, GPIO3_A2, GPIO3_A3, GPIO4_C3
#define COL1_GPIO 97
#define COL2_GPIO 98
#define COL3_GPIO 99
#define COL4_GPIO 147
// 按键值定义
#define KEY_1 '1'
#define KEY_2 '2'
#define KEY_3 '3'
#define KEY_4 '4'
#define KEY_5 '5'
#define KEY_6 '6'
#define KEY_7 '7'
#define KEY_8 '8'
#define KEY_9 '9'
#define KEY_STAR '*'
#define KEY_0 '0'
#define KEY_POUND '#'

struct key_debug_sample {
    char key;
    int row;
    int col;
};

// 密码设置
#define PASSWORD_LENGTH 4
extern char correct_password[PASSWORD_LENGTH];

// 函数声明
int key_matrix_init(void);
void key_matrix_deinit(void);
char key_scan(void);
struct key_debug_sample key_scan_debug(void);
int key_check_password(const char *input);
void key_clear_input(void);
void key_process_input(char key);

#endif
