#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

struct key_debug_sample {
    char key;
    int row;
    int col;
};

#define KEY_IOCTL_GET_DEBUG _IOR('k', 4, struct key_debug_sample)

int main(void) {
    int fd = open("/dev/test", O_RDWR);
    if (fd < 0) {
        printf("Failed to open device: %s\n", strerror(errno));
        return 1;
    }

    printf("=== Key Matrix Raw Debug Scanner ===\n");
    printf("Shows raw row/col even when the key is ignored. Press Ctrl+C to exit.\n\n");

    while (1) {
        struct key_debug_sample sample = {0, -1, -1};
        if (ioctl(fd, KEY_IOCTL_GET_DEBUG, &sample) != 0) {
            printf("Debug scan failed: %s\n", strerror(errno));
            break;
        }

        if (sample.row >= 0 && sample.col >= 0) {
            if (sample.key == 0) {
                printf("raw row=%d col=%d key=ignored\n", sample.row, sample.col);
            } else {
                printf("raw row=%d col=%d key=%c\n", sample.row, sample.col, sample.key);
            }
        }

        usleep(100000);
    }

    close(fd);
    return 0;
}
