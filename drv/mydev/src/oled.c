#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "oled.h"

// SSD1306命令定义
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D

static struct i2c_client *oled_client = NULL;
static u8 oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

// 简化字体数据
static const u8 simple_font[][5] = {
    // 数字 0-9
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    
    // 字母 A-Z
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    
    // 空格和其他字符
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 空格
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 未定义字符用空格
};

// 发送命令到OLED
static int oled_command(u8 cmd) {
    u8 buf[2] = {0x00, cmd};
    if (oled_client) {
        int ret = i2c_master_send(oled_client, buf, 2);
        return (ret == 2) ? 0 : -1;
    }
    return -1;
}

// 发送数据到OLED
static void oled_data(const u8 *data, size_t len) {
    u8 *buf;
    
    if (!oled_client || !data) 
        return;
    
    buf = kmalloc(len + 1, GFP_KERNEL);
    if (!buf) 
        return;
    
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    
    i2c_master_send(oled_client, buf, len + 1);
    kfree(buf);
}

// 绘制像素点 - 修正页面计算
static void oled_draw_pixel(int x, int y, int color) {
    int page, bit;
    
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
        return;
    
    // 计算页面和位位置
    page = y / 8;      // 每页8行
    bit = y % 8;       // 在页面中的位位置
    
    if (color)
        oled_buffer[x + page * OLED_WIDTH] |= (1 << bit);
    else
        oled_buffer[x + page * OLED_WIDTH] &= ~(1 << bit);
}

// 绘制字符 - 修正Y坐标计算
void oled_draw_string(int x, int y, const char *str) {
    int start_x = x;
    int char_idx;
    int i, j;
    u8 col;
    
    // 将Y坐标转换为像素行 (每行8像素)
    int pixel_y = y * 8;
    
    while (*str && x < OLED_WIDTH - 5) {
        // 空格字符
        if (*str == ' ') {
            x += 6;
            str++;
            continue;
        }
        
        // 获取字符索引
        if (*str >= '0' && *str <= '9') {
            char_idx = *str - '0';
        } else if (*str >= 'A' && *str <= 'Z') {
            char_idx = 10 + (*str - 'A');
        } else if (*str >= 'a' && *str <= 'z') {
            char_idx = 10 + (*str - 'a');
        } else {
            char_idx = 36; // 默认显示空格
        }
        
        // 确保索引在范围内
        if (char_idx >= sizeof(simple_font) / sizeof(simple_font[0])) {
            char_idx = 36;
        }
        
        // 绘制字符的5列
        for (i = 0; i < 5; i++) {
            col = simple_font[char_idx][i];
            for (j = 0; j < 7; j++) {
                if (col & (1 << j)) {
                    oled_draw_pixel(x + i, pixel_y + j, 1);
                }
            }
        }
        
        x += 6;
        str++;
        
        // 换行处理
        if (x >= OLED_WIDTH - 5 && *str) {
            x = start_x;
            pixel_y += 8; // 下一行
        }
    }
}

// 清屏
void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

// 更新显示 - 修正页面设置
void oled_display(void) {
    // 设置列地址范围 (0-127)
    oled_command(0x21);
    oled_command(0x00);
    oled_command(0x7F);
    
    // 设置页地址范围 (0-7)
    oled_command(0x22);
    oled_command(0x00);
    oled_command(0x07);
    
    // 发送显示数据
    oled_data(oled_buffer, sizeof(oled_buffer));
}

int oled_init(void) {
    struct i2c_adapter *adapter = NULL;
    struct i2c_board_info board_info = {
        .type = "ssd1306",
        .addr = OLED_I2C_ADDR,
    };
    
    printk(KERN_INFO "OLED: 初始化开始 (I2C-2, 0x3C)\n");
    
    // 获取I2C-2适配器
    adapter = i2c_get_adapter(2);
    if (!adapter) {
        printk(KERN_ERR "OLED: 无法获取I2C-2适配器\n");
        return -ENODEV;
    }
    
    // 创建I2C设备
    oled_client = i2c_new_device(adapter, &board_info);
    if (!oled_client) {
        printk(KERN_ERR "OLED: 在I2C-2上创建设备失败\n");
        i2c_put_adapter(adapter);
        return -ENODEV;
    }
    
    i2c_put_adapter(adapter);
    
    printk(KERN_INFO "OLED: 设备创建成功，开始初始化序列\n");
    
    // 等待硬件稳定
    mdelay(100);
    
    // 正确的SSD1306初始化序列
    oled_command(0xAE); // Display OFF
    mdelay(10);
    
    oled_command(0x20); // Set Memory Addressing Mode
    oled_command(0x00); // Horizontal Addressing Mode
    
    oled_command(0x21); // Set column address range
    oled_command(0x00); // Column start address = 0
    oled_command(0x7F); // Column end address = 127
    
    oled_command(0x22); // Set page address range
    oled_command(0x00); // Page start address = 0
    oled_command(0x07); // Page end address = 7 (64/8 - 1)
    
    oled_command(0x40); // Set display start line to 0
    
    oled_command(0x81); // Set contrast control
    oled_command(0x7F); // Contrast value
    
    oled_command(0xA1); // Set segment re-map (0xA0 for normal, 0xA1 for flipped)
    oled_command(0xA6); // Set normal display (not inverted)
    
    oled_command(0xA8); // Set multiplex ratio
    oled_command(0x3F); // 1/64 duty (for 64px height)
    
    oled_command(0xC8); // Set COM Output Scan Direction (0xC0 for normal, 0xC8 for flipped)
    
    oled_command(0xD3); // Set display offset
    oled_command(0x00); // No offset
    
    oled_command(0xD5); // Set display clock divide ratio/oscillator frequency
    oled_command(0x80); // Set divide ratio
    
    oled_command(0xD9); // Set pre-charge period
    oled_command(0xF1); // 
    
    oled_command(0xDA); // Set com pins hardware configuration
    oled_command(0x12); // Alternative COM pin configuration, disable COM left/right remap
    
    oled_command(0xDB); // Set vcomh
    oled_command(0x40); // 
    
    oled_command(0x8D); // Set DC-DC enable
    oled_command(0x14); // Enable charge pump
    
    oled_command(0xAF); // Display ON
    mdelay(100);
    
    // 清屏
    oled_clear();
    oled_display();
    oled_draw_string(0, 0, "Line 1: OLED OK");  // 第0-7像素行
    oled_draw_string(0, 1, "Line 2: I2C-2");    // 第8-15像素行  
    oled_draw_string(0, 2, "Line 3: Addr 0x3C"); // 第16-23像素行
    oled_draw_string(0, 3, "Line 4: RK3566");   // 第24-31像素行
    oled_draw_string(0, 4, "Line 5: Test");     // 第32-39像素行
    oled_draw_string(0, 5, "Line 6: Display");  // 第40-47像素行
    oled_draw_string(0, 6, "Line 7: Working");  // 第48-55像素行
    oled_draw_string(0, 7, "Line 8: End");      // 第56-63像素行
    oled_display();
    printk(KERN_INFO "OLED: 初始化完成\n");
    return 0;
}

void oled_deinit(void) {
    if (oled_client) {
        oled_command(SSD1306_DISPLAYOFF);
        i2c_unregister_device(oled_client);
        oled_client = NULL;
    }
    printk(KERN_INFO "OLED: 已卸载\n");
}

// 更新传感器状态显示 - 修正Y坐标
void oled_update_sensor_status(char sensor_state) {
    int i, j;
    
    // 清除状态区域 (第2行，像素行16-23)
    for (i = 0; i < OLED_WIDTH; i++) {
        for (j = 16; j < 24; j++) {
            oled_draw_pixel(i, j, 0);
        }
    }
    
    // 更新传感器状态 (第2行)
    oled_draw_string(0, 2, "Sensor: ");
    if (sensor_state == '1') {
        oled_draw_string(56, 2, "DETECT");
    } else {
        oled_draw_string(56, 2, "IDLE  ");
    }
    
    oled_display();
}

// 更新LED状态显示 - 修正Y坐标
void oled_update_led_status(int led_state) {
    int i, j;
    
    // 清除LED状态区域 (第4行，像素行32-39)
    for (i = 0; i < OLED_WIDTH; i++) {
        for (j = 32; j < 40; j++) {
            oled_draw_pixel(i, j, 0);
        }
    }
    
    // 更新LED状态 (第4行)
    oled_draw_string(0, 4, "LED: ");
    if (led_state) {
        oled_draw_string(40, 4, "ON ");
    } else {
        oled_draw_string(40, 4, "OFF");
    }
    
    oled_display();
}

void oled_update_face_result(const char *name, int score_percent) {
    char line1[22];
    char line2[22];
    int i, j;

    if (name == NULL || *name == '\0') {
        name = "unknown";
    }
    if (score_percent < 0) {
        score_percent = 0;
    }
    if (score_percent > 100) {
        score_percent = 100;
    }

    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));
    snprintf(line1, sizeof(line1), "Face:%s", name);
    snprintf(line2, sizeof(line2), "Sim:%d%%", score_percent);

    for (i = 0; i < OLED_WIDTH; i++) {
        for (j = 48; j < 64; j++) {
            oled_draw_pixel(i, j, 0);
        }
    }

    oled_draw_string(0, 6, line1);
    oled_draw_string(0, 7, line2);
    oled_display();
}

void oled_update_password_status(const char *status) {
    int i, j;

    if (status == NULL || *status == '\0') {
        status = "IDLE";
    }

    for (i = 0; i < OLED_WIDTH; i++) {
        for (j = 24; j < 32; j++) {
            oled_draw_pixel(i, j, 0);
        }
    }

    oled_draw_string(0, 3, "PWD: ");
    oled_draw_string(40, 3, status);
    oled_display();
}

void oled_update_servo_status(const char *status) {
    int i, j;

    if (status == NULL || *status == '\0') {
        status = "LOCK";
    }

    for (i = 0; i < OLED_WIDTH; i++) {
        for (j = 40; j < 48; j++) {
            oled_draw_pixel(i, j, 0);
        }
    }

    oled_draw_string(0, 5, "SERVO:");
    oled_draw_string(48, 5, status);
    oled_display();
}
