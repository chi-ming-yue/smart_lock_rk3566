#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    keep_running = 0;
}

static void print_timestamp(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buffer[32];

    if (tm_info == NULL) {
        printf("[time-error]");
        return;
    }

    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    printf("[%s]", buffer);
}

int main(int argc, char *argv[])
{
    const char *device_path = "/dev/test";
    int poll_ms = 200;
    char last_state = '\0';

    if (argc > 1) {
        poll_ms = atoi(argv[1]);
        if (poll_ms <= 0) {
            poll_ms = 200;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("传感器测试程序已启动\n");
    printf("设备节点: %s\n", device_path);
    printf("轮询间隔: %d ms\n", poll_ms);
    printf("状态说明: 0=无人, 1=检测到人, Ctrl+C 退出\n");

    while (keep_running) {
        int fd;
        char state = '0';
        ssize_t ret;

        fd = open(device_path, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "打开设备失败: %s, 错误: %s\n", device_path, strerror(errno));
            break;
        }

        ret = read(fd, &state, 1);
        close(fd);
        if (ret < 0) {
            fprintf(stderr, "读取传感器状态失败: %s\n", strerror(errno));
            break;
        }
        if (ret == 1 && state != last_state) {
            print_timestamp();
            if (state == '1') {
                printf(" 检测到人，传感器触发成功\n");
            } else {
                printf(" 当前无人，传感器恢复空闲状态\n");
            }
            fflush(stdout);
            last_state = state;
        }

        usleep((useconds_t)poll_ms * 1000);
    }

    printf("传感器测试程序结束\n");
    return 0;
}
