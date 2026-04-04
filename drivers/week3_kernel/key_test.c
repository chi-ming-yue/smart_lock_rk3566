#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#define KEY_IOCTL_SCAN _IO('k', 1)
#define KEY_IOCTL_CLEAR _IO('k', 2)
#define KEY_IOCTL_GET_KEY _IOR('k', 3, char)

int main() {
    int fd;
    char choice;
    struct pollfd fds[1];
    
    fd = open("/dev/test", O_RDWR);
    if (fd < 0) {
        printf("Failed to open device: %s\n", strerror(errno));
        return -1;
    }
    
    printf("=== Key Matrix Continuous Scanner ===\n");
    printf("Press keys on the matrix. Press 'q' to exit.\n");
    printf("Top row ignored; active layout: 1 2 3 confirm / 4 5 6 0 / 7 8 9 delete\n");
    printf("LED will turn ON when correct password (2580) is entered.\n\n");
    
    // 设置非阻塞IO
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    
    while (1) {
        char key = 0;

        // 执行一次按键扫描
        if (ioctl(fd, KEY_IOCTL_GET_KEY, &key) == 0) {
            if (key != 0) {
                printf("Key detected: %c\n", key);
                ioctl(fd, KEY_IOCTL_SCAN);
            }
            usleep(100000); // 100ms
        } else {
            printf("Scan failed: %s\n", strerror(errno));
            break;
        }
        
        // 检查用户是否想退出（非阻塞）
        if (read(STDIN_FILENO, &choice, 1) > 0) {
            if (choice == 'q' || choice == 'Q') {
                printf("Exiting...\n");
                break;
            }
        }
    }
    
    close(fd);
    return 0;
}
